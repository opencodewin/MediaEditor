#include <sstream>
#include "imgui.h"
#include <UI.h>
#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <imgui_helper.h>
#include <imgui_extra_widget.h>

#define NODE_VERSION    0x01000000

extern "C"
{
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavformat/avformat.h"
#include "libavutil/avstring.h"
#include "libavutil/pixdesc.h"
#include "libavutil/mathematics.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
#include "libavdevice/avdevice.h"
#include "libavcodec/avcodec.h"
};
#include <memory>

namespace BluePrint
{
struct AudioEqualizerNode final : Node
{
    BP_NODE_WITH_NAME(AudioEqualizerNode, "Audio Equalizer", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Audio")
    AudioEqualizerNode(BP* blueprint): Node(blueprint)
    {
        m_Name = "Audio Equalizer";
        m_HasCustomLayout = true;
        m_Skippable = true;
        memcpy(&m_bandCfg, &DEFAULT_BAND_CFG, sizeof(m_bandCfg));
        std::string errMsg;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        m_channels = -1;
#else
        m_chlyt.nb_channels = -1;
#endif
    }

    ~AudioEqualizerNode()
    {
        if (m_pDataConverter)
        {
            delete m_pDataConverter;
            m_pDataConverter = nullptr;
        }
        if (m_filterGraph)
        {
            avfilter_graph_free(&m_filterGraph);
            m_filterGraph = nullptr;
            m_inFilterCtx = nullptr;
            m_outFilterCtx = nullptr;
        }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    std::string GenerateEqFilterString()
    {
        std::ostringstream argstrOss;
        for (int i = 0; i < 10; i++)
        {
            argstrOss << "equalizer@" << i << "=f=" << m_bandCfg[i].centerFreq << ":t=h:w=" << m_bandCfg[i].bandWidth << ":g=" << m_bandCfg[i].gain;
            if (i < 9) argstrOss << ",";
        }
        // argstrOss << ",aformat=flt";
        return argstrOss.str();
    }

#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
    bool InitFFmpegFilterGraph(AVSampleFormat sampleFormat, int sampleRate, uint64_t channelLayout, std::string& err)
#else
    bool InitFFmpegFilterGraph(AVSampleFormat sampleFormat, int sampleRate, const AVChannelLayout& chlyt, std::string& err)
#endif
    {
        std::string argstr = GenerateEqFilterString();

        int fferr;
        AVFilterGraph* filterGraph      {nullptr};
        AVFilterContext* inFilterCtx    {nullptr};
        AVFilterContext* outFilterCtx   {nullptr};
        filterGraph = avfilter_graph_alloc();
        char abuffersrcArgs[256];
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        snprintf(abuffersrcArgs, sizeof(abuffersrcArgs), "sample_rate=%d:sample_fmt=%d:channel_layout=%llu:time_base=%d/%d",
                    sampleRate, sampleFormat, channelLayout, 1, sampleRate);
#else
        char chlytDescBuff[128] = {0};
        av_channel_layout_describe(&chlyt, chlytDescBuff, sizeof(chlytDescBuff));
        snprintf(abuffersrcArgs, sizeof(abuffersrcArgs), "sample_rate=%d:sample_fmt=%d:channel_layout=%s:time_base=%d/%d",
                    sampleRate, sampleFormat, chlytDescBuff, 1, sampleRate);
#endif

        const AVFilter *avFilter;
        avFilter = avfilter_get_by_name("abuffer");
        fferr = avfilter_graph_create_filter(&inFilterCtx, avFilter, "buffer_source", abuffersrcArgs, NULL, filterGraph);
        if (fferr < 0)
        {
            std::ostringstream oss;
            oss << "FAILED to create 'abuffer' filter! fferr = " << fferr << ".";
            err = oss.str();
            return false;
        }
        avFilter = avfilter_get_by_name("abuffersink");
        fferr = avfilter_graph_create_filter(&outFilterCtx, avFilter, "buffer_sink", NULL, NULL, filterGraph);
        if (fferr < 0)
        {
            std::ostringstream oss;
            oss << "FAILED to create 'abuffersink' filter! fferr = " << fferr << ".";
            err = oss.str();
            return false;
        }
        AVFilterInOut *outputs = avfilter_inout_alloc();
        AVFilterInOut *inputs = avfilter_inout_alloc();
        outputs->name = av_strdup("in");
        outputs->filter_ctx = inFilterCtx;
        outputs->pad_idx = 0;
        outputs->next = NULL;
        inputs->name = av_strdup("out");
        inputs->filter_ctx = outFilterCtx;
        inputs->pad_idx = 0;
        inputs->next = NULL;
        int orgNbFilters = filterGraph->nb_filters;
        fferr = avfilter_graph_parse_ptr(filterGraph, argstr.c_str(), &inputs, &outputs, NULL);
        if (fferr < 0)
        {
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);
            std::ostringstream oss;
            oss << "FAILED to parse audio filter graph args! Arguments string is '" << argstr.c_str() << "'. fferr = " << fferr << ".";
            err = oss.str();
            return false;
        }
        int newNbFilters = filterGraph->nb_filters;
        if (newNbFilters > orgNbFilters)
        {
            for (int i = 0; i < newNbFilters-orgNbFilters; i++)
            {
                auto filter = filterGraph->filters[i];
                filterGraph->filters[i] = filterGraph->filters[i+orgNbFilters];
                filterGraph->filters[i+orgNbFilters] = filter;
            }
        }
        fferr = avfilter_graph_config(filterGraph, NULL);
        if (fferr < 0)
        {
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);
            std::ostringstream oss;
            oss << "FAILED to config audio filter graph! fferr = " << fferr << ".";
            throw std::runtime_error(oss.str());
        }
        avfilter_inout_free(&outputs);
        avfilter_inout_free(&inputs);
        if (m_filterGraph)
        {
            avfilter_graph_free(&m_filterGraph);
            m_filterGraph = nullptr;
            m_inFilterCtx = nullptr;
            m_outFilterCtx = nullptr;
        }
        m_filterGraph = filterGraph;
        m_inFilterCtx = inFilterCtx;
        m_outFilterCtx = outFilterCtx;

        return true;
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto inMat = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (!inMat.empty())
        {
            if (!m_Enabled)
            {
                m_MatOut.SetValue(inMat);
                return m_Exit;
            }

#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
            if (m_channels == -1)
#else
            if (m_chlyt.nb_channels == -1)
#endif
            {
                if (inMat.c != 1 && inMat.c != 2)
                    throw std::runtime_error("ONLY SUPPORT audio 'channels' of 1 or 2.");
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
                m_channels = inMat.c;
                m_channelLayout = av_get_default_channel_layout(m_channels);
#else
                av_channel_layout_default(&m_chlyt, inMat.c);
#endif
            }
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
            if (m_channels != inMat.c)
#else
            if (m_chlyt.nb_channels != inMat.c)
#endif
                throw std::runtime_error("Do NOT SUPPORT audio 'channels' changing during runtime!");
            if (!m_pDataConverter)
            {
                if (inMat.rate.num <= 0 || inMat.rate.den != 1)
                {
                    std::ostringstream oss;
                    oss << "INVALID input audio ImMat 'rate' {" << inMat.rate.num << "/" << inMat.rate.den << "}!";
                    throw std::runtime_error(oss.str());
                }
                m_pDataConverter = new AudioAVFrameImMatConverter(inMat.rate.num);
            }
            else if (inMat.rate.num != m_pDataConverter->SampleRate() || inMat.rate.den != 1)
            {
                std::ostringstream oss;
                oss << "WRONG input audio ImMat 'rate' {" << inMat.rate.num << "/" << inMat.rate.den << "}! Does NOT match saved sample rate "
                    << m_pDataConverter->SampleRate() << ".";
                throw std::runtime_error(oss.str());
            }

            ImGui::ImMat outMat;
            SelfFreeAVFramePtr avfrmptr = AllocSelfFreeAVFramePtr();
            AVRational tb = { 1, inMat.rate.num };
            int64_t pts = av_rescale_q((int64_t)(inMat.time_stamp*1000), TIMEBASE_MILLISEC, tb);
            if (!m_pDataConverter->ConvertImMatToAVFrame(inMat, avfrmptr.get(), pts))
            {
                throw std::runtime_error("FAILED to convert audio 'ImMat' to 'AVFrame'!");
            }

            if (!m_inputAttrSet)
            {
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
                bool success = InitFFmpegFilterGraph((AVSampleFormat)avfrmptr->format, avfrmptr->sample_rate, m_channelLayout, m_filterGraphInitErrMsg);
#else
                bool success = InitFFmpegFilterGraph((AVSampleFormat)avfrmptr->format, avfrmptr->sample_rate, avfrmptr->ch_layout, m_filterGraphInitErrMsg);
#endif
                if (!success)
                    throw std::runtime_error(m_filterGraphInitErrMsg);
                m_sampleFormat = (AVSampleFormat)avfrmptr->format;
                m_sampleRate = avfrmptr->sample_rate;
                m_inputAttrSet = true;
            }

            int fferr;
            fferr = av_buffersrc_write_frame(m_inFilterCtx, avfrmptr.get());
            if (fferr < 0)
            {
                std::ostringstream oss;
                oss << "FAILED to write frame into audio filter graph! fferr = " << fferr << ".";
                throw std::runtime_error(oss.str());
            }
            av_frame_unref(avfrmptr.get());
            fferr = av_buffersink_get_frame(m_outFilterCtx, avfrmptr.get());
            if (fferr == 0)
            {
                ImGui::ImMat outMat;
                double timestamp = (double)av_rescale_q(avfrmptr->pts, { 1, avfrmptr->sample_rate }, TIMEBASE_MILLISEC)/1000;
                if (!m_pDataConverter->ConvertAVFrameToImMat(avfrmptr.get(), outMat, timestamp))
                {
                    throw std::runtime_error("FAILED to convert audio 'AVFrame' to 'ImMat'!");
                }
                outMat.copy_attribute(inMat);
                m_MatOut.SetValue(outMat);
            }
        }
        return m_Exit;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        changed |= Node::DrawDataTypeSetting("Mat Type:", m_pcmDataType);
        return changed;
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key, bool embedded) override
    {
        ImGui::SetCurrentContext(ctx);
        float indent_offset = 0;
        if (!embedded)
        {
            ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 sub_window_size = ImGui::GetWindowSize();
            indent_offset = (sub_window_size.x - 36 * 11) / 2;
        }
        bool changed = false;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput | ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Mark;
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::SetCursorScreenPos(ImGui::GetCursorScreenPos() + ImVec2(indent_offset, 0));
        ImGui::TextColored({ 0.9, 0.4, 0.4, 1.0 }, "Hz");
        for (int i = 0; i < 10; i++)
        {
            if (i > 0) ImGui::SameLine();
            else ImGui::SetCursorScreenPos(ImGui::GetCursorScreenPos() + ImVec2(indent_offset, 0));
            ImGui::BeginGroup();
            std::string cfTag = GetFrequencyTag(m_bandCfg[i].centerFreq);
            ImGui::TextUnformatted(cfTag.c_str());
            ImGui::PushID(i);
            int gain = m_bandCfg[i].gain;
            ImGui::VSliderInt("##band_gain", ImVec2(24, 200), &gain, MIN_GAIN, MAX_GAIN, "%+ddm", flags);
            ImGui::PopID();
            if (gain != m_bandCfg[i].gain)
            {
                char targetFilter[32] = {0};
                snprintf(targetFilter, sizeof(targetFilter)-1, "equalizer@%d", i);
                char cmdarg[8] = {0};
                snprintf(cmdarg, sizeof(cmdarg)-1, "%d", gain);
                char res[128] = {0};
                if (m_filterGraph)
                {
                    if (avfilter_graph_send_command(m_filterGraph, targetFilter, "gain", cmdarg, res, sizeof(res)-1, 0) < 0)
                    {
                        std::ostringstream oss;
                        oss << "FAILED to invoke 'avfilter_graph_send_command' to set gain value " << gain << " to target filter '" << targetFilter << "'!";
                        throw std::runtime_error(oss.str());
                    }
                }
                m_bandCfg[i].gain = gain;
                changed = true;
            }
            ImGui::Text("%d", gain);
            ImGui::EndGroup();
        }
        ImGui::SetCursorScreenPos(ImGui::GetCursorScreenPos() + ImVec2(indent_offset, 0));
        ImGui::TextColored({ 0.4, 0.4, 0.9, 1.0 }, "Db");
        ImGui::SameLine(indent_offset + 360 - 36);
        if (ImGui::Button(ICON_RESET "##reset_equ##AudioEqualizer"))
        {
            for (int i = 0; i < 10; i++)
            {
                if (m_bandCfg[i].gain != 0)
                {
                    char targetFilter[32] = {0};
                    snprintf(targetFilter, sizeof(targetFilter)-1, "equalizer@%d", i);
                    char cmdarg[8] = {0};
                    snprintf(cmdarg, sizeof(cmdarg)-1, "%d", 0);
                    char res[128] = {0};
                    int fferr = avfilter_graph_send_command(m_filterGraph, targetFilter, "gain", cmdarg, res, sizeof(res)-1, 0);
                    if (fferr < 0)
                    {
                        std::ostringstream oss;
                        oss << "FAILED to invoke 'avfilter_graph_send_command' to set gain value " << 0 << " to target filter '" << targetFilter << "'!";
                        throw std::runtime_error(oss.str());
                    }
                    m_bandCfg[i].gain = 0;
                    changed = true;
                }
            }
        }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        return m_Enabled ? changed : false;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;

        if (value.contains("mat_type"))
        {
            auto& val = value["mat_type"];
            if (val.is_number()) 
                m_pcmDataType = (ImDataType)val.get<imgui_json::number>();
        }
        if (value.contains("band_gains") && value["band_gains"].is_array())
        {
            auto& bandGainsAry = value["band_gains"].get<imgui_json::array>();
            int idx = 0;
            for (auto& jval : bandGainsAry)
            {
                m_bandCfg[idx].gain = (int32_t)jval.get<imgui_json::number>();
                if (idx >= 10)
                    break;
                idx++;
            }
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_pcmDataType);
        imgui_json::array bandGains;
        for (auto& bandCfg : m_bandCfg)
            bandGains.push_back(imgui_json::number(bandCfg.gain));
        value["band_gains"] = bandGains;
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\uf1de"));
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatIn}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };
    MatPin    m_MatIn   = { this, "In" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

    struct BandConfig
    {
        uint32_t centerFreq;  // in Hz
        uint32_t bandWidth;  // in Hz
        int32_t gain;  // in db
    };

    const AVRational TIMEBASE_MILLISEC = { 1, 1000 };
    const BandConfig DEFAULT_BAND_CFG[10] = {
        { 32,       32,         0 },        { 64,       64,         0 },
        { 125,      125,        0 },        { 250,      250,        0 },
        { 500,      500,        0 },        { 1000,     1000,       0 },
        { 2000,     2000,       0 },        { 4000,     4000,       0 },
        { 8000,     8000,       0 },        { 16000,    16000,      0 },
    };
    const int32_t MAX_GAIN = 12;
    const int32_t MIN_GAIN = -12;

private:
    static ImDataType GetImDataTypeByAVSampleFormat(AVSampleFormat smpfmt)
    {
        ImDataType dtype = IM_DT_UNDEFINED;
        switch (smpfmt)
        {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            dtype = IM_DT_INT8;
            break;
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            dtype = IM_DT_INT16;
            break;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
            dtype = IM_DT_INT32;
            break;
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            dtype = IM_DT_FLOAT32;
            break;
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
            dtype = IM_DT_FLOAT64;
            break;
        case AV_SAMPLE_FMT_S64:
        case AV_SAMPLE_FMT_S64P:
            dtype = IM_DT_INT64;
            break;
        default:
            break;
        }
        return dtype;
    }

    static AVSampleFormat GetAVSampleFormatByImDataType(ImDataType dtype, bool isPlanar)
    {
        AVSampleFormat smpfmt = AV_SAMPLE_FMT_NONE;
        switch (dtype)
        {
        case IM_DT_INT8:
            smpfmt = AV_SAMPLE_FMT_U8;
            break;
        case IM_DT_INT16:
            smpfmt = AV_SAMPLE_FMT_S16;
            break;
        case IM_DT_INT32:
            smpfmt = AV_SAMPLE_FMT_S32;
            break;
        case IM_DT_FLOAT32:
            smpfmt = AV_SAMPLE_FMT_FLT;
            break;
        case IM_DT_FLOAT64:
            smpfmt = AV_SAMPLE_FMT_DBL;
            break;
        case IM_DT_INT64:
            smpfmt = AV_SAMPLE_FMT_S64;
            break;
        default:
            break;
        }
        if (smpfmt != AV_SAMPLE_FMT_NONE && isPlanar)
            smpfmt = av_get_planar_sample_fmt(smpfmt);
        return smpfmt;
    }

    static std::string GetFrequencyTag(uint32_t freq)
    {
        char tag[16]= {0};
        if (freq < 1000)
            snprintf(tag, 16, "%u", freq);
        else if (freq%1000 > 0)
            snprintf(tag, sizeof(tag)-1, "%.1fK", (float)(freq/1000));
        else
            snprintf(tag, sizeof(tag)-1, "%uK", freq/1000);
        return std::string(tag);
    }

    using SelfFreeAVFramePtr = std::shared_ptr<AVFrame>;
    static SelfFreeAVFramePtr AllocSelfFreeAVFramePtr()
    {
        SelfFreeAVFramePtr frm = shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame* avfrm) {
            if (avfrm)
                av_frame_free(&avfrm);
        });
        if (!frm.get())
            return nullptr;
        return frm;
    }

    class AudioAVFrameImMatConverter
    {
    public:
        AudioAVFrameImMatConverter(uint32_t sampleRate) { m_sampleRate = sampleRate; };
        bool ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& amat, double timestamp)
        {
            amat.release();
            ImDataType dtype = GetImDataTypeByAVSampleFormat((AVSampleFormat)avfrm->format);
            bool isPlanar = av_sample_fmt_is_planar((AVSampleFormat)avfrm->format) == 1;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
            const int channels = avfrm->channels;
#else
            const int channels = avfrm->ch_layout.nb_channels;
#endif
            amat.create_type(avfrm->nb_samples, (int)1, channels, dtype);
            amat.elempack = isPlanar ? 1 : channels;
            int bytesPerSample = av_get_bytes_per_sample((AVSampleFormat)avfrm->format);
            int bytesPerLine = avfrm->nb_samples*bytesPerSample*(isPlanar?1:channels);
            if (isPlanar)
            {
                uint8_t* dstptr = (uint8_t*)amat.data;
                for (int i = 0; i < channels; i++)
                {
                    const uint8_t* srcptr = i < 8 ? avfrm->data[i] : avfrm->extended_data[i-8];
                    memcpy(dstptr, srcptr, bytesPerLine);
                    dstptr += bytesPerLine;
                }
            }
            else
            {
                int totalBytes = bytesPerLine;
                memcpy(amat.data, avfrm->data[0], totalBytes);
            }
            amat.rate = { avfrm->sample_rate, 1 };
            amat.time_stamp = timestamp;
            amat.elempack = isPlanar ? 1 : channels;
            return true;
        }
        bool ConvertImMatToAVFrame(const ImGui::ImMat& amat, AVFrame* avfrm, int64_t pts)
        {
            av_frame_unref(avfrm);
            bool isPlanar = amat.elempack == 1;
            avfrm->format = (int)GetAVSampleFormatByImDataType(amat.type, isPlanar);
            avfrm->nb_samples = amat.w;
            const int channels = amat.c;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
            avfrm->channels = channels;
            avfrm->channel_layout = av_get_default_channel_layout(channels);
#else
            av_channel_layout_default(&avfrm->ch_layout, channels);
#endif
            int fferr = av_frame_get_buffer(avfrm, 0);
            if (fferr < 0)
            {
                std::cerr << "FAILED to allocate buffer for audio AVFrame! format=" << av_get_sample_fmt_name((AVSampleFormat)avfrm->format)
                    << ", nb_samples=" << avfrm->nb_samples << ", channels=" << channels << ". fferr=" << fferr << "." << std::endl;
                return false;
            }
            int bytesPerSample = av_get_bytes_per_sample((AVSampleFormat)avfrm->format);
            int bytesPerLine = avfrm->nb_samples*bytesPerSample*(isPlanar?1:channels);
            if (isPlanar)
            {
                const uint8_t* srcptr = (const uint8_t*)amat.data;
                for (int i = 0; i < channels; i++)
                {
                    uint8_t* dstptr = i < 8 ? avfrm->data[i] : avfrm->extended_data[i-8];
                    memcpy(dstptr, srcptr, bytesPerLine);
                    srcptr += bytesPerLine;
                }
            }
            else
            {
                int totalBytes = bytesPerLine;
                memcpy(avfrm->data[0], amat.data, totalBytes);
            }
            avfrm->sample_rate = amat.rate.num;
            avfrm->pts = pts;
            return true;
        }
        uint32_t SampleRate() const { return m_sampleRate; }
    private:
        uint32_t m_sampleRate {0};
    };

private:
    ImDataType m_pcmDataType {IM_DT_UNDEFINED};
    bool m_inputAttrSet {false};
    AVSampleFormat m_sampleFormat   {AV_SAMPLE_FMT_NONE};
    int m_sampleRate    {-1};
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
    int m_channels      {-1};
    uint64_t m_channelLayout    {0};
#else
    AVChannelLayout m_chlyt;
#endif
    BandConfig m_bandCfg[10];
    int m_filterArgsMaxChars    {2048};
    bool m_filterArgsChanged    {false};
    std::string m_filterGraphInitErrMsg;
    AudioAVFrameImMatConverter* m_pDataConverter    {nullptr};
    AVFilterGraph* m_filterGraph    {nullptr};
    AVFilterContext* m_inFilterCtx  {nullptr};
    AVFilterContext* m_outFilterCtx {nullptr};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(AudioEqualizerNode, "Audio Equalizer", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Audio")

