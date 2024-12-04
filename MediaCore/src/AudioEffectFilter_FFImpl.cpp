/*
    Copyright (c) 2023-2024 CodeWin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <sstream>
#include <iostream>
#include "AudioEffectFilter.h"
#include "FFUtils.h"
extern "C"
{
    #include "libavutil/avutil.h"
    #include "libavfilter/avfilter.h"
    #include "libavfilter/buffersrc.h"
    #include "libavfilter/buffersink.h"
    #include "libswresample/swresample.h"
}

using namespace std;
using namespace Logger;

namespace MediaCore
{
class AudioEffectFilter_FFImpl : public AudioEffectFilter
{
public:
    AudioEffectFilter_FFImpl(const string& loggerName = "")
    {
        if (loggerName.empty())
            m_logger = AudioEffectFilter::GetLogger();
        else
        {
            m_logger = Logger::GetLogger(loggerName);
            int n;
            Level l = AudioEffectFilter::GetLogger()->GetShowLevels(n);
            m_logger->SetShowLevels(l, n);
        }
    }

    virtual ~AudioEffectFilter_FFImpl()
    {
        ReleaseFilterGraph();
        ReleasePanFilterGraph();
    }

    bool Init(uint32_t composeFlags, const string& sampleFormat, uint32_t channels, uint32_t sampleRate) override
    {
        AVSampleFormat smpfmt = av_get_sample_fmt(sampleFormat.c_str());
        if (smpfmt == AV_SAMPLE_FMT_NONE)
        {
            ostringstream oss;
            oss << "Invalid argument 'sampleFormat' for AudioEffectFilter::Init()! Value '" << sampleFormat << "' is NOT a VALID sample format.";
            m_errMsg = oss.str();
            return false;
        }
        if (channels == 0)
        {
            ostringstream oss;
            oss << "Invalid argument 'channels' for AudioEffectFilter::Init()! Value " << channels << " is a bad value.";
            m_errMsg = oss.str();
            return false;
        }
        if (sampleRate == 0)
        {
            ostringstream oss;
            oss << "Invalid argument 'sampleRate' for AudioEffectFilter::Init()! Value " << sampleRate << " is a bad value.";
            m_errMsg = oss.str();
            return false;
        }

        if (composeFlags > 0)
        {
            bool ret = CreateFilterGraph(composeFlags, smpfmt, channels, sampleRate);
            if (!ret)
                return false;
            if (CheckFilters(composeFlags, PAN))
            {
                ret = CreatePanFilterGraph(smpfmt, channels, sampleRate);
                if (!ret)
                    return false;
            }
        }
        else
        {
            m_logger->Log(DEBUG) << "This 'AudioEffectFilter' is using pass-through mode because 'composeFlags' is 0." << endl;
            m_passThrough = true;
        }

        m_smpfmt = smpfmt;
        m_matDt = GetDataTypeFromSampleFormat(smpfmt);
        m_channels = channels;
        m_sampleRate = sampleRate;
        m_blockAlign = channels*av_get_bytes_per_sample(smpfmt);
        m_isPlanar = av_sample_fmt_is_planar(smpfmt);

        m_inited = true;
        return true;
    }

    bool ProcessData(const ImGui::ImMat& in, list<ImGui::ImMat>& out) override
    {
        out.clear();
        if (!m_inited)
        {
            m_errMsg = "This 'AudioEffectFilter' instance is NOT INITIALIZED!";
            return false;
        }
        if (in.empty())
            return true;
        if (m_passThrough)
        {
            out.push_back(in);
            return true;
        }

        SelfFreeAVFramePtr avfrm = AllocSelfFreeAVFramePtr();
        int64_t pts = (int64_t)(in.time_stamp*m_sampleRate);
        if (!m_matCvter.ConvertImMatToAVFrame(in, avfrm.get(), pts))
        {
            ostringstream oss;
            oss << "FAILED to invoke AudioImMatAVFrameConverter::ConvertImMatToAVFrame()!";
            m_errMsg = oss.str();
            return false;
        }
        if (in.c == 1 && avfrm->format != (int)m_smpfmt)
        {
            AVSampleFormat altfmt;
            if (m_isPlanar)
                altfmt = av_get_planar_sample_fmt((AVSampleFormat)avfrm->format);
            else
                altfmt = av_get_packed_sample_fmt((AVSampleFormat)avfrm->format);
            if (altfmt == m_smpfmt)
                avfrm->format = (int)m_smpfmt;
        }
        // m_logger->Log(DEBUG) << "Get incoming mat: ts=" << in.time_stamp << "; avfrm: pts=" << pts << endl;

        UpdateFilterParameters();

        int fferr;
        if (m_useGeneralFg)
        {
            fferr = av_buffersrc_add_frame(m_bufsrcCtx, avfrm.get());
            if (fferr < 0)
            {
                ostringstream oss;
                oss << "FAILED to invoke av_buffersrc_add_frame()! fferr = " << fferr << ".";
                m_errMsg = oss.str();
                return false;
            }
        }

        bool hasErr = false;
        while (true)
        {
            if (m_useGeneralFg)
            {
                av_frame_unref(avfrm.get());
                fferr = av_buffersink_get_frame(m_bufsinkCtx, avfrm.get());
            }
            else
            {
                fferr = 0;
            }
            if (fferr >= 0)
            {
                if (m_usePanFg)
                {
                    fferr = av_buffersrc_add_frame(m_panBufsrcCtx, avfrm.get());
                    if (fferr < 0)
                    {
                        ostringstream oss;
                        oss << "FAILED to invoke av_buffersrc_add_frame() on PAN filter-graph! fferr = " << fferr << ".";
                        m_errMsg = oss.str();
                        hasErr = true;
                        break;
                    }
                    av_frame_unref(avfrm.get());
                    fferr = av_buffersink_get_frame(m_panBufsinkCtx, avfrm.get());
                    if (fferr < 0)
                    {
                        ostringstream oss;
                        oss << "FAILED to invoke av_buffersink_get_frame() on PAN filter-graph! fferr = " << fferr << ".";
                        m_errMsg = oss.str();
                        hasErr = true;
                        break;
                    }
                }
                if (avfrm->nb_samples > 0)
                {
                    ImGui::ImMat m;
                    double ts = (double)avfrm->pts/m_sampleRate;

                    // handle muted state here if 'VOLUME' is not a part of the filter compsition
                    if (!HasFilter(VOLUME) && m_currMuted)
                    {
                        ImDataType dtype = GetDataTypeFromSampleFormat((AVSampleFormat)avfrm->format);
                        bool isPlanar = av_sample_fmt_is_planar((AVSampleFormat)avfrm->format) == 1;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
                        const int channels = avfrm->channels;
#else
                        const int channels = avfrm->ch_layout.nb_channels;
#endif
                        m.create_type(avfrm->nb_samples, (int)1, channels, dtype);
                        memset(m.data, 0, m.total()*m.elemsize);
                        m.flags = IM_MAT_FLAGS_AUDIO_FRAME;
                        m.rate = { (int)m_sampleRate, 1 };
                        m.elempack = m_isPlanar ? 1 : m_channels;
                        m.time_stamp = ts;
                    }
                    else
                    {
                        if (m_matCvter.ConvertAVFrameToImMat(avfrm.get(), m, ts))
                        {
                            // m_logger->Log(DEBUG) << "Add output avfrm: pts=" << avfrm->pts << "; mat: ts=" << m.time_stamp << endl;
                            m.type = m_matDt;
                            m.flags = IM_MAT_FLAGS_AUDIO_FRAME;
                            m.rate = { (int)m_sampleRate, 1 };
                            m.elempack = m_isPlanar ? 1 : m_channels;
                            out.push_back(m);
                        }
                        else
                        {
                            ostringstream oss;
                            oss << "FAILED to invoke AudioImMatAVFrameConverter::ConvertAVFrameToImMat()!";
                            m_errMsg = oss.str();
                            hasErr = true;
                            break;
                        }
                    }
                }
                else
                {
                    Log(WARN) << "av_buffersink_get_frame() returns INVALID number of samples! nb_samples=" << avfrm->nb_samples << "." << endl;
                }
            }
            else if (fferr == AVERROR(EAGAIN))
                break;
            else
            {
                ostringstream oss;
                oss << "FAILED to invoke av_buffersink_get_frame()! fferr = " << fferr << ".";
                m_errMsg = oss.str();
                hasErr = true;
                break;
            }
        }
        return !hasErr;
    }

    bool HasFilter(uint32_t composeFlags) const override
    {
        return CheckFilters(m_composeFlags, composeFlags);
    }

    void CopyParamsFrom(AudioEffectFilter* pAeFilter) override
    {
        auto volumeParams = pAeFilter->GetVolumeParams();
        SetVolumeParams(&volumeParams);
        auto panParams = pAeFilter->GetPanParams();
        SetPanParams(&panParams);
        auto limiterParams = pAeFilter->GetLimiterParams();
        SetLimiterParams(&limiterParams);
        auto gateParams = pAeFilter->GetGateParams();
        SetGateParams(&gateParams);
        auto compressorParams = pAeFilter->GetCompressorParams();
        SetCompressorParams(&compressorParams);
        auto eqBandInfo = pAeFilter->GetEqualizerBandInfo();
        for (int i = 0; i < eqBandInfo.bandCount; i++)
        {
            auto eqParams = pAeFilter->GetEqualizerParamsByIndex(i);
            SetEqualizerParamsByIndex(&eqParams, i);
        }
        auto isMuted = pAeFilter->IsMuted();
        SetMuted(isMuted);
    }

    bool SetVolumeParams(VolumeParams* params) override
    {
        if (!HasFilter(VOLUME))
        {
            m_errMsg = "CANNOT set 'VolumeParams' because this instance is NOT initialized with 'AudioEffectFilter::VOLUME' compose-flag!";
            return false;
        }
        m_setVolumeParams = *params;
        return true;
    }

    VolumeParams GetVolumeParams() const override
    {
        return m_setVolumeParams;
    }

    bool SetPanParams(PanParams* params) override
    {
        if (!HasFilter(PAN))
        {
            m_errMsg = "CANNOT set 'PanParams' because this instance is NOT initialized with 'AudioEffectFilter::PAN' compose-flag!";
            return false;
        }
        m_setPanParams = *params;
        return true;
    }

    PanParams GetPanParams() const override
    {
        return m_setPanParams;
    }

    bool SetLimiterParams(LimiterParams* params) override
    {
        if (!HasFilter(LIMITER))
        {
            m_errMsg = "CANNOT set 'LimiterParams' because this instance is NOT initialized with 'AudioEffectFilter::LIMITER' compose-flag!";
            return false;
        }
        m_setLimiterParams = *params;
        return true;
    }

    LimiterParams GetLimiterParams() const override
    {
        return m_setLimiterParams;
    }

    bool SetGateParams(GateParams* params) override
    {
        if (!HasFilter(GATE))
        {
            m_errMsg = "CANNOT set 'GateParams' because this instance is NOT initialized with 'AudioEffectFilter::GATE' compose-flag!";
            return false;
        }
        m_setGateParams = *params;
        return true;
    }

    GateParams GetGateParams() const override
    {
        return m_setGateParams;
    }

    bool SetCompressorParams(CompressorParams* params) override
    {
        if (!HasFilter(COMPRESSOR))
        {
            m_errMsg = "CANNOT set 'CompressorParams' because this instance is NOT initialized with 'AudioEffectFilter::COMPRESSOR' compose-flag!";
            return false;
        }
        m_setCompressorParams = *params;
        return true;
    }

    CompressorParams GetCompressorParams() const override
    {
        return m_setCompressorParams;
    }

    bool SetEqualizerParamsByIndex(EqualizerParams* params, uint32_t index) override
    {
        if (!HasFilter(EQUALIZER))
        {
            m_errMsg = "CANNOT set 'EqualizerParams' because this instance is NOT initialized with 'AudioEffectFilter::EQUALIZER' compose-flag!";
            return false;
        }
        m_setEqualizerParamsList.at(index) = *params;
        return true;
    }

    EqualizerParams GetEqualizerParamsByIndex(uint32_t index) const override
    {
        return m_setEqualizerParamsList.at(index);
    }

    EqualizerBandInfo GetEqualizerBandInfo() const override
    {
        EqualizerBandInfo eqBandInfo;
        eqBandInfo.bandCount = DF_CENTER_FREQS.size();
        eqBandInfo.centerFreqList = DF_CENTER_FREQS.data();
        eqBandInfo.bandWidthList = DF_BAND_WTHS.data();
        return eqBandInfo;
    }

    void SetMuted(bool muted) override
    {
        m_setMuted = muted;
    }

    bool IsMuted() const override
    {
        return m_setMuted;
    }

    string GetError() const override
    {
        return m_errMsg;
    }

private:
    bool CreateFilterGraph(uint32_t composeFlags, const AVSampleFormat smpfmt, uint32_t channels, uint32_t sampleRate)
    {
        if ((composeFlags&~PAN) == 0)
        {
            m_useGeneralFg = false;
            return true;
        }
        else
        {
            m_useGeneralFg = true;
        }

        const AVFilter *abuffersrc  = avfilter_get_by_name("abuffer");
        const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
        m_filterGraph = avfilter_graph_alloc();
        if (!m_filterGraph)
        {
            m_errMsg = "FAILED to allocate new 'AVFilterGraph'!";
            return false;
        }

        ostringstream abufsrcArgsOss;
        abufsrcArgsOss << "time_base=1/" << sampleRate << ":sample_rate=" << sampleRate
            << ":sample_fmt=" << av_get_sample_fmt_name(smpfmt);
        char chlytDescBuff[256] = {0};
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        int64_t chlyt = av_get_default_channel_layout(channels);
        av_get_channel_layout_string(chlytDescBuff, sizeof(chlytDescBuff), channels, (uint64_t)chlyt);
#else
        AVChannelLayout chlyt{AV_CHANNEL_ORDER_UNSPEC, 0};
        av_channel_layout_default(&chlyt, channels);
        av_channel_layout_describe(&chlyt, chlytDescBuff, sizeof(chlytDescBuff));
#endif
        abufsrcArgsOss << ":channel_layout=" << chlytDescBuff;
        string bufsrcArgs = abufsrcArgsOss.str();
        int fferr;
        AVFilterContext* bufSrcCtx = nullptr;
        fferr = avfilter_graph_create_filter(&bufSrcCtx, abuffersrc, "BufferSource", bufsrcArgs.c_str(), nullptr, m_filterGraph);
        if (fferr < 0)
        {
            ostringstream oss;
            oss << "FAILED when invoking 'avfilter_graph_create_filter' for source buffer! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }
        AVFilterInOut* outputs = avfilter_inout_alloc();
        if (!outputs)
        {
            m_errMsg = "FAILED to allocate 'AVFilterInOut' instance!";
            return false;
        }
        outputs->name       = av_strdup("in");
        outputs->filter_ctx = bufSrcCtx;
        outputs->pad_idx    = 0;
        outputs->next       = nullptr;

        AVFilterContext* bufSinkCtx = nullptr;
        fferr = avfilter_graph_create_filter(&bufSinkCtx, abuffersink, "BufferSink", nullptr, nullptr, m_filterGraph);
        if (fferr < 0)
        {
            ostringstream oss;
            oss << "FAILED when invoking 'avfilter_graph_create_filter' for sink buffer! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }
        AVFilterInOut* inputs = avfilter_inout_alloc();
        if (!inputs)
        {
            avfilter_inout_free(&outputs);
            m_errMsg = "FAILED to allocate 'AVFilterInOut' instance!";
            return false;
        }
        inputs->name        = av_strdup("out");
        inputs->filter_ctx  = bufSinkCtx;
        inputs->pad_idx     = 0;
        inputs->next        = nullptr;

        ostringstream fgArgsOss;
        bool isFirstFilter = true;
        if (CheckFilters(composeFlags, LIMITER))
        {
            if (!isFirstFilter) fgArgsOss << ","; else isFirstFilter = false;
            fgArgsOss << "alimiter=limit=" << m_currLimiterParams.limit << ":attack=" << m_currLimiterParams.attack << ":release=" << m_currLimiterParams.release;
        }
        if (CheckFilters(composeFlags, GATE))
        {
            if (!isFirstFilter) fgArgsOss << ","; else isFirstFilter = false;
            fgArgsOss << "agate=threshold=" << m_currGateParams.threshold << ":range=" << m_currGateParams.range << ":ratio=" << m_currGateParams.ratio << ":attack="
                << m_currGateParams.attack << ":release=" << m_currGateParams.release << ":makeup=" << m_currGateParams.makeup << ":knee=" << m_currGateParams.knee;
        }
        if (CheckFilters(composeFlags, EQUALIZER))
        {
            for (int i=0; i < DF_CENTER_FREQS.size(); i++)
            {
                m_currEqualizerParamsList.push_back({0});
                m_setEqualizerParamsList.push_back({0});
                if (!isFirstFilter) fgArgsOss << ","; else isFirstFilter = false;
                fgArgsOss << "equalizer@" << i << "=f=" << DF_CENTER_FREQS[i] << ":t=h:w=" << DF_BAND_WTHS[i] << ":g=0";
            }
        }
        if (CheckFilters(composeFlags, COMPRESSOR))
        {
            if (!isFirstFilter) fgArgsOss << ","; else isFirstFilter = false;
            fgArgsOss << "acompressor=threshold=" << m_currCompressorParams.threshold << ":ratio=" << m_currCompressorParams.ratio << ":knee="
                << m_currCompressorParams.knee << ":mix=" << m_currCompressorParams.mix << ":attack=" << m_currCompressorParams.attack << ":release="
                << m_currCompressorParams.release << ":makeup=" << m_currCompressorParams.makeup << ":level_in=" << m_currCompressorParams.levelIn;
        }
        if (CheckFilters(composeFlags, VOLUME))
        {
            if (!isFirstFilter) fgArgsOss << ","; else isFirstFilter = false;
            fgArgsOss << "volume=volume=" << m_currVolumeParams.volume << ":precision=float:eval=frame";
        }
        if (!isFirstFilter)
        {
            fgArgsOss << ",aformat=sample_fmts=" << av_get_sample_fmt_name(smpfmt);
        }
        string fgArgs = fgArgsOss.str();
        m_logger->Log(DEBUG) << "Initialze filter-graph with arguments '" << fgArgs << "'." << endl;
        fferr = avfilter_graph_parse_ptr(m_filterGraph, fgArgs.c_str(), &inputs, &outputs, nullptr);
        if (fferr < 0)
        {
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);
            ostringstream oss;
            oss << "FAILED to invoke 'avfilter_graph_parse_ptr' with arguments string '" << fgArgs << "'! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }

        fferr = avfilter_graph_config(m_filterGraph, nullptr);
        if (fferr < 0)
        {
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);
            ostringstream oss;
            oss << "FAILED to invoke 'avfilter_graph_config'! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }

        avfilter_inout_free(&outputs);
        avfilter_inout_free(&inputs);
        m_bufsrcCtx = bufSrcCtx;
        m_bufsinkCtx = bufSinkCtx;
        m_composeFlags = composeFlags;
        return true;
    }

    void ReleaseFilterGraph()
    {
        if (m_filterGraph)
        {
            avfilter_graph_free(&m_filterGraph);
            m_filterGraph = nullptr;
        }
        m_bufsrcCtx = nullptr;
        m_bufsinkCtx = nullptr;
    }

    bool CreatePanFilterGraph(const AVSampleFormat smpfmt, uint32_t channels, uint32_t sampleRate)
    {
        if (m_currPanParams.x == 0.5f && m_currPanParams.y == 0.5f)
        {
            m_usePanFg = false;
            return true;
        }
        else
        {
            m_usePanFg = true;
        }

        const AVFilter *abuffersrc  = avfilter_get_by_name("abuffer");
        const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
        m_panFg = avfilter_graph_alloc();
        if (!m_panFg)
        {
            m_errMsg = "FAILED to allocate new 'AVFilterGraph'(pan)!";
            return false;
        }

        ostringstream abufsrcArgsOss;
        abufsrcArgsOss << "time_base=1/" << sampleRate << ":sample_rate=" << sampleRate
            << ":sample_fmt=" << av_get_sample_fmt_name(smpfmt);
        char chlytDescBuff[256] = {0};
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        uint64_t chlyt = (uint64_t)av_get_default_channel_layout(channels);
        av_get_channel_layout_string(chlytDescBuff, sizeof(chlytDescBuff), channels, chlyt);
#else
        AVChannelLayout chlyt{AV_CHANNEL_ORDER_UNSPEC, 0};
        av_channel_layout_default(&chlyt, channels);
        av_channel_layout_describe(&chlyt, chlytDescBuff, sizeof(chlytDescBuff));
#endif
        abufsrcArgsOss << ":channel_layout=" << chlytDescBuff;
        string bufsrcArgs = abufsrcArgsOss.str();
        int fferr;
        AVFilterContext* bufSrcCtx = nullptr;
        fferr = avfilter_graph_create_filter(&bufSrcCtx, abuffersrc, "BufferSource", bufsrcArgs.c_str(), nullptr, m_panFg);
        if (fferr < 0)
        {
            ostringstream oss;
            oss << "FAILED when invoking 'avfilter_graph_create_filter' for source buffer! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }
        AVFilterInOut* outputs = avfilter_inout_alloc();
        if (!outputs)
        {
            m_errMsg = "FAILED to allocate 'AVFilterInOut' instance!";
            return false;
        }
        outputs->name       = av_strdup("in");
        outputs->filter_ctx = bufSrcCtx;
        outputs->pad_idx    = 0;
        outputs->next       = nullptr;

        AVFilterContext* bufSinkCtx = nullptr;
        fferr = avfilter_graph_create_filter(&bufSinkCtx, abuffersink, "BufferSink", nullptr, nullptr, m_panFg);
        if (fferr < 0)
        {
            ostringstream oss;
            oss << "FAILED when invoking 'avfilter_graph_create_filter' for sink buffer! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }
        AVFilterInOut* inputs = avfilter_inout_alloc();
        if (!inputs)
        {
            avfilter_inout_free(&outputs);
            m_errMsg = "FAILED to allocate 'AVFilterInOut' instance!";
            return false;
        }
        inputs->name        = av_strdup("out");
        inputs->filter_ctx  = bufSinkCtx;
        inputs->pad_idx     = 0;
        inputs->next        = nullptr;

        ostringstream fgArgsOss;
        fgArgsOss << "pan=" << chlytDescBuff << "| ";
        for (int i = 0; i < channels; i++)
        {
            double xCoef = 1., yCoef = 1.;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
            uint64_t ch = av_channel_layout_extract_channel(chlyt, i);
            if (ch == AV_CH_FRONT_LEFT || ch == AV_CH_BACK_LEFT || ch == AV_CH_FRONT_LEFT_OF_CENTER ||
                ch == AV_CH_SIDE_LEFT || ch == AV_CH_TOP_FRONT_LEFT || ch == AV_CH_TOP_BACK_LEFT ||
                ch == AV_CH_STEREO_LEFT || ch == AV_CH_WIDE_LEFT || ch == AV_CH_SURROUND_DIRECT_LEFT
#if (LIBAVUTIL_VERSION_MAJOR > 56) || (LIBAVUTIL_VERSION_MAJOR == 56) && (LIBAVUTIL_VERSION_MINOR > 57)
                || ch == AV_CH_TOP_SIDE_LEFT || ch == AV_CH_BOTTOM_FRONT_LEFT
#endif
                )
                xCoef *= (1-m_currPanParams.x)/0.5;
            else if (ch == AV_CH_FRONT_RIGHT || ch == AV_CH_BACK_RIGHT || ch == AV_CH_FRONT_RIGHT_OF_CENTER ||
                ch == AV_CH_SIDE_RIGHT || ch == AV_CH_TOP_FRONT_RIGHT || ch == AV_CH_TOP_BACK_RIGHT ||
                ch == AV_CH_STEREO_RIGHT || ch == AV_CH_WIDE_RIGHT || ch == AV_CH_SURROUND_DIRECT_RIGHT
#if (LIBAVUTIL_VERSION_MAJOR > 56) || (LIBAVUTIL_VERSION_MAJOR == 56) && (LIBAVUTIL_VERSION_MINOR > 57)
                || ch == AV_CH_TOP_SIDE_RIGHT || ch == AV_CH_BOTTOM_FRONT_RIGHT
#endif
                )
                xCoef *= m_currPanParams.x/0.5;
            if (ch == AV_CH_FRONT_LEFT || ch == AV_CH_FRONT_RIGHT || ch == AV_CH_FRONT_CENTER ||
                ch == AV_CH_FRONT_LEFT_OF_CENTER || ch == AV_CH_FRONT_RIGHT_OF_CENTER || ch == AV_CH_TOP_FRONT_LEFT ||
                ch == AV_CH_TOP_FRONT_CENTER || ch == AV_CH_TOP_FRONT_RIGHT
#if (LIBAVUTIL_VERSION_MAJOR > 56) || (LIBAVUTIL_VERSION_MAJOR == 56) && (LIBAVUTIL_VERSION_MINOR > 57)
                || ch == AV_CH_BOTTOM_FRONT_CENTER || ch == AV_CH_BOTTOM_FRONT_LEFT || ch == AV_CH_BOTTOM_FRONT_RIGHT
#endif                
                )
                yCoef *= (1-m_currPanParams.y)/0.5;
            else if (ch == AV_CH_BACK_LEFT || ch == AV_CH_BACK_RIGHT || ch == AV_CH_BACK_CENTER ||
                ch == AV_CH_TOP_BACK_LEFT || ch == AV_CH_TOP_BACK_CENTER || ch == AV_CH_TOP_BACK_RIGHT)
                yCoef *= m_currPanParams.y/0.5;
#else
            enum AVChannel ch = av_channel_layout_channel_from_index(&chlyt, (unsigned int)i);
            if (ch == AV_CHAN_FRONT_LEFT || ch == AV_CHAN_BACK_LEFT || ch == AV_CHAN_FRONT_LEFT_OF_CENTER ||
                ch == AV_CHAN_SIDE_LEFT || ch == AV_CHAN_TOP_FRONT_LEFT || ch == AV_CHAN_TOP_BACK_LEFT ||
                ch == AV_CHAN_STEREO_LEFT || ch == AV_CHAN_WIDE_LEFT || ch == AV_CHAN_SURROUND_DIRECT_LEFT ||
                ch == AV_CHAN_TOP_SIDE_LEFT || ch == AV_CHAN_BOTTOM_FRONT_LEFT)
                xCoef *= (1-m_currPanParams.x)/0.5;
            else if (ch == AV_CHAN_FRONT_RIGHT || ch == AV_CHAN_BACK_RIGHT || ch == AV_CHAN_FRONT_RIGHT_OF_CENTER ||
                ch == AV_CHAN_SIDE_RIGHT || ch == AV_CHAN_TOP_FRONT_RIGHT || ch == AV_CHAN_TOP_BACK_RIGHT ||
                ch == AV_CHAN_STEREO_RIGHT || ch == AV_CHAN_WIDE_RIGHT || ch == AV_CHAN_SURROUND_DIRECT_RIGHT ||
                ch == AV_CHAN_TOP_SIDE_RIGHT || ch == AV_CHAN_BOTTOM_FRONT_RIGHT)
                xCoef *= m_currPanParams.x/0.5;
            if (ch == AV_CHAN_FRONT_LEFT || ch == AV_CHAN_FRONT_RIGHT || ch == AV_CHAN_FRONT_CENTER ||
                ch == AV_CHAN_FRONT_LEFT_OF_CENTER || ch == AV_CHAN_FRONT_RIGHT_OF_CENTER || ch == AV_CHAN_TOP_FRONT_LEFT ||
                ch == AV_CHAN_TOP_FRONT_CENTER || ch == AV_CHAN_TOP_FRONT_RIGHT || ch == AV_CHAN_BOTTOM_FRONT_CENTER ||
                ch == AV_CHAN_BOTTOM_FRONT_LEFT || ch == AV_CHAN_BOTTOM_FRONT_RIGHT)
                yCoef *= (1-m_currPanParams.y)/0.5;
            else if (ch == AV_CHAN_BACK_LEFT || ch == AV_CHAN_BACK_RIGHT || ch == AV_CHAN_BACK_CENTER ||
                ch == AV_CHAN_TOP_BACK_LEFT || ch == AV_CHAN_TOP_BACK_CENTER || ch == AV_CHAN_TOP_BACK_RIGHT)
                yCoef *= m_currPanParams.y/0.5;
#endif
            double finalCoef = xCoef*yCoef;
            fgArgsOss << "c" << i << "=" << finalCoef << "*" << "c" << i;
            if (i < channels-1)
                fgArgsOss << " | ";
        }
        fgArgsOss << ",aformat=sample_fmts=" << av_get_sample_fmt_name(smpfmt);
        string fgArgs = fgArgsOss.str();
        m_logger->Log(DEBUG) << "Initialze PAN filter-graph with arguments '" << fgArgs << "'." << endl;
        fferr = avfilter_graph_parse_ptr(m_panFg, fgArgs.c_str(), &inputs, &outputs, nullptr);
        if (fferr < 0)
        {
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);
            ostringstream oss;
            oss << "FAILED to invoke 'avfilter_graph_parse_ptr' with arguments string '" << fgArgs << "'! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }

        fferr = avfilter_graph_config(m_panFg, nullptr);
        if (fferr < 0)
        {
            avfilter_inout_free(&outputs);
            avfilter_inout_free(&inputs);
            ostringstream oss;
            oss << "FAILED to invoke 'avfilter_graph_config'! fferr=" << fferr << ".";
            m_errMsg = oss.str();
            return false;
        }

        avfilter_inout_free(&outputs);
        avfilter_inout_free(&inputs);
        m_panBufsrcCtx = bufSrcCtx;
        m_panBufsinkCtx = bufSinkCtx;
        return true;
    }

    void ReleasePanFilterGraph()
    {
        if (m_panFg)
        {
            avfilter_graph_free(&m_panFg);
            m_panFg = nullptr;
        }
        m_panBufsrcCtx = nullptr;
        m_panBufsinkCtx = nullptr;
    }

    void UpdateFilterParameters()
    {
        int fferr;
        char cmdRes[256] = {0};
        // Check VolumeParams
        if (m_setMuted != m_currMuted)
        {
            m_logger->Log(DEBUG) << "Change muted state: " << m_setMuted << "." << endl;;
            m_currMuted = m_setMuted;
            if (HasFilter(VOLUME))
            {
                int fferr;
                char cmdArgs[32] = {0};
                if (m_currMuted)
                {
                    snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", 0.);
                    fferr = avfilter_graph_send_command(m_filterGraph, "volume", "volume", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                }
                else
                {
                    snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_currVolumeParams.volume);
                    fferr = avfilter_graph_send_command(m_filterGraph, "volume", "volume", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                }
                if (fferr < 0)
                    m_logger->Log(WARN) << "FAILED set muted state as " << m_currMuted << "! Set 'volume' param failed with returned fferr=" << fferr << "." << endl;
            }
        }
        if (!m_currMuted && m_setVolumeParams.volume != m_currVolumeParams.volume)
        {
            m_logger->Log(DEBUG) << "Change VolumeParams::volume: " << m_currVolumeParams.volume << " -> " << m_setVolumeParams.volume << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setVolumeParams.volume);
            fferr = avfilter_graph_send_command(m_filterGraph, "volume", "volume", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currVolumeParams.volume = m_setVolumeParams.volume;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "volume" << "', cmd='" << "volume"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        // Check CompressorParams
        if (m_setCompressorParams.threshold != m_currCompressorParams.threshold)
        {
            m_logger->Log(DEBUG) << "Change CompressorParams::threshold: " << m_currCompressorParams.threshold << " -> " << m_setCompressorParams.threshold << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setCompressorParams.threshold);
            fferr = avfilter_graph_send_command(m_filterGraph, "acompressor", "threshold", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currCompressorParams.threshold = m_setCompressorParams.threshold;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "acompressor" << "', cmd='" << "threshold"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setCompressorParams.ratio != m_currCompressorParams.ratio)
        {
            m_logger->Log(DEBUG) << "Change CompressorParams::ratio: " << m_currCompressorParams.ratio << " -> " << m_setCompressorParams.ratio << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setCompressorParams.ratio);
            fferr = avfilter_graph_send_command(m_filterGraph, "acompressor", "ratio", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currCompressorParams.ratio = m_setCompressorParams.ratio;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "acompressor" << "', cmd='" << "ratio"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setCompressorParams.knee != m_currCompressorParams.knee)
        {
            m_logger->Log(DEBUG) << "Change CompressorParams::knee: " << m_currCompressorParams.knee << " -> " << m_setCompressorParams.knee << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setCompressorParams.knee);
            fferr = avfilter_graph_send_command(m_filterGraph, "acompressor", "knee", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currCompressorParams.knee = m_setCompressorParams.knee;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "acompressor" << "', cmd='" << "knee"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setCompressorParams.mix != m_currCompressorParams.mix)
        {
            m_logger->Log(DEBUG) << "Change CompressorParams::mix: " << m_currCompressorParams.mix << " -> " << m_setCompressorParams.mix << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setCompressorParams.mix);
            fferr = avfilter_graph_send_command(m_filterGraph, "acompressor", "mix", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currCompressorParams.mix = m_setCompressorParams.mix;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "acompressor" << "', cmd='" << "mix"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setCompressorParams.attack != m_currCompressorParams.attack)
        {
            m_logger->Log(DEBUG) << "Change CompressorParams::attack: " << m_currCompressorParams.attack << " -> " << m_setCompressorParams.attack << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setCompressorParams.attack);
            fferr = avfilter_graph_send_command(m_filterGraph, "acompressor", "attack", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currCompressorParams.attack = m_setCompressorParams.attack;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "acompressor" << "', cmd='" << "attack"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setCompressorParams.release != m_currCompressorParams.release)
        {
            m_logger->Log(DEBUG) << "Change CompressorParams::release: " << m_currCompressorParams.release << " -> " << m_setCompressorParams.release << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setCompressorParams.release);
            fferr = avfilter_graph_send_command(m_filterGraph, "acompressor", "release", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currCompressorParams.release = m_setCompressorParams.release;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "acompressor" << "', cmd='" << "release"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setCompressorParams.makeup != m_currCompressorParams.makeup)
        {
            m_logger->Log(DEBUG) << "Change CompressorParams::makeup: " << m_currCompressorParams.makeup << " -> " << m_setCompressorParams.makeup << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setCompressorParams.makeup);
            fferr = avfilter_graph_send_command(m_filterGraph, "acompressor", "makeup", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currCompressorParams.makeup = m_setCompressorParams.makeup;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "acompressor" << "', cmd='" << "makeup"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setCompressorParams.levelIn != m_currCompressorParams.levelIn)
        {
            m_logger->Log(DEBUG) << "Change CompressorParams::level_sc: " << m_currCompressorParams.levelIn << " -> " << m_setCompressorParams.levelIn << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setCompressorParams.levelIn);
            fferr = avfilter_graph_send_command(m_filterGraph, "acompressor", "level_in", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currCompressorParams.levelIn = m_setCompressorParams.levelIn;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "acompressor" << "', cmd='" << "level_in"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        // Check EqualizerParams
        for (int i=0; i < m_currEqualizerParamsList.size(); i++)
        {
            auto &m_currEQLParams = m_currEqualizerParamsList[i];
            auto &m_setEQLParams = m_setEqualizerParamsList[i];
            if (m_setEQLParams.gain != m_currEQLParams.gain)
            {
                m_logger->Log(DEBUG) << "Change (CenterFreq@" << DF_CENTER_FREQS[i] << ") EqualizerParams::gain: " << m_currEQLParams.gain << " -> " << m_setEQLParams.gain << " ... ";
                char targetFilter[32] = {0};
                snprintf(targetFilter, sizeof(targetFilter)-1, "equalizer@%d", i);
                char cmdArgs[32] = {0};
                snprintf(cmdArgs, sizeof(cmdArgs)-1, "%d", m_setEQLParams.gain);
                fferr = avfilter_graph_send_command(m_filterGraph, targetFilter, "gain", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
                if (fferr >= 0)
                {
                    m_currEQLParams.gain = m_setEQLParams.gain;
                    m_logger->Log(DEBUG) << "Succeeded." << endl;
                }
                else
                {
                    m_logger->Log(DEBUG) << "FAILED!" << endl;
                    ostringstream oss;
                    oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "equalizer@" << i << "', cmd='" << "gain"
                        << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                    m_errMsg = oss.str();
                    m_logger->Log(WARN) << m_errMsg << endl;
                }
            }
        }
        // Check GateParams
        if (m_setGateParams.threshold != m_currGateParams.threshold)
        {
            m_logger->Log(DEBUG) << "Change GateParams::threshold: " << m_currGateParams.threshold << " -> " << m_setGateParams.threshold << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setGateParams.threshold);
            fferr = avfilter_graph_send_command(m_filterGraph, "agate", "threshold", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currGateParams.threshold = m_setGateParams.threshold;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "agate" << "', cmd='" << "threshold"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setGateParams.range != m_currGateParams.range)
        {
            m_logger->Log(DEBUG) << "Change GateParams::range: " << m_currGateParams.range << " -> " << m_setGateParams.range << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setGateParams.range);
            fferr = avfilter_graph_send_command(m_filterGraph, "agate", "range", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currGateParams.range = m_setGateParams.range;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "agate" << "', cmd='" << "range"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setGateParams.ratio != m_currGateParams.ratio)
        {
            m_logger->Log(DEBUG) << "Change GateParams::ratio: " << m_currGateParams.ratio << " -> " << m_setGateParams.ratio << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setGateParams.ratio);
            fferr = avfilter_graph_send_command(m_filterGraph, "agate", "ratio", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currGateParams.ratio = m_setGateParams.ratio;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "agate" << "', cmd='" << "ratio"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setGateParams.attack != m_currGateParams.attack)
        {
            m_logger->Log(DEBUG) << "Change GateParams::attack: " << m_currGateParams.attack << " -> " << m_setGateParams.attack << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setGateParams.attack);
            fferr = avfilter_graph_send_command(m_filterGraph, "agate", "attack", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currGateParams.attack = m_setGateParams.attack;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "agate" << "', cmd='" << "attack"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setGateParams.release != m_currGateParams.release)
        {
            m_logger->Log(DEBUG) << "Change GateParams::release: " << m_currGateParams.release << " -> " << m_setGateParams.release << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setGateParams.release);
            fferr = avfilter_graph_send_command(m_filterGraph, "agate", "release", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currGateParams.release = m_setGateParams.release;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "agate" << "', cmd='" << "release"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setGateParams.makeup != m_currGateParams.makeup)
        {
            m_logger->Log(DEBUG) << "Change GateParams::makeup: " << m_currGateParams.makeup << " -> " << m_setGateParams.makeup << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setGateParams.makeup);
            fferr = avfilter_graph_send_command(m_filterGraph, "agate", "makeup", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currGateParams.makeup = m_setGateParams.makeup;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "agate" << "', cmd='" << "makeup"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setGateParams.knee != m_currGateParams.knee)
        {
            m_logger->Log(DEBUG) << "Change GateParams::knee: " << m_currGateParams.knee << " -> " << m_setGateParams.knee << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setGateParams.knee);
            fferr = avfilter_graph_send_command(m_filterGraph, "agate", "knee", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currGateParams.knee = m_setGateParams.knee;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "agate" << "', cmd='" << "knee"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        // Check LimiterParams
        if (m_setLimiterParams.limit != m_currLimiterParams.limit)
        {
            m_logger->Log(DEBUG) << "Change LimiterParams::limit: " << m_currLimiterParams.limit << " -> " << m_setLimiterParams.limit << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setLimiterParams.limit);
            fferr = avfilter_graph_send_command(m_filterGraph, "alimiter", "limit", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currLimiterParams.limit = m_setLimiterParams.limit;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "alimiter" << "', cmd='" << "limit"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setLimiterParams.attack != m_currLimiterParams.attack)
        {
            m_logger->Log(DEBUG) << "Change LimiterParams::attack: " << m_currLimiterParams.attack << " -> " << m_setLimiterParams.attack << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setLimiterParams.attack);
            fferr = avfilter_graph_send_command(m_filterGraph, "alimiter", "attack", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currLimiterParams.attack = m_setLimiterParams.attack;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "alimiter" << "', cmd='" << "attack"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        if (m_setLimiterParams.release != m_currLimiterParams.release)
        {
            m_logger->Log(DEBUG) << "Change LimiterParams::release: " << m_currLimiterParams.release << " -> " << m_setLimiterParams.release << " ... ";
            char cmdArgs[32] = {0};
            snprintf(cmdArgs, sizeof(cmdArgs)-1, "%f", m_setLimiterParams.release);
            fferr = avfilter_graph_send_command(m_filterGraph, "alimiter", "release", cmdArgs, cmdRes, sizeof(cmdRes)-1, 0);
            if (fferr >= 0)
            {
                m_currLimiterParams.release = m_setLimiterParams.release;
                m_logger->Log(DEBUG) << "Succeeded." << endl;
            }
            else
            {
                m_logger->Log(DEBUG) << "FAILED!" << endl;
                ostringstream oss;
                oss << "FAILED to invoke 'avfilter_graph_send_command()' with arguments: target='" << "alimiter" << "', cmd='" << "release"
                    << "', arg='" << cmdArgs << "'. Returned fferr=" << fferr << ", res='" << cmdRes << "'.";
                m_errMsg = oss.str();
                m_logger->Log(WARN) << m_errMsg << endl;
            }
        }
        // Check PanParams
        if (m_setPanParams.x != m_currPanParams.x || m_setPanParams.y != m_currPanParams.y)
        {
            m_logger->Log(DEBUG) << "Change PanParams (" << m_currPanParams.x << ", " << m_currPanParams.y << ") -> (" << m_setPanParams.x << ", " << m_setPanParams.y << ")." << endl;
            m_currPanParams = m_setPanParams;
            ReleasePanFilterGraph();
            if (!CreatePanFilterGraph(m_smpfmt, m_channels, m_sampleRate))
            {
                m_logger->Log(Error) << "FAILED to re-create PAN filter-graph during updating the parameters! Error is '" << m_errMsg << "'." << endl;
                m_usePanFg = false;
            }
        }
    }

    bool CheckFilters(uint32_t composeFlags, uint32_t checkFlags) const
    {
        return (composeFlags&checkFlags) == checkFlags;
    }

private:
    ALogger* m_logger;
    uint32_t m_composeFlags{0};
    bool m_inited{false};
    bool m_passThrough{false};
    AVSampleFormat m_smpfmt{AV_SAMPLE_FMT_NONE};
    ImDataType m_matDt;
    uint32_t m_channels{0};
    uint32_t m_sampleRate{0};
    uint32_t m_blockAlign{0};
    bool m_isPlanar{false};
    bool m_useGeneralFg{false};
    AVFilterGraph* m_filterGraph{nullptr};
    AVFilterContext* m_bufsrcCtx{nullptr};
    AVFilterContext* m_bufsinkCtx{nullptr};
    bool m_usePanFg{false};
    AVFilterGraph* m_panFg{nullptr};
    AVFilterContext* m_panBufsrcCtx{nullptr};
    AVFilterContext* m_panBufsinkCtx{nullptr};

    static const std::vector<uint32_t> DF_CENTER_FREQS;
    static const std::vector<uint32_t> DF_BAND_WTHS;

    VolumeParams m_setVolumeParams, m_currVolumeParams;
    PanParams m_setPanParams, m_currPanParams;
    LimiterParams m_setLimiterParams, m_currLimiterParams;
    GateParams m_setGateParams, m_currGateParams;
    CompressorParams m_setCompressorParams, m_currCompressorParams;
    std::vector<EqualizerParams> m_setEqualizerParamsList, m_currEqualizerParamsList;
    bool m_setMuted{false}, m_currMuted{false};

    AudioImMatAVFrameConverter m_matCvter;
    string m_errMsg;
};

const std::vector<uint32_t> AudioEffectFilter_FFImpl::DF_CENTER_FREQS = {
    32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000
};
const std::vector<uint32_t> AudioEffectFilter_FFImpl::DF_BAND_WTHS = {
    32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000
};

const uint32_t AudioEffectFilter::VOLUME        = 0x1;
const uint32_t AudioEffectFilter::PAN           = 0x2;
const uint32_t AudioEffectFilter::LIMITER       = 0x4;
const uint32_t AudioEffectFilter::GATE          = 0x8;
const uint32_t AudioEffectFilter::EQUALIZER     = 0x10;
const uint32_t AudioEffectFilter::COMPRESSOR    = 0x20;

static const auto AUDIO_EFFECT_FILTER_HOLDER_DELETER = [] (AudioEffectFilter* p) {
    AudioEffectFilter_FFImpl* ptr = dynamic_cast<AudioEffectFilter_FFImpl*>(p);
    delete ptr;
};

AudioEffectFilter::Holder AudioEffectFilter::CreateInstance(const string& loggerName)
{
    return AudioEffectFilter::Holder(new AudioEffectFilter_FFImpl(loggerName), AUDIO_EFFECT_FILTER_HOLDER_DELETER);
}

ALogger* AudioEffectFilter::GetLogger()
{
    return Logger::GetLogger("AEFilter");
}
}
