#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_extra_widget.h>
#include <SDL.h>
#include <SDL_thread.h>

#define MAX_AUDIO_BUFFER    128
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

#define MAX(a,b) ((a) > (b) ? (a) : (b))
const uint8_t log2_tab[256]=
{
        0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

static inline int log2_c(unsigned int v)
{
    int n = 0;
    if (v & 0xffff0000) 
    {
        v >>= 16;
        n += 16;
    }
    if (v & 0xff00) 
    {
        v >>= 8;
        n += 8;
    }
    n += log2_tab[v];
    return n;
}

static void sdl_audio_callback(void *opaque, Uint8 *stream, int len);

#define NODE_VERSION    0x01000000

namespace BluePrint
{
// ======================================================= //
// ================ Rendering Node FFMPEG ================ //
// ======================================================= //
struct SDLAudioRenderNode final : Node
{
    BP_NODE_WITH_NAME(SDLAudioRenderNode, "Audio Render", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    SDLAudioRenderNode(BP* blueprint): Node(blueprint)
    {
        m_Name = "SDLAudioRender";
        m_HasCustomLayout = true;
#if !IMGUI_APPLICATION_PLATFORM_SDL2
        SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER);
#endif
    }

    ~SDLAudioRenderNode()
    {
        m_mutex.lock();
        for (auto mat : m_queue) { mat.release(); } m_queue.clear();
        m_mutex.unlock();
        for (auto data : m_channel_wave_data) { if (data) free(data); data = nullptr; }
        m_channel_wave_data.clear();
        SDL_CloseAudioDevice(m_audio_dev);
#if !IMGUI_APPLICATION_PLATFORM_SDL2
        SDL_Quit();
#endif
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        m_mutex.lock();
        for (auto mat : m_queue) { mat.release(); } m_queue.clear();
        m_mutex.unlock();
        m_buffer_available = true;
        m_BufferAvailable.SetValue(true);
        m_audio_callback_time = 0;
        m_current_pts = NAN;
        m_Tick.SetValue(m_current_pts);
        m_audio_sample_rate = 0;
        m_audio_channels = 0;
        m_audio_data_type = IM_DT_FLOAT32;
        m_channel_levels.clear();
        m_channel_stack.clear();
        m_channel_count.clear();
        for (auto data : m_channel_wave_data) { if (data) free(data); data = nullptr; }
        m_channel_wave_data.clear();
        SDL_ClearQueuedAudio(m_audio_dev);
        SDL_CloseAudioDevice(m_audio_dev);
        m_audio_dev = 0;
    }

    void OnPause(Context& context) override 
    { 
        if (m_audio_dev) SDL_PauseAudioDevice(m_audio_dev, 1);
        for ( int i = 0; i < m_channel_levels.size(); i++) m_channel_levels[i] = 0;;
    }

    void OnResume(Context& context) override { if (m_audio_dev) SDL_PauseAudioDevice(m_audio_dev, 0); }

    void OnStop(Context& context) override 
    { 
        if (m_audio_dev) { SDL_ClearQueuedAudio(m_audio_dev); SDL_PauseAudioDevice(m_audio_dev, 1); }
        for ( int i = 0; i < m_channel_levels.size(); i++) m_channel_levels[i] = 0;;
    }

    void OnStepNext(Context& context) override { if (m_audio_dev) SDL_PauseAudioDevice(m_audio_dev, 0); }

    void OnStepCurrent(Context& context) override { if (m_audio_dev) SDL_PauseAudioDevice(m_audio_dev, 0); }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        if (entryPoint.m_ID == m_Reset.m_ID)
        {
            Reset(context);
            return m_Exit;
        }
        else if (entryPoint.m_ID == m_Enter.m_ID)
        {
            m_mutex.lock();
            auto mat = context.GetPinValue<ImGui::ImMat>(m_Mat);
            if (!mat.empty())
            {
                m_audio_sample_rate = mat.rate.num;
                m_audio_channels = mat.c;
                m_audio_samples = mat.w;
                if (!m_audio_dev)
                {
                    SDL_AudioSpec wanted_spec, spec;
                    wanted_spec.channels = m_audio_channels;
                    wanted_spec.freq = m_audio_sample_rate;
                    wanted_spec.format = AUDIO_F32SYS;//mat.type == IM_DT_FLOAT32 ? AUDIO_F32SYS : mat.type == IM_DT_INT32 ? AUDIO_S32SYS : AUDIO_S16SYS;
                    wanted_spec.silence = 0;
                    wanted_spec.samples = MAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << log2_c(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC)); // mat.w ? 
                    wanted_spec.callback = sdl_audio_callback;
                    wanted_spec.userdata = this;
                    m_audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
                    if (m_audio_dev)
                        SDL_PauseAudioDevice(m_audio_dev, 0);
                    m_channel_levels.resize(m_audio_channels);
                    m_channel_stack.resize(m_audio_channels);
                    m_channel_count.resize(m_audio_channels);
                    for (auto data : m_channel_wave_data) { if (data) free(data); data = nullptr; }
                    m_channel_wave_data.clear();
                    for (int i = 0; i < m_audio_channels; i++)
                    {
                        float * data = (float *)malloc(m_audio_samples * sizeof(float));
                        m_channel_wave_data.push_back(data);
                    }
                    //m_audio_data_type = mat.type;
                }
                if (m_queue.size() < MAX_AUDIO_BUFFER)
                {
                    ImGui::ImMat data;
                    data = mat;
                    m_queue.push_back(data);
                }
                m_buffer_available = m_queue.size() < MAX_AUDIO_BUFFER;
                m_BufferAvailable.SetValue(m_buffer_available);
                mat.release();
            }
            m_mutex.unlock();
        }
        return {};
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key, bool embedded) override
    {
        int preview_width = 200;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 1.0));
        ImGui::PushItemWidth(preview_width);
        if (m_view_type == 0) ImGui::Dummy(ImVec2(preview_width, 20));
        for (int c = 0; c < m_audio_channels; c++)
        {
            char buf[17];
            if (m_view_type == 0)
            {
                snprintf(buf, sizeof(buf), "##huvr%d", c);
                if (c < m_channel_levels.size())
                {
                    ImGui::UvMeter(buf, ImVec2(preview_width, 15), &m_channel_levels[c], 0, 96, preview_width*2, &m_channel_stack[c], &m_channel_count[c]);
                }
            }
            else
            {
                snprintf(buf, sizeof(buf), "##wave%d", c);
                if ( c < m_channel_wave_data.size() && m_channel_wave_data[c])
                {
                    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 1.f,0.f, 1.f));
                    ImGui::PlotLinesEx(buf, m_channel_wave_data[c],  m_audio_samples, 0, buf, -0.5f, 0.5f, ImVec2(preview_width, 40), sizeof(float), false);
                    ImGui::PopStyleColor();
                }
            }
        }
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        return false;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        changed |= ImGui::RadioButton("Level", (int *)&m_view_type, 0); ImGui::SameLine();
        changed |= ImGui::RadioButton("Wave", (int *)&m_view_type, 1);
        return changed;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;
        if (value.contains("view_type"))
        {
            auto& val = value["view_type"];
            if (!val.is_number())
                return BP_ERR_NODE_LOAD;
            m_view_type = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["view_type"] = imgui_json::number(m_view_type);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Reset   = { this, "Reset" };
    MatPin    m_Mat     = { this, "Mat" };
    FlowPin   m_Exit    = { this, "Exit" };
    BoolPin   m_BufferAvailable = { this, "Buffer", true };
    DoublePin m_Tick    = { this, "Tick" };

    Pin* m_InputPins[3] = { &m_Enter, &m_Reset, &m_Mat };
    Pin* m_OutputPins[3] = { &m_Exit, &m_BufferAvailable, &m_Tick };

    SDL_AudioDeviceID m_audio_dev {0};
    int64_t m_audio_callback_time {0};
    double m_current_pts {NAN};
    bool m_buffer_available {true};
    int m_audio_channels {0};
    int m_audio_sample_rate {0};
    int m_audio_samples {0};
    int m_view_type {0}; // 0 = levels 1 = wave data
    ImDataType m_audio_data_type {IM_DT_FLOAT32};
    std::vector<int> m_channel_levels;
    std::vector<int> m_channel_stack;
    std::vector<int> m_channel_count;
    std::vector<float*> m_channel_wave_data;

    std::vector<ImGui::ImMat>   m_queue;
    std::mutex                  m_mutex;
};
} // namespace BluePrint

template<typename T>
static int calculate_audio_db(const T* data, int channels, int channel_index, size_t length, const float max_level) 
{
    static const float kMaxSquaredLevel = max_level * max_level;
    constexpr float kMinLevel = -96.f;
    float sum_square_ = 0;
    size_t sample_count_ = 0;
    for (size_t i = 0; i < length; i += channels) 
    {
        T audio_data = data[i + channel_index];
        sum_square_ += audio_data * audio_data;
    }
    sample_count_ += length / channels;
    float rms = sum_square_ / (sample_count_ * kMaxSquaredLevel);
    rms = 10 * log10(rms);
    if (rms < kMinLevel)
        rms = kMinLevel;
    rms = -kMinLevel + rms;
    return static_cast<int>(rms + 0.5);
}

static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    BluePrint::SDLAudioRenderNode * node = (BluePrint::SDLAudioRenderNode *)opaque;
    int audio_size, len1 = 0;
    node->m_audio_callback_time = ImGui::get_current_time_msec();
    node->m_mutex.lock();
    Uint8 * stream_data = stream;
    while (len > 0)
    {
        if (node->m_queue.size() > 0)
        {
            auto mat = node->m_queue.at(0);
            node->m_current_pts = mat.time_stamp;
            int data_size = node->m_audio_data_type == IM_DT_FLOAT32 || node->m_audio_data_type == IM_DT_INT32 ? 4 : node->m_audio_data_type == IM_DT_INT16 || node->m_audio_data_type == IM_DT_INT16_BE ? 2 : 1;
            for (int i = 0; i < mat.w; i++)
            {
                for (int c = 0; c < mat.c; c++)
                {
                    if (mat.type == IM_DT_FLOAT32)
                    {
                        if (c < node->m_channel_wave_data.size())
                        {
                            auto data = node->m_channel_wave_data[c];
                            data[i] = mat.at<float>(i, 0, c);
                            *(float *)stream_data = data[i];
                        }
                    }
                    else if (mat.type == IM_DT_INT32)
                    {
                        if (c < node->m_channel_wave_data.size())
                        {
                            auto data = node->m_channel_wave_data[c];
                            data[i] = mat.at<int32_t>(i, 0, c) / float(INT32_MAX);
                             *(float *)stream_data = data[i];
                        }
                    }
                    else if (mat.type == IM_DT_INT16)
                    {
                        if (c < node->m_channel_wave_data.size())
                        {
                            auto data = node->m_channel_wave_data[c];
                            data[i] = mat.at<int16_t>(i, 0, c) / float(INT16_MAX);
                             *(float *)stream_data = data[i];
                        }
                    }
                    else
                    {
                        if (c < node->m_channel_wave_data.size())
                        {
                            auto data = node->m_channel_wave_data[c];
                            data[i] = mat.at<Uint8>(i, 0, c) / float(INT8_MAX);
                             *(float *)stream_data = data[i];
                        }
                    }
                    stream_data += data_size;
                    len -= data_size;
                    len1 += data_size;
                    if (len <= 0)
                        break;
                }
                if (len <= 0)
                    break;
            }
            mat.release();
            node->m_queue.erase(node->m_queue.begin());
            node->m_buffer_available = node->m_queue.size() < MAX_AUDIO_BUFFER;
            node->m_BufferAvailable.SetValue(node->m_buffer_available);
            node->m_Tick.SetValue(node->m_current_pts);
        }
        else
        {
            memset(stream_data, 0, len);
            break;
        }
    }
    for (int c = 0; c < node->m_audio_channels; c++)
    {
        if (c < node->m_channel_levels.size())
        {
            if (node->m_audio_data_type == IM_DT_FLOAT32)
            {
                float * audio_data = (float *)stream;
                int sample_len = len1 / (sizeof(float) * node->m_audio_channels);
                node->m_channel_levels[c] = calculate_audio_db<float>(audio_data, node->m_audio_channels, c, sample_len, 1.0f);
            }
            else if (node->m_audio_data_type == IM_DT_INT32)
            {
                int32_t * audio_data = (int32_t *)stream;
                int sample_len = len1 / (sizeof(int32_t) * node->m_audio_channels);
                node->m_channel_levels[c] = calculate_audio_db<int32_t>(audio_data, node->m_audio_channels, c, sample_len, (float)(INT32_MAX));
            }
            else if (node->m_audio_data_type == IM_DT_INT16)
            {
                short * audio_data = (short *)stream;
                int sample_len = len1 / (sizeof(short) * node->m_audio_channels);
                node->m_channel_levels[c] = calculate_audio_db<short>(audio_data, node->m_audio_channels, c, sample_len, (float)(INT16_MAX));
            }
            else if (node->m_audio_data_type == IM_DT_INT8)
            {
                int8_t * audio_data = (int8_t *)stream;
                int sample_len = len1 / (sizeof(int8_t) * node->m_audio_channels);
                node->m_channel_levels[c] = calculate_audio_db<int8_t>(audio_data, node->m_audio_channels, c, sample_len, (float)(INT8_MAX));
            }
        }
    }
    node->m_mutex.unlock();
}

BP_NODE_DYNAMIC_WITH_NAME(SDLAudioRenderNode, "Audio Render", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
