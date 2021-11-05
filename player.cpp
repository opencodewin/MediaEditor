#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <application.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include <imgui_knob.h>
#include <iostream>
#include <sstream>
#include <fstream>
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#include <Lut3D.h>
#endif
#include "MediaPlayer.h"
#include "Log.h"
#include "GstToolkit.h"
#include "ImGuiToolkit.h"
#if defined(_WIN32)
#include <SDL.h>
#include <SDL_thread.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#endif

static std::string ini_file = "Media_Player.ini";
static std::string bookmark_path = "bookmark.ini";
static ImTextureID g_texture = 0;
MediaPlayer g_player;
static SDL_AudioDeviceID g_audio_dev = 0;
#if IMGUI_VULKAN_SHADER
ImGui::ColorConvert_vulkan * m_yuv2rgb {nullptr};
ImGui::LUT3D_vulkan *        m_lut3d {nullptr};
#endif

#define MAX_AUDIO_BUFFER_SIZE   65536
static uint8_t audio_buffer[MAX_AUDIO_BUFFER_SIZE];
static int audio_buffer_data_size = 0;

static size_t Resample_f32(const float *input, float *output, float speed, uint64_t inputSize, uint32_t channels, int max_len)
{
    if (input == NULL)
        return 0;
    size_t outputSize = (size_t) (inputSize / speed);
    outputSize -= outputSize % channels;
    if (outputSize > max_len / sizeof(float) / channels)
        outputSize = max_len / sizeof(float) / channels;
    if (output == NULL)
        return outputSize;
    float stepDist = speed;
    const uint32_t fixedFraction = (1LL << 16);
    const float normFixed = (1.0 / (1LL << 16));
    uint32_t step = ((uint32_t) (stepDist * fixedFraction + 0.5));
    uint32_t curOffset = 0;
    for (uint32_t i = 0; i < outputSize; i += 1)
    {
        for (uint32_t c = 0; c < channels; c += 1)
        {
            *output++ = (float) (input[c] + (input[c + channels] - input[c]) * (
                    (float) (curOffset >> 16) + ((curOffset & (fixedFraction - 1)) * normFixed)
            )
            );
        }
        curOffset += step;
        input += (curOffset >> 16) * channels;
        curOffset &= (fixedFraction - 1);
    }
    return outputSize;
}

static size_t Resample_s16(const int16_t *input, int16_t *output, float speed, size_t inputSize, uint32_t channels, int max_len) 
{
    if (input == NULL)
        return 0;
    size_t outputSize = (size_t) (inputSize / speed);
    outputSize -= outputSize % channels;
    if (outputSize > max_len / sizeof(int16_t) / channels)
        outputSize = max_len / sizeof(int16_t) / channels;
    if (output == NULL)
        return outputSize;
    float stepDist = speed;
    const uint32_t fixedFraction = (1LL << 16);
    const float normFixed = (1.0 / (1LL << 16));
    uint32_t step = ((uint32_t) (stepDist * fixedFraction + 0.5));
    uint32_t curOffset = 0;
    for (uint32_t i = 0; i < outputSize; i += 1)
    {
        for (uint32_t c = 0; c < channels; c += 1)
        {
            *output++ = (int16_t) (input[c] + (input[c + channels] - input[c]) * (
                    (float) (curOffset >> 16) + ((curOffset & (fixedFraction - 1)) * normFixed)
            )
            );
        }
        curOffset += step;
        input += (curOffset >> 16) * channels;
        curOffset &= (fixedFraction - 1);
    }
    return outputSize;
}

static void Reverse_f32(float *input, size_t inputSize, uint32_t channels)
{
    float * buffer = (float *)malloc(inputSize * channels * sizeof(float));
    float * input_data = input + (inputSize - 1) * channels ;
    float * output_data = buffer;
    for (int i = 0; i < inputSize; i++)
    {
        for (int c = 0; c < channels; c++)
        {
            *output_data++ = *input_data--; 
        }
    }
    memcpy(input, buffer, inputSize * channels * sizeof(float));
    free(buffer);
}

static void Reverse_s16(int16_t *input, size_t inputSize, uint32_t channels)
{
    int16_t * buffer = (int16_t *)malloc(inputSize * channels * sizeof(int16_t));
    int16_t * input_data = input + (inputSize - 1) * channels ;
    int16_t * output_data = buffer;
    for (int i = 0; i < inputSize; i++)
    {
        for (int c = 0; c < channels; c++)
        {
            *output_data++ = *input_data--; 
        }
    }
    memcpy(input, buffer, inputSize * channels * sizeof(int16_t));
    free(buffer);
}

#define SDL_AUDIO_MIN_BUFFER_SIZE 1024
static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    MediaPlayer *player = (MediaPlayer *)opaque;
    if (!player)
    {
        memset(stream, 0, len);
        return;
    }
    float speed = fabs(player->playSpeed());
    bool need_reverse = player->playSpeed() < 0;
    if (speed <= 0.05) speed = 0.1;
    int input_sample_rate = player->sample_rate();
    int output_sample_rate = input_sample_rate / speed;
    int wait_count = 0;
    if (len > 0)
    {
        if (audio_buffer_data_size < len)
        {
            while (audio_buffer_data_size < len)
            {
                auto mat = player->audioMat();
                if (!mat.empty())
                {
                    Uint8 * dst = audio_buffer + audio_buffer_data_size;
                    ImGui::ImMat packet_mat = mat.transpose();
#ifdef AUDIO_FORMAT_FLOAT
                    if (need_reverse)
                        Reverse_f32((float *)packet_mat.data, mat.w, mat.c);
                    auto size = Resample_f32((const float *)packet_mat.data, (float *)dst, speed, mat.w, mat.c, MAX_AUDIO_BUFFER_SIZE - audio_buffer_data_size);
                    audio_buffer_data_size += size * sizeof(float) * mat.c;
#else
                    if (need_reverse)
                        Reverse_s16((int16_t *)packet_mat.data, mat.w, mat.c);
                    auto size = Resample_s16((const int16_t *)packet_mat.data, (int16_t *)dst, speed, mat.w, mat.c, MAX_AUDIO_BUFFER_SIZE - audio_buffer_data_size);
                    audio_buffer_data_size += size * sizeof(int16_t) * mat.c;
#endif
                }
                else
                {
                    wait_count ++;
                    if (wait_count >= 4)
                    {
                        memset(audio_buffer + audio_buffer_data_size, 0, len - audio_buffer_data_size);
                        audio_buffer_data_size = len;
                    }
                }
            }
        }
        memcpy(stream, audio_buffer, len);
        memmove(audio_buffer, audio_buffer + len, len);
        audio_buffer_data_size -= len;
    }
}

static bool open_audio_device(int audio_sample_rate, int audio_channels, SDL_AudioFormat format)
{
    bool ret = false;
    if (g_audio_dev) { SDL_ClearQueuedAudio(g_audio_dev); SDL_CloseAudioDevice(g_audio_dev); g_audio_dev = 0; }
    SDL_AudioSpec wanted_spec, spec;
    wanted_spec.channels = audio_channels;
    wanted_spec.freq = audio_sample_rate;
    wanted_spec.format = format;
    wanted_spec.silence = 0;
    wanted_spec.samples = SDL_AUDIO_MIN_BUFFER_SIZE;
    wanted_spec.callback = sdl_audio_callback;
    wanted_spec.userdata = &g_player;
    g_audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (g_audio_dev) { SDL_PauseAudioDevice(g_audio_dev, 0); ret = true; }
    return ret;
}

// Application Framework Functions
void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "Media Player";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 1280;
    property.height = 720;
}

void Application_Initialize(void** handle)
{
    gst_init(nullptr, nullptr);
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = ini_file.c_str();
#ifdef USE_BOOKMARK
	// load bookmarks
	std::ifstream docFile(bookmark_path, std::ios::in);
	if (docFile.is_open())
	{
		std::stringstream strStream;
		strStream << docFile.rdbuf();//read the file
		ImGuiFileDialog::Instance()->DeserializeBookmarks(strStream.str());
		docFile.close();
	}
#endif
#if IMGUI_VULKAN_SHADER
    m_yuv2rgb = new ImGui::ColorConvert_vulkan(ImGui::get_default_gpu_index());
#endif

    memset(audio_buffer, 0, MAX_AUDIO_BUFFER_SIZE);
    audio_buffer_data_size = 0;

#if !IMGUI_APPLICATION_PLATFORM_SDL2
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER);
#endif
}

void Application_Finalize(void** handle)
{
#if IMGUI_VULKAN_SHADER
    if (m_yuv2rgb) { delete m_yuv2rgb; m_yuv2rgb = nullptr; }
    if (m_lut3d) { delete m_lut3d; m_lut3d = nullptr; }
#endif
    if (g_texture) { ImGui::ImDestroyTexture(g_texture); g_texture = nullptr; }
    if (g_player.isOpen()) g_player.close();
    if (g_audio_dev) { SDL_ClearQueuedAudio(g_audio_dev); SDL_CloseAudioDevice(g_audio_dev); g_audio_dev = 0; }
#ifdef USE_BOOKMARK
	// save bookmarks
	std::ofstream configFileWriter(bookmark_path, std::ios::out);
	if (!configFileWriter.bad())
	{
		configFileWriter << ImGuiFileDialog::Instance()->SerializeBookmarks();
		configFileWriter.close();
	}
#endif
    gst_deinit();
#if !IMGUI_APPLICATION_PLATFORM_SDL2
    SDL_Quit();
#endif
}

bool Application_Frame(void * handle)
{
    static bool show_ctrlbar = true;
    static bool show_log_window = false; 
    static bool force_software = false;
    static bool convert_hdr = false;
    static bool has_hdr = false;
    static bool muted = false;
    static bool full_screen = false;
    static double volume = 0;
    static int ctrlbar_hide_count = 0;
    bool done = false;
    auto& io = ImGui::GetIO();
    const ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    g_player.update();
    if (g_player.isOpen() && !g_audio_dev)
    {
        open_audio_device(
            g_player.sample_rate(), 
            g_player.channels(),
#ifdef AUDIO_FORMAT_FLOAT
            AUDIO_F32SYS
#else
            AUDIO_S16SYS
#endif
            );
    }

    if (g_player.isOpen() && g_player.isSeeking())
    {
        memset(audio_buffer, 0, MAX_AUDIO_BUFFER_SIZE);
        audio_buffer_data_size = 0;
    }

    // Show PlayControl panel
    if (g_player.isOpen() && (show_ctrlbar && io.FrameCountSinceLastInput))
    {
        ctrlbar_hide_count++;
        if (ctrlbar_hide_count >= 100)
        {
            ctrlbar_hide_count = 0;
            show_ctrlbar = false;
        }
    }
    if (io.FrameCountSinceLastInput == 0)
    {
        ctrlbar_hide_count = 0;
        show_ctrlbar = true;
    }

    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.85f);
    ImVec2 panel_size(io.DisplaySize.x - 40.0, 160);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5);
    if (show_ctrlbar && ImGui::Begin("Control", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        int i = ImGui::FindWindowByName("Control")->Size.x;
        ImGui::Indent((i - 32.0f) * 0.4f);
        ImVec2 size = ImVec2(32.0f, 32.0f); // Size of the image we want to make visible
        ImFont* font = ImGui::GetFont();
        float org_scale = font->Scale;
        // add open button
        ImGui::SetWindowFontScale(org_scale * 1.2);
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN, size))
        {
            //ImGui::ShowTooltipOnHover("Open Media File.");
            const char *filters = "视频文件(*.mp4 *.mov *.mkv *.avi){.mp4,.mov,.mkv,.avi,.MP4,.MOV,.MKV,.AVI},.*";
			ImGuiFileDialog::Instance()->OpenModal("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " 打开视频文件", filters, ".", 1, nullptr, ImGuiFileDialogFlags_ShowBookmark);
        }
        ImGui::ShowTooltipOnHover("Open Media File.");
        // add play button
        ImGui::SameLine();
        ImGui::SetWindowFontScale(org_scale * 1.5);
        if (ImGui::Button(g_player.isOpen() ? (g_player.isPlaying() ? ICON_FAD_PAUSE : ICON_FAD_PLAY) : ICON_FAD_PLAY, size))
        {
            if (g_player.isPlaying())
            {   g_player.play(false); if (g_audio_dev) SDL_PauseAudioDevice(g_audio_dev, 1);}
            else
            {   g_player.play(true); if (g_audio_dev) SDL_PauseAudioDevice(g_audio_dev, 0);}
        }
        ImGui::ShowTooltipOnHover("Toggle Play/Pause.");
        // add step next button
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA5_STEP_FORWARD, size))
        {
            if (g_player.isOpen()) g_player.step();
        }
        ImGui::ShowTooltipOnHover("Step Next Frame.");
        // add mute button
        ImGui::SameLine();
        if (ImGui::Button(g_player.isOpen() ? muted ? ICON_FA5_VOLUME_MUTE : ICON_FA5_VOLUME_UP : ICON_FA5_VOLUME_UP, size))
        {
            muted = !muted;
            if (muted)
            {   volume = g_player.volume(); g_player.set_volume(0); }
            else
            {   g_player.set_volume(volume); }
        }
        ImGui::ShowTooltipOnHover("Toggle Audio Mute.");
        // add about button
        ImGui::SameLine();
        if (ImGui::Button(ICON_FK_TAG, size))
        {
            ImGui::OpenPopup("##about", ImGuiPopupFlags_AnyPopup);
        }
        ImGui::ShowTooltipOnHover("Show About.");

        ImGui::SetWindowFontScale(org_scale);
        ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.4f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("##about", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("ImGUI Media Player(GStreamer)");
            ImGui::Separator();
            ImGui::Text("Dicky 2021");
            ImGui::Separator();
            int i = ImGui::GetCurrentWindow()->ContentSize.x;
            ImGui::Indent((i - 40.0f) * 0.5f);
            if (ImGui::Button("OK", ImVec2(40, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }

        ImGui::SameLine(); ImGui::Dummy(size);
        ImGui::SameLine(); ImGui::Dummy(size);
        ImGui::SameLine();
        ImGui::PushItemWidth(100);
        float play_speed = g_player.playSpeed();
        if (ImGui::SliderFloat("Speed", &play_speed, -2.f, 2.f, "%.1f", ImGuiSliderFlags_NoInput))
        {
            if (g_player.isOpen()) g_player.setPlaySpeed(play_speed);
        }
        ImGui::PopItemWidth();
        // add software decode button
        ImGui::SameLine();
        if (ImGui::ToggleButton("SW", &force_software, size * 0.75))
        {
            g_player.setSoftwareDecodingForced(force_software);
        }
        ImGui::ShowTooltipOnHover("Software decoder");
        // add HDR decode button
        ImGui::SameLine();
        if (ImGui::ToggleButton(has_hdr ? "HDR" : "SDR", &convert_hdr, size * 0.75))
        {
            if (!has_hdr) convert_hdr = false;
        }
        ImGui::ShowTooltipOnHover("HDR decoder");
        // add show log button
        ImGui::SameLine();
        ImGui::ToggleButton(ICON_FA5_LIST_UL, &show_log_window, size * 0.75);
        ImGui::ShowTooltipOnHover("Show Log");
        // add button end

        // show time info
        ImGui::SameLine(); ImGui::Dummy(size);
        ImGui::SameLine();
        ImGui::Text("%s/%s", GstToolkit::time_to_string(g_player.position()).c_str(), GstToolkit::time_to_string(g_player.duration()).c_str());

        ImGui::Unindent((i - 32.0f) * 0.4f);
        ImGui::Separator();

        float padding = style.FramePadding.x * 16;
        // add audio meter bar
        static int left_stack = 0;
        static int left_count = 0;
        static int right_stack = 0;
        static int right_count = 0;
        if (g_player.isOpen())
        {
            int l_level = g_player.audio_level(0);
            int r_level = g_player.audio_level(1);
            ImGui::UvMeter("##lhuvr", ImVec2(panel_size.x - padding, 10), &l_level, 0, 96, 200, &left_stack, &left_count);
            ImGui::UvMeter("##rhuvr", ImVec2(panel_size.x - padding, 10), &r_level, 0, 96, 200, &right_stack, &right_count);
        }
        else
        {
            int zero_channel_level = 0;
            ImGui::UvMeter("##lhuvr", ImVec2(panel_size.x - padding, 10), &zero_channel_level, 0, 96, 200);
            ImGui::UvMeter("##rhuvr", ImVec2(panel_size.x - padding, 10), &zero_channel_level, 0, 96, 200);
        }
        ImGui::Separator();
        auto timescale_width = io.DisplaySize.x - padding;
        if (g_player.isOpen())
        {
            Timeline *timeline = g_player.timeline();
            guint64 seek_t = g_player.position();
            if (ImGuiToolkit::TimelineSlider("##timeline", &seek_t, timeline->begin(), timeline->first(), timeline->end(), timeline->step(), timescale_width))
            {
                g_player.seek(seek_t);
            }
        }
        else
        {
            guint64 seek_t = 0;
            ImGuiToolkit::TimelineSlider("##timeline", &seek_t, 0, 0, 0, 0, timescale_width);
        }
        ImGui::Separator();

        // add buffer extent
        float v_extent = g_player.video_buffer_extent();
        ImGui::UvMeter("##video_extent", ImVec2(200, 6), &v_extent, 0, 1.0, 200); ImGui::ShowTooltipOnHover("VB extent:%.1f%%", v_extent * 100);
        float a_extent = g_player.audio_buffer_extent();
        ImGui::UvMeter("##audio_extent", ImVec2(200, 6), &a_extent, 0, 1.0, 200); ImGui::ShowTooltipOnHover("AB extent:%.1f%%", a_extent * 100);
        // add buffer extent end
        ImGui::End();
    }

    // handle key event
    if (g_player.isOpen() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space), false))
    {
        if (g_player.isPlaying())
        {   g_player.play(false); if (g_audio_dev) SDL_PauseAudioDevice(g_audio_dev, 1); }
        else 
        {   g_player.play(true); if (g_audio_dev) SDL_PauseAudioDevice(g_audio_dev, 0); }
    }
    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape), false))
    {
        if (g_player.isOpen()) g_player.close();
        done = true;
    }
    if (g_player.isOpen() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow), true))
    {
        auto g_pos = g_player.position();
        if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
        g_pos -= 5e+9;
        if (g_pos < 0) g_pos = 0;
        g_player.seek(g_pos);
    }
    if (g_player.isOpen() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow), true))
    {
        auto g_pos = g_player.position();
        if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
        g_pos += 5e+9;
        g_player.seek(g_pos);
    }
    if (g_player.isOpen() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow), true))
    {
        auto g_pos = g_player.position();
        if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
        g_pos -= 1e+10;
        if (g_pos < 0) g_pos = 0;
        g_player.seek(g_pos);
    }
    if (g_player.isOpen() && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow), true))
    {
        auto g_pos = g_player.position();
        if (g_pos == GST_CLOCK_TIME_NONE) g_pos = 0;
        g_pos += 1e+10;
        g_player.seek(g_pos);
    }
    if (g_player.isOpen() && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z), true))
    {
        g_player.step();
    }

    // handle screen double click event
    if (ImGui::IsMouseDoubleClicked(0) && !ImGui::IsAnyItemHovered())
    {
        full_screen = !full_screen;
        Application_FullScreen(full_screen);
    }

    // Message Boxes
    ImVec2 modal_center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 maxSize = ImVec2((float)io.DisplaySize.x, (float)io.DisplaySize.y);
	ImVec2 minSize = maxSize * 0.5f;
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
	{
        if (ImGuiFileDialog::Instance()->IsOk())
		{
            if (g_texture) { ImGui::ImDestroyTexture(g_texture); g_texture = nullptr; }
            if (g_audio_dev) { SDL_ClearQueuedAudio(g_audio_dev); SDL_CloseAudioDevice(g_audio_dev); g_audio_dev = 0; }
#if IMGUI_VULKAN_SHADER
            if (m_lut3d) { delete m_lut3d; m_lut3d = nullptr; }
            has_hdr = false;
            convert_hdr = false;
#endif
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_player.open(filePathName);
            g_player.play(true);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Show Log Window
    if (show_log_window)
    {
        Log::ShowLogWindow(&show_log_window);
    }

    // Video Texture Render
    if (g_player.isOpen())
    {
        ImGui::ImMat vmat = g_player.videoMat();
        if (!vmat.empty())
        {
#if IMGUI_VULKAN_SHADER
            int video_depth = vmat.type == IM_DT_INT8 ? 8 : vmat.type == IM_DT_INT16 ? 16 : 8;
            int video_shift = vmat.depth != 0 ? vmat.depth : vmat.type == IM_DT_INT8 ? 8 : vmat.type == IM_DT_INT16 ? 16 : 8;
#ifdef VIDEO_FORMAT_RGBA
            ImGui::ImGenerateOrUpdateTexture(g_texture, vmat.w, vmat.h, 4, (const unsigned char *)vmat.data);
#else
            ImGui::VkMat in_RGB; in_RGB.type = IM_DT_INT8;
            m_yuv2rgb->YUV2RGBA(vmat, in_RGB, vmat.color_format, vmat.color_space, vmat.color_range, video_depth, video_shift);
            if (vmat.color_space == IM_CS_BT2020) has_hdr = true; else has_hdr = false;
            int lut_mode = vmat.flags & IM_MAT_FLAGS_VIDEO_HDR_HLG ? HDRHLG_SDR709 : vmat.flags & IM_MAT_FLAGS_VIDEO_HDR_PQ ? HDRPQ_SDR709 : NO_DEFAULT;
            ImGui::VkMat im_RGB; im_RGB.type = IM_DT_INT8;
            if (has_hdr && convert_hdr && lut_mode != NO_DEFAULT)
            {
                // Convert HDR to SDR
                if (!m_lut3d)
                {
                    m_lut3d = new ImGui::LUT3D_vulkan(lut_mode, IM_INTERPOLATE_TRILINEAR, ImGui::get_default_gpu_index());
                }
                if (m_lut3d)
                {
                    m_lut3d->filter(in_RGB, im_RGB);
                }
                else
                    im_RGB = in_RGB;
            }
            else
                im_RGB = in_RGB;
            ImGui::ImGenerateOrUpdateTexture(g_texture, im_RGB.w, im_RGB.h, im_RGB.c, im_RGB.buffer_offset() , (const unsigned char *)im_RGB.buffer());
#endif
#else
#ifdef VIDEO_FORMAT_RGBA
            ImGui::ImGenerateOrUpdateTexture(g_texture, vmat.w, vmat.h, 4, (const unsigned char *)vmat.data);
#endif
#endif
        }
    }
    if (g_texture)
    {
        ImVec2 window_size = io.DisplaySize;
        bool bViewisLandscape = window_size.x >= window_size.y ? true : false;
        bool bRenderisLandscape = g_player.width() >= g_player.height() ? true : false;
        bool bNeedChangeScreenInfo = bViewisLandscape ^ bRenderisLandscape;
        float adj_w = bNeedChangeScreenInfo ? window_size.y : window_size.x;
        float adj_h = bNeedChangeScreenInfo ? window_size.x : window_size.y;
        float adj_x = adj_h * g_player.aspectRatio();
        float adj_y = adj_h;
        if (adj_x > adj_w) { adj_y *= adj_w / adj_x; adj_x = adj_w; }
        float offset_x = (window_size.x - adj_x) / 2.0;
        float offset_y = (window_size.y - adj_y) / 2.0;
        ImGui::GetBackgroundDrawList ()->AddImage (
            (void *)g_texture,
            ImVec2 (offset_x, offset_y),
            ImVec2 (offset_x + adj_x, offset_y + adj_y),
            ImVec2 (0, 0),
            ImVec2 (1, 1)
        );
    }
    return done;
}