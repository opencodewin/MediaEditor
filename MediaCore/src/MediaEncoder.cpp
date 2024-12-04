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

#include <mutex>
#include <thread>
#include <chrono>
#include <sstream>
#include <list>
#include <algorithm>
#include "MediaEncoder.h"
#include "FFUtils.h"
#include "FileSystemUtils.h"
#include "ThreadUtils.h"
extern "C"
{
    #include "libavutil/avutil.h"
    #include "libavutil/avstring.h"
    #include "libavutil/pixdesc.h"
    #include "libavformat/avformat.h"
    #include "libavcodec/avcodec.h"
    #include "libavdevice/avdevice.h"
    #include "libavfilter/avfilter.h"
    #include "libavfilter/buffersrc.h"
    #include "libavfilter/buffersink.h"
    #include "libswscale/swscale.h"
    #include "libswresample/swresample.h"
}

using namespace std;
using namespace Logger;

namespace MediaCore
{
class MediaEncoder_Impl : public MediaEncoder
{
public:
    MediaEncoder_Impl()
    {
        m_logger = MediaEncoder::GetLogger();
    }

    MediaEncoder_Impl(const MediaEncoder_Impl&) = delete;
    MediaEncoder_Impl(MediaEncoder_Impl&&) = delete;
    MediaEncoder_Impl& operator=(const MediaEncoder_Impl&) = delete;

    virtual ~MediaEncoder_Impl() {}

    bool Open(const string& url) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_opened)
            Close();

        if (!Open_Internal(url))
        {
            Close();
            return false;
        }

        m_opened = true;
        return true;
    }

    bool Close() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        int fferr;
        bool success = true;

        TerminateAllThreads();
        FlushAllQueues();

        if (m_avfmtCtx)
        {
            if ((m_avfmtCtx->oformat->flags&AVFMT_NOFILE) == 0)
                avio_closep(&m_avfmtCtx->pb);
            avformat_free_context(m_avfmtCtx);
            m_avfmtCtx = nullptr;
        }

        CloseVideoComponents();
        CloseAudioComponents();

        m_muxEof = false;
        m_started = false;
        m_opened = false;

        return success;
    }

    bool ConfigureVideoStream(const std::string& codecName,
            string& imageFormat, uint32_t width, uint32_t height,
            const Ratio& frameRate, uint64_t bitRate,
            vector<Option>* extraOpts = nullptr) override
    {
        if (!m_opened)
        {
            m_errMsg = "This MediaEncoder has NOT opened yet!";
            return false;
        }
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_started)
        {
            m_errMsg = "This MediaEncoder already started!";
            return false;
        }

        if (ConfigureVideoStream_Internal(codecName, imageFormat, width, height, frameRate, bitRate, extraOpts))
        {
            m_vidStmIdx = m_vidAvStm->index;
            imageFormat = av_get_pix_fmt_name(m_videncPixfmt);
        }
        else
        {
            CloseVideoComponents();
        }

        return m_vidStmIdx < 0 ? false : true;
    }

    bool ConfigureAudioStream(const std::string& codecName,
            string& sampleFormat, uint32_t channels, uint32_t sampleRate, uint64_t bitRate) override
    {
        if (!m_opened)
        {
            m_errMsg = "This MediaEncoder has NOT opened yet!";
            return false;
        }
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_started)
        {
            m_errMsg = "This MediaEncoder already started!";
            return false;
        }

        if (ConfigureAudioStream_Internal(codecName, sampleFormat, channels, sampleRate, bitRate))
        {
            m_audStmIdx = m_audAvStm->index;
            sampleFormat = av_get_sample_fmt_name(m_audencSmpfmt);
        }
        else
        {
            CloseAudioComponents();
        }

        return m_audStmIdx < 0 ? false : true;
    }

    bool Start() override
    {
        if (!m_opened)
        {
            m_errMsg = "This MediaEncoder has NOT opened yet!";
            return false;
        }
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_started)
        {
            m_errMsg = "This MediaEncoder already started!";
            return false;
        }
        if (!HasVideo() && !HasAudio())
        {
            m_errMsg = "No video nor audio stream has been added!";
            return false;
        }
        if (!HasVideo())
            m_videncEof = true;
        if (!HasAudio())
            m_audencEof = true;

        int fferr = avformat_write_header(m_avfmtCtx, nullptr);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avformat_write_header", fferr);
            return false;
        }

        StartAllThreads();
        m_started = true;
        return true;
    }

    bool FinishEncoding() override
    {
        if (!m_opened)
        {
            m_errMsg = "This MediaEncoder has NOT opened yet!";
            return false;
        }
        if (!m_started)
        {
            m_errMsg = "This MediaEncoder has NOT started yet!";
            return false;
        }
        lock_guard<recursive_mutex> lk(m_apiLock);

        if (HasVideo())
            m_vidinpEof = true;
        if (HasAudio())
            m_audinpEof = true;
        while (!m_muxEof)
            this_thread::sleep_for(chrono::milliseconds(5));

        bool success = true;
        int fferr;
        if (m_avfmtCtx)
        {
            fferr = av_write_trailer(m_avfmtCtx);
            if (fferr < 0)
            {
                m_errMsg = FFapiFailureMessage("av_write_trailer", fferr);
                success = false;
            }
            if ((m_avfmtCtx->oformat->flags&AVFMT_NOFILE) == 0)
                avio_closep(&m_avfmtCtx->pb);
            avformat_free_context(m_avfmtCtx);
            m_avfmtCtx = nullptr;
        }

        return success;
    }

    bool EncodeVideoFrame(VideoFrame::Holder hVfrm, bool& consumed, bool wait) override
    {
        consumed = false;
        if (!m_opened)
        {
            m_errMsg = "This MediaEncoder has NOT opened yet!";
            return false;
        }
        if (!m_started)
        {
            m_errMsg = "This MediaEncoder has NOT started yet!";
            return false;
        }
        if (!HasVideo())
        {
            m_errMsg = "This MediaEncoder does NOT have video!";
            return false;
        }
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_vidinpEof)
        {
            m_errMsg = "Video stream has already reaches EOF!";
            return false;
        }
        if (m_encErr)
        {
            return false;
        }

        if (!hVfrm)
        {
            m_vidinpEof = true;
            return true;
        }

        while (wait && m_vfrmQ.size() >= m_vmatQMaxSize && !m_quit)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_quit)
            return false;
        if (m_vfrmQ.size() >= m_vmatQMaxSize)
            return true;

        {
            lock_guard<mutex> lk(m_vmatQLock);
            m_vfrmQ.push_back(hVfrm);
            consumed = true;
        }
        return true;
    }

    bool EncodeVideoFrame(ImGui::ImMat& vmat, bool& consumed, bool wait) override
    {
        VideoFrame::Holder hVfrm;
        if (!vmat.empty())
            hVfrm = VideoFrame::CreateMatInstance(vmat);
        return EncodeVideoFrame(hVfrm, consumed, wait);
    }

    bool EncodeAudioSamples(uint8_t* buf, uint32_t size, bool& consumed, bool wait) override
    {
        consumed = false;
        if (!m_opened)
        {
            m_errMsg = "This MediaEncoder has NOT opened yet!";
            return false;
        }
        if (!m_started)
        {
            m_errMsg = "This MediaEncoder has NOT started yet!";
            return false;
        }
        if (!HasAudio())
        {
            m_errMsg = "This MediaEncoder does NOT have video!";
            return false;
        }
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_audinpEof)
        {
            m_errMsg = "Audio stream has already reaches EOF!";
            return false;
        }
        if (m_encErr)
        {
            return false;
        }

        if (!buf)
        {
            if (m_audencfrm)
            {
                uint32_t bufoffset = m_audencfrmSmpOffset*m_audinpFrameSize;
                memset(m_audencfrm->data[0]+bufoffset, 0, m_audencfrm->linesize[0]-bufoffset);
                {
                    lock_guard<mutex> lk(m_audfrmQLock);
                    m_audfrmQ.push_back(m_audencfrm);
                }
                m_audencfrm = nullptr;
            }
            m_audinpEof = true;
            return true;
        }

        if (m_audfrmQ.size() >= m_audfrmQMaxSize)
        {
            if (!wait)
                return true;
            while (m_audfrmQ.size() >= m_audfrmQMaxSize && !m_quit)
                this_thread::sleep_for(chrono::milliseconds(5));
            if (m_quit)
                return false;
        }
        uint32_t inpSamples = (uint32_t)(size/m_audinpFrameSize);
        if (inpSamples*m_audinpFrameSize != size)
        {
            m_logger->Log(WARN) << "Input audio data size " << size << " is NOT an integral multiply of input-frame-size " << m_audinpFrameSize << "!" << endl;
            size = inpSamples*m_audinpFrameSize;
        }

        int fferr;
        uint32_t readSize = 0;
        uint32_t bufferedSamples = 0;
        const uint8_t* inpbuf = buf;
        while ((readSize < size || bufferedSamples > 0) && !m_quit)
        {
            if (!m_audencfrm)
            {
                m_audencfrm = AllocSelfFreeAVFramePtr();
                if (!m_audencfrm)
                {
                    m_errMsg = "FAILED allocate new AVFrame for audio input frame!";
                    return false;
                }
                m_audencfrm->format = m_audencSmpfmt;
                m_audencfrm->sample_rate = m_audencCtx->sample_rate;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
                m_audencfrm->channels = m_audencCtx->channels;
                m_audencfrm->channel_layout = m_audencCtx->channel_layout;
#else
                m_audencfrm->ch_layout = m_audencCtx->ch_layout;
#endif
                m_audencfrm->nb_samples = m_audencFrameSamples;
                fferr = av_frame_get_buffer(m_audencfrm.get(), 0);
                if (fferr < 0)
                {
                    m_errMsg = FFapiFailureMessage("av_frame_get_buffer(EncodeAudioSamples)", fferr);
                    return false;
                }
                m_audencfrm->pts = m_audfrmPts;
            }

            if (m_swrCtx)
            {
                fferr = swr_convert(m_swrCtx, m_audencfrm->data, m_audencfrm->nb_samples-m_audencfrmSmpOffset, &inpbuf, inpSamples);
                if (fferr <= 0)
                {
                    m_errMsg = FFapiFailureMessage("swr_convert", fferr);
                    return false;
                }
                readSize += inpSamples*m_audinpFrameSize;
                m_audencfrmSmpOffset += fferr;
                inpbuf = nullptr;
                inpSamples = 0;
                bufferedSamples = swr_get_out_samples(m_swrCtx, 0);
            }
            else
            {
                uint32_t copySize = (m_audencfrm->nb_samples-m_audencfrmSmpOffset)*m_audinpFrameSize;
                if (copySize > size-readSize)
                    copySize = size-readSize;
                uint32_t bufoffset = m_audencfrmSmpOffset*m_audinpFrameSize;
                memcpy(m_audencfrm->data[0]+bufoffset, inpbuf, copySize);
                m_audencfrmSmpOffset += (uint32_t)(copySize/m_audinpFrameSize);
                readSize += copySize;
            }

            if (m_audencfrmSmpOffset >= m_audencfrm->nb_samples)
            {
                lock_guard<mutex> lk(m_audfrmQLock);
                m_audfrmQ.push_back(m_audencfrm);
                m_audfrmPts += m_audencfrm->nb_samples;
                m_audencfrm = nullptr;
                m_audencfrmSmpOffset = 0;
            }
        }
        consumed = true;
        if (m_quit)
            return false;

        return true;
    }

    bool EncodeAudioSamples(ImGui::ImMat& amat, bool& consumed, bool wait) override
    {
        return EncodeAudioSamples((uint8_t*)amat.data, amat.total()*amat.elemsize, consumed, wait);
    }

    bool IsOpened() const override
    {
        return m_opened;
    }

    bool HasVideo() const override
    {
        return m_vidStmIdx >= 0;
    }

    bool HasAudio() const override
    {
        return m_audStmIdx >= 0;
    }

    Ratio GetVideoFrameRate() const override
    {
        if (!m_videncCtx)
            return {0, 0};
        return {m_videncCtx->framerate.num, m_videncCtx->framerate.den};
    }

    bool IsHwAccelEnabled() const override
    {
        return m_vidPreferUseHw;
    }

    void EnableHwAccel(bool enable) override
    {
        m_vidPreferUseHw = enable;
    }

    string GetError() const override
    {
        return m_errMsg;
    }

    bool CheckHwPixFmt(AVPixelFormat pixfmt)
    {
        return pixfmt == m_videncPixfmt;
    }

private:
    string FFapiFailureMessage(const string& apiName, int fferr)
    {
        ostringstream oss;
        oss << "FF api '" << apiName << "' returns error! fferr=" << fferr << ".";
        return oss.str();
    }

    bool Open_Internal(const string& url)
    {
        int fferr;

        fferr = avformat_alloc_output_context2(&m_avfmtCtx, nullptr, nullptr, url.c_str());
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avformat_alloc_output_context2", fferr);
            return false;
        }

        if ((m_avfmtCtx->oformat->flags&AVFMT_NOFILE) == 0)
        {
            fferr = avio_open(&m_avfmtCtx->pb, url.c_str(), AVIO_FLAG_WRITE);
            if (fferr == AVERROR(EACCES))
            {
                m_errMsg = "Target path '" + url + "' is NOT ACCESSIBLE!";
                return false;
            }
            else if (fferr < 0)
            {
                m_errMsg = FFapiFailureMessage("avio_open", fferr);
                return false;
            }
        }
        return true;
    }

    bool ConfigureVideoStream_Internal(const std::string& codecName,
            string& imageFormat, uint32_t width, uint32_t height,
            const Ratio& frameRate, uint64_t bitRate,
            vector<Option>* extraOpts)
    {
        AVPixelFormat requiredInputPixfmt = AV_PIX_FMT_NONE;
        if (!imageFormat.empty())
        {
            requiredInputPixfmt = av_get_pix_fmt(imageFormat.c_str());
            if (requiredInputPixfmt == AV_PIX_FMT_NONE)
            {
                ostringstream oss;
                oss << "INVALID image format '" << imageFormat << "'!";
                m_errMsg = oss.str();
                return false;
            }
        }

        const bool bGlobalHeader = (m_avfmtCtx->oformat->flags&AVFMT_GLOBALHEADER) != 0;
        AVCodecContext* pTempVidencCtx = nullptr;
        AVBufferRef* pTempHwDevCtx = nullptr;
        m_videnc = avcodec_find_encoder_by_name(codecName.c_str());
        if (m_videnc)
        {
            if (!OpenVideoEncoder(m_videnc, &pTempVidencCtx, &pTempHwDevCtx,
                    width, height, frameRate, bitRate, extraOpts,
                    requiredInputPixfmt, bGlobalHeader))
            {
                if (pTempVidencCtx)
                {
                    avcodec_free_context(&pTempVidencCtx);
                    pTempVidencCtx = nullptr;
                }
                if (pTempHwDevCtx)
                {
                    av_buffer_unref(&pTempHwDevCtx);
                    pTempHwDevCtx = nullptr;
                }
            }
        }
        else
        {
            AVCodecContext* pSwVidencCtx = nullptr;
            AVCodecContext* pHwVidencCtx = nullptr;
            const AVCodecDescriptor* desc = avcodec_descriptor_get_by_name(codecName.c_str());
            if (desc)
            {
                void* i = 0;
                AVCodecPtr p;
                while ((p = (AVCodecPtr)av_codec_iterate(&i)))
                {
                    if (p->id != desc->id || p->type != AVMEDIA_TYPE_VIDEO ||
                        !av_codec_is_encoder(p) || (p->capabilities&AV_CODEC_CAP_EXPERIMENTAL))
                        continue;
                    if (p->capabilities&AV_CODEC_CAP_HARDWARE)
                    {
                        if (!pHwVidencCtx)
                        {
                            if (OpenVideoEncoder(p, &pTempVidencCtx, &pTempHwDevCtx,
                                    width, height, frameRate, bitRate, extraOpts,
                                    requiredInputPixfmt, bGlobalHeader))
                            {
                                pHwVidencCtx = pTempVidencCtx;
                            }
                            else
                            {
                                if (pTempVidencCtx)
                                {
                                    avcodec_free_context(&pTempVidencCtx);
                                    pTempVidencCtx = nullptr;
                                }
                                if (pTempHwDevCtx)
                                {
                                    av_buffer_unref(&pTempHwDevCtx);
                                    pTempHwDevCtx = nullptr;
                                }
                            }
                        }
                    }
                    else if (!pSwVidencCtx)
                    {
                        if (OpenVideoEncoder(p, &pTempVidencCtx, nullptr,
                                width, height, frameRate, bitRate, extraOpts,
                                requiredInputPixfmt, bGlobalHeader))
                        {
                            pSwVidencCtx = pTempVidencCtx;
                        }
                        else
                        {
                            if (pTempVidencCtx)
                            {
                                avcodec_free_context(&pTempVidencCtx);
                                pTempVidencCtx = nullptr;
                            }
                        }
                    }

                    bool encoderChosen = false;
                    if (m_vidPreferUseHw && pHwVidencCtx)
                    {
                        pTempVidencCtx = pHwVidencCtx;
                        encoderChosen = true;
                    }
                    else if (!m_vidPreferUseHw && pSwVidencCtx)
                    {
                        pTempVidencCtx = pSwVidencCtx;
                        encoderChosen = true;
                    }
                    if (encoderChosen)
                        break;
                }
                if (!pTempVidencCtx)
                {
                    if (pHwVidencCtx)
                        pTempVidencCtx = pHwVidencCtx;
                    else if (pSwVidencCtx)
                        pTempVidencCtx = pSwVidencCtx;
                }
                // free unused encoder context
                if (pHwVidencCtx && pHwVidencCtx != pTempVidencCtx)
                {
                    if (pHwVidencCtx)
                    {
                        avcodec_free_context(&pHwVidencCtx);
                        pHwVidencCtx = nullptr;
                    }
                }
                if (pSwVidencCtx && pSwVidencCtx != pTempVidencCtx)
                {
                    if (pSwVidencCtx)
                    {
                        avcodec_free_context(&pSwVidencCtx);
                        pSwVidencCtx = nullptr;
                    }
                }
            }
        }
        if (!pTempVidencCtx)
        {
            ostringstream oss;
            oss << "NO encoder can be found from hint '" << codecName << "'!";
            m_errMsg = oss.str();
            return false;
        }
        m_videncCtx = pTempVidencCtx;
        m_videncPixfmt = m_videncCtx->pix_fmt;
        if (m_videncCtx->hw_frames_ctx)
        {
            AVHWFramesContext* pHwFrmCtx = (AVHWFramesContext*)(m_videncCtx->hw_frames_ctx->data);
            m_videncPixfmt = pHwFrmCtx->sw_format;
        }
        m_videnc = (AVCodec *)m_videncCtx->codec;
        m_logger->Log(DEBUG) << "Choose to use video encoder '" << m_videnc->name << "'." << endl;
        m_logger->Log(DEBUG) << "Choose to use encoding pixel-format '" << av_get_pix_fmt_name(m_videncPixfmt) << "'." << endl;

        m_imgCvter.SetUseVulkanConverter(true);
        if (!m_imgCvter.SetOutSize(width, height))
        {
            ostringstream oss;
            oss << "FAILED to set 'ImMatToAVFrameConverter' with out-size " << width << "x" << height << "!";
            m_errMsg = oss.str();
            return false;
        }
        if (!m_imgCvter.SetOutPixelFormat(m_videncPixfmt))
        {
            ostringstream oss;
            const char* name = av_get_pix_fmt_name(m_videncPixfmt);
            oss << "FAILED to set 'ImMatToAVFrameConverter' with pixel-format '" << (name ? name : "(null)") << "'!";
            m_errMsg = oss.str();
            return false;
        }

        m_vmatQMaxSize = (uint32_t)(((double)m_videncCtx->framerate.num/m_videncCtx->framerate.den)*m_dataQCacheDur);
        if (m_vmatQMaxSize < 2)
            m_vmatQMaxSize = 2;

        m_vidAvStm = avformat_new_stream(m_avfmtCtx, m_videnc);
        if (!m_vidAvStm)
        {
            m_errMsg = "FAILED to create new stream by 'avformat_new_stream'!";
            return false;
        }
        m_vidAvStm->id = m_avfmtCtx->nb_streams-1;
        m_vidAvStm->time_base = m_videncCtx->time_base;
        m_vidAvStm->avg_frame_rate = m_videncCtx->framerate;
        avcodec_parameters_from_context(m_vidAvStm->codecpar, m_videncCtx);

        return true;
    }

    bool OpenVideoEncoder(AVCodecPtr videnc, AVCodecContext** ppVidencCtx, AVBufferRef** ppHwDevCtx,
            uint32_t width, uint32_t height, const Ratio& frameRate, uint64_t bitRate,
            vector<Option>* extraOpts, AVPixelFormat requiredInputPixfmt, bool bGlobalHeader)
    {
        if (ppHwDevCtx) *ppHwDevCtx = nullptr;
        *ppVidencCtx = nullptr;

        AVPixelFormat videncPixfmt = AV_PIX_FMT_NONE;
        if (videnc->pix_fmts)
        {
            if (requiredInputPixfmt == AV_PIX_FMT_NONE)
            {
                bool isHwEncoder = (videnc->capabilities&AV_CODEC_CAP_HARDWARE) != 0;
                for (int i = 0; ; i++)
                {
                    AVPixelFormat pixfmt = videnc->pix_fmts[i];
                    if (pixfmt == AV_PIX_FMT_NONE)
                        break;
                    const AVPixFmtDescriptor* pixdesc = av_pix_fmt_desc_get(pixfmt);
                    if (!pixdesc)
                        continue;
                    bool isHwPixfmt = (pixdesc->flags&AV_PIX_FMT_FLAG_HWACCEL) != 0;
                    if (!isHwEncoder && isHwPixfmt)
                        continue;
                    videncPixfmt = pixfmt;
                    break;
                }
            }
            else
            {
                bool isFmtSupport = false;
                for (int i = 0; ; i++)
                {
                    if (videnc->pix_fmts[i] == AV_PIX_FMT_NONE)
                        break;
                    else if (videnc->pix_fmts[i] == requiredInputPixfmt)
                    {
                        isFmtSupport = true;
                        break;
                    }
                }
                if (isFmtSupport)
                    videncPixfmt = requiredInputPixfmt;
                else
                {
                    ostringstream oss;
                    oss << "Required input pixel-format '" << av_get_pix_fmt_name(requiredInputPixfmt) << "' is NOT SUPPORTED by encoder '"
                        << videnc->name << "'!";
                    m_errMsg = oss.str();
                    m_logger->Log(DEBUG) << m_errMsg << endl;
                    return false;
                }
            }
        }
        if (videncPixfmt == AV_PIX_FMT_NONE)
            videncPixfmt = requiredInputPixfmt!=AV_PIX_FMT_NONE ? requiredInputPixfmt : AV_PIX_FMT_YUV420P;

        *ppVidencCtx = avcodec_alloc_context3(videnc);
        if (!(*ppVidencCtx))
        {
            m_errMsg = "FAILED to allocate AVCodecContext by 'avcodec_alloc_context3'!";
            return false;
        }

        int fferr;
        if (videnc->capabilities&AV_CODEC_CAP_HARDWARE)
        {
            const AVCodecHWConfig* config = nullptr;
            for (int i = 0; ; i++)
            {
                config = avcodec_get_hw_config(videnc, i);
                if (!config)
                {
                    ostringstream oss;
                    oss << "Encoder '" << videnc->name << "' has NO MORE hardware configuration! End index is " << i << ".";
                    m_errMsg = oss.str();
                    m_logger->Log(DEBUG) << m_errMsg << endl;
                    return false;
                }
                auto pStrLabel = av_hwdevice_get_type_name(config->device_type);
                const string strHwTypeName = pStrLabel ? string(pStrLabel) : "(null)";
                pStrLabel = av_get_pix_fmt_name(config->pix_fmt);
                const string strPixfmtName = pStrLabel ? string(pStrLabel) : "(null)";
                m_logger->Log(DEBUG) << "Checking encoder '" << videnc->name << "' hw_config[" << i << "] : methods=" << config->methods
                        << ", device_type=" << strHwTypeName << ", pix_fmt=" << strPixfmtName << "." << endl;
                const auto& hwMethods = config->methods;
                if (hwMethods&AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
                {
                    if (m_vidUseHwType != AV_HWDEVICE_TYPE_NONE && m_vidUseHwType != config->device_type)
                        m_logger->Log(DEBUG) << "Will NOT use encoder '" << videnc->name << "' because this encoder has hardware type '"
                            << av_hwdevice_get_type_name(config->device_type) << "', but configuration is set to use '" << av_hwdevice_get_type_name(m_vidUseHwType)
                            << "' ONLY." << endl;
                    else
                        break;
                }
                else if (hwMethods&AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX)
                {
                    break;
                }
            }

            auto pStrLabel = av_hwdevice_get_type_name(config->device_type);
            const string strHwTypeName = pStrLabel ? string(pStrLabel) : "(null)";
            if ((fferr = av_hwdevice_ctx_create(ppHwDevCtx, config->device_type, nullptr, nullptr, 0)) < 0)
            {
                ostringstream oss; oss << FFapiFailureMessage("av_hwdevice_ctx_create", fferr) << " Device type is '" << strHwTypeName << "'.";
                m_errMsg = oss.str();
                m_logger->Log(DEBUG) << m_errMsg << endl;
                return false;
            }
            if (config->pix_fmt != AV_PIX_FMT_NONE)
                videncPixfmt = config->pix_fmt;
            if (config->methods&AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
            {
                (*ppVidencCtx)->hw_device_ctx = *ppHwDevCtx;
                *ppHwDevCtx = nullptr;
            }
            else if (config->methods&AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX)
            {
                (*ppVidencCtx)->hw_device_ctx = *ppHwDevCtx;
                AVBufferRef* pHwFrmRef = av_hwframe_ctx_alloc(*ppHwDevCtx);
                *ppHwDevCtx = nullptr;
                if (!pHwFrmRef)
                {
                    ostringstream oss; oss << "FAILED to allocate hardware frame context (by 'av_hwframe_ctx_alloc()') for hardware type '" << strHwTypeName << "'!";
                    m_errMsg = oss.str();
                    m_logger->Log(DEBUG) << m_errMsg << endl;
                    return false;
                }
                AVHWFramesContext* pHwFrmCtx = (AVHWFramesContext*)(pHwFrmRef->data);
                pHwFrmCtx->format = config->pix_fmt;
                pHwFrmCtx->sw_format = AV_PIX_FMT_NV12;
                pHwFrmCtx->width = width;
                pHwFrmCtx->height = height;
                pHwFrmCtx->initial_pool_size = 20;  // TODO: wyvern-> this value '20' is copied from ffmpeg example 'vaapi_encode.c'
                if ((fferr = av_hwframe_ctx_init(pHwFrmRef)) < 0)
                {
                    ostringstream oss; oss << "FAILED to invoke 'av_hwframe_ctx_init()' on hardware frame for hardware type '" << strHwTypeName << "'! fferr=" << fferr << ".";
                    m_errMsg = oss.str();
                    m_logger->Log(DEBUG) << m_errMsg << endl;
                    av_buffer_unref(&pHwFrmRef);
                    return false;
                }
                (*ppVidencCtx)->hw_frames_ctx = pHwFrmRef;
            }
        }

        (*ppVidencCtx)->pix_fmt = videncPixfmt;
        (*ppVidencCtx)->width = width;
        (*ppVidencCtx)->height = height;
        (*ppVidencCtx)->time_base = { frameRate.den, frameRate.num };
        (*ppVidencCtx)->framerate = { frameRate.num, frameRate.den };
        (*ppVidencCtx)->bit_rate = bitRate;
        (*ppVidencCtx)->sample_aspect_ratio = { 1, 1 };

        AVDictionary *encOpts = nullptr;
        if (extraOpts)
        {
            for (auto& extopt : *extraOpts)
            {
                ostringstream oss;
                oss << extopt.value;
                string optval = oss.str();
                av_dict_set(&encOpts, extopt.name.c_str(), optval.c_str(), 0);
            }
        }

        if (bGlobalHeader)
            (*ppVidencCtx)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; 
        fferr = avcodec_open2(*ppVidencCtx, videnc, &encOpts);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avcodec_open2", fferr);
            m_logger->Log(DEBUG) << "During opening encoder '" << videnc->name << "'. " << m_errMsg << endl;
            return false;
        }
        m_logger->Log(DEBUG) << "Successfully opened video encoder '" << videnc->name << "'." << endl;
        return true;
    }

    void CloseVideoComponents()
    {
        if (m_videncCtx)
        {
            avcodec_free_context(&m_videncCtx);
            m_videncCtx = nullptr;
        }
        m_videnc = nullptr;
        m_videncPixfmt = AV_PIX_FMT_NONE;
        m_vidAvStm = nullptr;
        m_vidStmIdx = -1;
        m_vidinpEof = false;
        m_videncEof = false;
    }

    bool ConfigureAudioStream_Internal(const std::string& codecName,
            string& sampleFormat, uint32_t channels, uint32_t sampleRate, uint64_t bitRate)
    {
        m_audenc = avcodec_find_encoder_by_name(codecName.c_str());
        if (!m_audenc)
        {
            ostringstream oss;
            oss << "Can NOT find encoder by name '" << codecName << "'!";
            m_errMsg = oss.str();
            return false;
        }
        else if (m_audenc->type != AVMEDIA_TYPE_AUDIO)
        {
            ostringstream oss;
            oss << "Codec name '" << codecName << "' is NOT for an AUDIO encoder!";
            m_errMsg = oss.str();
            return false;
        }

        if (!sampleFormat.empty())
        {
            m_audencSmpfmt = av_get_sample_fmt(sampleFormat.c_str());
            if (m_audencSmpfmt < 0)
            {
                ostringstream oss;
                oss << "INVALID sample format '" << sampleFormat << "'!";
                m_errMsg = oss.str();
                return false;
            }
        }
        else
        {
            if (m_audenc->sample_fmts)
                m_audencSmpfmt = m_audenc->sample_fmts[0];
            else
                m_audencSmpfmt = AV_SAMPLE_FMT_FLT;
        }

        m_audencCtx = avcodec_alloc_context3(m_audenc);
        if (!m_audencCtx)
        {
            m_errMsg = "FAILED to allocate AVCodecContext by 'avcodec_alloc_context3'!";
            return false;
        }

        m_audencCtx->sample_fmt = m_audencSmpfmt;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        m_audencCtx->channels = channels;
        m_audencCtx->channel_layout = av_get_default_channel_layout(channels);
#else
        av_channel_layout_default(&m_audencCtx->ch_layout, channels);
#endif
        m_audencCtx->sample_rate = sampleRate;
        m_audencCtx->bit_rate = bitRate;
        m_audencCtx->time_base = { 1, (int)sampleRate };

        int fferr;
        fferr = avcodec_open2(m_audencCtx, m_audenc, nullptr);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avcodec_open2", fferr);
            return false;
        }
        m_logger->Log(DEBUG) << "Audio encoder '" << m_audenc->name << "' is opened." << endl;


        if (m_audencCtx->frame_size > 0)
            m_audencFrameSamples = m_audencCtx->frame_size;
        else
            m_audencFrameSamples = 1024;
        m_audinpFrameSize = av_get_bytes_per_sample(m_audinpSmpfmt)*channels;
        m_audencFrameSize = av_get_bytes_per_sample(m_audencSmpfmt)*channels;

        m_audfrmQMaxSize = (uint32_t)(m_dataQCacheDur*sampleRate/m_audencFrameSamples);
        if (m_audfrmQMaxSize < 2)
            m_audfrmQMaxSize = 2;

        m_audAvStm = avformat_new_stream(m_avfmtCtx, m_audenc);
        if (!m_audAvStm)
        {
            m_errMsg = "FAILED to create new stream by 'avformat_new_stream'!";
            return false;
        }
        m_audAvStm->id = m_avfmtCtx->nb_streams-1;
        if ((m_avfmtCtx->oformat->flags&AVFMT_GLOBALHEADER) != 0)
            m_audencCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; 
        m_audAvStm->time_base = m_audencCtx->time_base;
        avcodec_parameters_from_context(m_audAvStm->codecpar, m_audencCtx);

        if (m_audinpSmpfmt != m_audencSmpfmt)
        {
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
            m_swrCtx = swr_alloc_set_opts(nullptr, m_audencCtx->channel_layout, m_audencSmpfmt, m_audencCtx->sample_rate,
                m_audencCtx->channel_layout, m_audinpSmpfmt, m_audencCtx->sample_rate, 0, nullptr);
            if (!m_swrCtx)
#else
            fferr = swr_alloc_set_opts2(&m_swrCtx, &m_audencCtx->ch_layout, m_audencSmpfmt, m_audencCtx->sample_rate,
                &m_audencCtx->ch_layout, m_audinpSmpfmt, m_audencCtx->sample_rate, 0, nullptr);
            if (fferr < 0)
#endif
            {
                m_errMsg = "FAILED to setup SwrContext for audio input format conversion!";
                return false;
            }
            fferr = swr_init(m_swrCtx);
            if (fferr < 0)
            {
                m_errMsg = FFapiFailureMessage("swr_init", fferr);
                return false;
            }
        }
        else if (m_swrCtx)
        {
            swr_free(&m_swrCtx);
            m_swrCtx = nullptr;
        }

        return true;
    }

    void CloseAudioComponents()
    {
        if (m_audencCtx)
        {
            avcodec_free_context(&m_audencCtx);
            m_audencCtx = nullptr;
        }
        m_audenc = nullptr;
        m_audencSmpfmt = AV_SAMPLE_FMT_NONE;
        m_audfrmPts = 0;
        m_audencfrm = nullptr;
        m_audencfrmSmpOffset = 0;
        m_audAvStm = nullptr;
        m_audStmIdx = -1;
        m_audinpEof = false;
        m_audencEof = false;
        if (m_swrCtx)
        {
            swr_free(&m_swrCtx);
            m_swrCtx = nullptr;
        }
    }

    void StartAllThreads()
    {
        string fileName = SysUtils::ExtractFileName(m_avfmtCtx->url);
        ostringstream thnOss;
        m_quit = false;
        if (HasVideo())
        {
            m_videncThread = thread(&MediaEncoder_Impl::VideoEncodingThreadProc, this);
            thnOss << "EncVenc-" << fileName;
            SysUtils::SetThreadName(m_videncThread, thnOss.str());
        }
        if (HasAudio())
        {
            m_audencThread = thread(&MediaEncoder_Impl::AudioEncodingThreadProc, this);
            thnOss.str(""); thnOss << "EncAenc-" << fileName;
            SysUtils::SetThreadName(m_audencThread, thnOss.str());
        }
        m_muxThread = thread(&MediaEncoder_Impl::MuxingThreadProc, this);
        thnOss.str(""); thnOss << "EncMux-" << fileName;
        SysUtils::SetThreadName(m_muxThread, thnOss.str());
    }

    void TerminateAllThreads()
    {
        m_quit = true;
        if (m_videncThread.joinable())
            m_videncThread.join();
        if (m_audencThread.joinable())
            m_audencThread.join();
        if (m_muxThread.joinable())
            m_muxThread.join();
    }

    void FlushAllQueues()
    {
        m_vfrmQ.clear();
        m_audfrmQ.clear();
    }

    SelfFreeAVFramePtr ConvertImMatToAVFrame(ImGui::ImMat& vmat)
    {
        if (vmat.empty())
            return nullptr;
        SelfFreeAVFramePtr vfrm = AllocSelfFreeAVFramePtr();
        if (!vfrm)
        {
            m_logger->Log(Error) << "FAILED to allocate new 'SelfFreeAVFramePtr'!";
            return nullptr;
        }
        int64_t pts = av_rescale_q((int64_t)(vmat.time_stamp*1000), MILLISEC_TIMEBASE, m_videncCtx->time_base);
        m_imgCvter.ConvertImage(vmat, vfrm.get(), pts);
        return vfrm;
    }

    void VideoEncodingThreadProc()
    {
        m_logger->Log(DEBUG) << "Enter VideoEncodingThreadProc()..." << endl;

        VideoFrame::Holder hVfrm;
        SelfFreeAVFramePtr encfrm;
        m_vidNullFrameSent = false;
        while (!m_quit)
        {
            bool idleLoop = true;
            int fferr;

            if (!encfrm)
            {
                if (!m_vfrmQ.empty())
                {
                    {
                        lock_guard<mutex> lk(m_vmatQLock);
                        hVfrm = m_vfrmQ.front();
                        m_vfrmQ.pop_front();
                    }
                    auto tNatvieData = hVfrm->GetNativeData();
                    if (tNatvieData.eType == VideoFrame::NativeData::AVFRAME)
                        encfrm = CloneSelfFreeAVFramePtr((const AVFrame*)tNatvieData.pData);
                    else if (tNatvieData.eType == VideoFrame::NativeData::AVFRAME_HOLDER)
                        encfrm = *((SelfFreeAVFramePtr*)tNatvieData.pData);
                    else if (tNatvieData.eType == VideoFrame::NativeData::MAT)
                        encfrm = ConvertImMatToAVFrame(*((ImGui::ImMat*)tNatvieData.pData));
                    else
                        m_logger->Log(Error) << "UNSUPPORTED 'VideoFrame::NativeData::Type' " << (int)tNatvieData.eType << "!" << endl;
                    if (encfrm && encfrm->format != m_videncPixfmt)
                    {
                        ostringstream oss; oss << "INVALID encoding AVFrame pixel format, input frame has format " << encfrm->format << "(" << av_get_pix_fmt_name((AVPixelFormat)encfrm->format)
                                << "), while the required input format is " << m_videncPixfmt << "(" << av_get_pix_fmt_name(m_videncPixfmt) << ")!";
                        m_errMsg = oss.str();
                        throw runtime_error(m_errMsg);
                    }
                    if (m_videncCtx->hw_frames_ctx && m_videncCtx->pix_fmt != (AVPixelFormat)encfrm->format)
                    {
                        SelfFreeAVFramePtr hwfrm = AllocSelfFreeAVFramePtr();
                        if ((fferr = av_hwframe_get_buffer(m_videncCtx->hw_frames_ctx, hwfrm.get(), 0)) < 0)
                        {
                            stringstream oss; oss << "FAILED to allocate buffer for hardware frame, av_hwframe_get_buffer() returns " << fferr << "!";
                            m_errMsg = oss.str();
                            throw runtime_error(m_errMsg);
                        }
                        if ((fferr = av_hwframe_transfer_data(hwfrm.get(), encfrm.get(), 0)) < 0)
                        {
                            stringstream oss; oss << "FAILED to transfer data to hardware frame, av_hwframe_transfer_data() returns " << fferr << "!";
                            m_errMsg = oss.str();
                            throw runtime_error(m_errMsg);
                        }
                        av_frame_copy_props(hwfrm.get(), encfrm.get());
                        encfrm = hwfrm;
                    }
                }
                else if (m_vidinpEof)
                {
                    {
                        lock_guard<mutex> lk(m_videncLock);
                        avcodec_send_frame(m_videncCtx, NULL);
                        m_vidNullFrameSent = true;
                        // m_logger->Log(DEBUG) << "--> SEND NULL video frame!! fferr=" << fferr << endl;
                    }
                    if (fferr == 0)
                    {
                        m_logger->Log(DEBUG) << "Sent encode video EOF." << endl;
                        break;
                    }
                    else if (fferr != AVERROR(EAGAIN))
                    {
                        ostringstream oss; oss << "Video encoder ERROR! avcodec_send_frame(EOF) returns " << fferr << ".";
                        m_errMsg = oss.str();
                        m_logger->Log(Error) << m_errMsg << endl;
                        m_encErr = true;
                        break;
                    }
                }
            }

            if (encfrm)
            {
                {
                    lock_guard<mutex> lk(m_videncLock);
                    fferr = avcodec_send_frame(m_videncCtx, encfrm.get());
                    // m_logger->Log(DEBUG) << "--> Encode video frame, mts=" << av_rescale_q(encfrm->pts, m_videncCtx->time_base, MILLISEC_TIMEBASE) << ", fferr=" << fferr << endl;
                }
                if (fferr == 0)
                {
                    // m_logger->Log(DEBUG) << "Encode video frame at "
                    //     << MillisecToString(av_rescale_q(encfrm->pts, m_videncCtx->time_base, MILLISEC_TIMEBASE))
                    //     << "(" << encfrm->pts << ")." << endl;
                    encfrm = nullptr;
                    hVfrm = nullptr;
                    idleLoop = false;
                }
                else
                {
                    if (fferr != AVERROR(EAGAIN))
                    {
                        ostringstream oss; oss << "Video encoder ERROR! avcodec_send_frame() returns " << fferr << ".";
                        m_errMsg = oss.str();
                        m_logger->Log(Error) << m_errMsg << endl;
                        m_encErr = true;
                        break;
                    }
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(2));
        }

        m_logger->Log(DEBUG) << "Leave VideoEncodingThreadProc()." << endl;
    }

    void AudioEncodingThreadProc()
    {
        m_logger->Log(DEBUG) << "Enter AudioEncodingThreadProc()..." << endl;

        SelfFreeAVFramePtr encfrm;
        m_audNullFrameSent = false;
        while (!m_quit)
        {
            bool idleLoop = true;
            int fferr;

            if (!encfrm)
            {
                if (!m_audfrmQ.empty())
                {
                    lock_guard<mutex> lk(m_audfrmQLock);
                    encfrm = m_audfrmQ.front();
                    m_audfrmQ.pop_front();
                }
                else if (m_audinpEof)
                {
                    {
                        lock_guard<mutex> lk(m_audencLock);
                        fferr = avcodec_send_frame(m_audencCtx, NULL);
                        m_audNullFrameSent = true;
                        // m_logger->Log(DEBUG) << "================> SEND NULL audio frame!! fferr=" << fferr << endl;
                    }
                    if (fferr == 0)
                    {
                        m_logger->Log(DEBUG) << "Sent encode audio EOF." << endl;
                        break;
                    }
                    else if (fferr != AVERROR(EAGAIN))
                    {
                        ostringstream oss; oss << "Audio encoder ERROR! avcodec_send_frame(EOF) returns " << fferr << ".";
                        m_errMsg = oss.str();
                        m_logger->Log(Error) << m_errMsg << endl;
                        m_encErr = true;
                        break;
                    }
                }
            }

            if (encfrm)
            {
                int fferr;
                {
                    lock_guard<mutex> lk(m_audencLock);
                    fferr = avcodec_send_frame(m_audencCtx, encfrm.get());
                    // m_logger->Log(DEBUG) << "================> Encode audio frame, mts=" << av_rescale_q(encfrm->pts, m_audencCtx->time_base, MILLISEC_TIMEBASE) << ", fferr=" << fferr << endl;
                }
                if (fferr == 0)
                {
                    // m_logger->Log(DEBUG) << "Encode audio frame at "
                    //     << MillisecToString(av_rescale_q(encfrm->pts, m_audencCtx->time_base, MILLISEC_TIMEBASE))
                    //     << "(" << encfrm->pts << ")." << endl;
                    encfrm = nullptr;
                    idleLoop = false;
                }
                else if (fferr != AVERROR(EAGAIN))
                {
                    ostringstream oss; oss << "Audio encoder ERROR! avcodec_send_frame() returns " << fferr << ".";
                    m_errMsg = oss.str();
                    m_logger->Log(Error) << m_errMsg << endl;
                    m_encErr = true;
                    break;
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(2));
        }

        m_logger->Log(DEBUG) << "Leave AudioEncodingThreadProc()." << endl;
    }

    void MuxingThreadProc()
    {
        m_logger->Log(DEBUG) << "Enter MuxingThreadProc()..." << endl;

        AVPacket avpkt{0};
        bool avpktLoaded = false;
        int64_t vidposMts{0}, audposMts{0};
        while (!m_quit)
        {
            bool idleLoop = true;
            int fferr;

            // bool toRecvVidpkt = !m_videncEof && !avpktLoaded && (vidposMts <= audposMts || m_audencEof);
            // m_logger->Log(DEBUG) << "toRecvVidpkt=" << toRecvVidpkt << ", m_videncEof=" << m_videncEof << ", avpktLoaded=" << avpktLoaded << ", vidposMts=" << vidposMts << ", audposMts=" << audposMts << ", m_audencEof=" << m_audencEof << endl;
            if (!m_videncEof && !avpktLoaded && (vidposMts <= audposMts || m_audencEof))
            {
                {
                    lock_guard<mutex> lk(m_videncLock);
                    fferr = avcodec_receive_packet(m_videncCtx, &avpkt);
                    // m_logger->Log(DEBUG) << "\t\t\t--> Receive video packet, fferr=" << fferr << endl;
                }
                if (fferr == 0)
                {
                    avpkt.stream_index = m_vidStmIdx;
                    av_packet_rescale_ts(&avpkt, m_videncCtx->time_base, m_vidAvStm->time_base);
                    avpktLoaded = true;
                    idleLoop = false;
                    vidposMts = av_rescale_q(avpkt.pts, m_vidAvStm->time_base, MILLISEC_TIMEBASE);
                    m_logger->Log(DEBUG) << "Got VIDEO packet at " << MillisecToString(vidposMts) << "(" << avpkt.pts << ")." << endl;
                }
                else if (fferr == AVERROR_EOF)
                {
                    m_videncEof = true;
                    idleLoop = false;
                }
                else if (fferr != AVERROR(EAGAIN))
                {
                    m_logger->Log(Error) << "In muxing thread, video 'avcodec_receive_packet' FAILED with return code " << fferr << "!" << endl;
                    break;
                }
                else if (m_vidNullFrameSent)
                {
                    m_logger->Log(WARN) << "WRONG STATE! Video encoder still returns AVERROR(EAGAIN) after NULL frame is sent! Treat it as AVERROR_EOF received." << endl;
                    m_videncEof = true;
                    idleLoop = false;
                }
            }

            // bool toRecvAudpkt = !m_audencEof && !avpktLoaded && (audposMts <= vidposMts || m_videncEof);
            // m_logger->Log(DEBUG) << "toRecvAudpkt=" << toRecvAudpkt << ", m_audencEof=" << m_audencEof << ", avpktLoaded=" << avpktLoaded << ", vidposMts=" << vidposMts << ", audposMts=" << audposMts << ", m_videncEof=" << m_videncEof << endl;
            if (!m_audencEof && !avpktLoaded && (audposMts <= vidposMts || m_videncEof))
            {
                {
                    lock_guard<mutex> lk(m_audencLock);
                    fferr = avcodec_receive_packet(m_audencCtx, &avpkt);
                    // m_logger->Log(DEBUG) << "\t\t\t================> Receive audio packet, fferr=" << fferr << endl;
                }
                if (fferr == 0)
                {
                    avpkt.stream_index = m_audStmIdx;
                    av_packet_rescale_ts(&avpkt, m_audencCtx->time_base, m_audAvStm->time_base);
                    avpktLoaded = true;
                    idleLoop = false;
                    audposMts = av_rescale_q(avpkt.pts, m_audAvStm->time_base, MILLISEC_TIMEBASE);
                    m_logger->Log(DEBUG) << "Got AUDIO packet at " << MillisecToString(audposMts) << "(" << avpkt.pts << ")." << endl;
                }
                else if (fferr == AVERROR_EOF)
                {
                    m_audencEof = true;
                    idleLoop = false;
                }
                else if (fferr != AVERROR(EAGAIN))
                {
                    m_logger->Log(Error) << "In muxing thread, audio 'avcodec_receive_packet' FAILED with return code " << fferr << "!" << endl;
                    break;
                }
                else if (m_audNullFrameSent)
                {
                    m_logger->Log(WARN) << "WRONG STATE! Audio encoder still returns AVERROR(EAGAIN) after NULL frame is sent! Treat it as AVERROR_EOF received." << endl;
                    m_audencEof = true;
                    idleLoop = false;
                }
            }

            if (avpktLoaded)
            {
                fferr = av_interleaved_write_frame(m_avfmtCtx, &avpkt);
                if (fferr == 0)
                {
                    av_packet_unref(&avpkt);
                    avpktLoaded = false;
                    idleLoop = false;
                }
                else
                {
                    m_logger->Log(Error) << "'av_interleaved_write_frame' FAILED with return code " << fferr << "!" << endl;
                    break;
                }
            }
            else if (((!HasVideo() || m_videncEof) && (!HasAudio() || m_audencEof)) || m_encErr)
            {
                break;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(1));
        }

        m_muxEof = true;
        m_logger->Log(DEBUG) << "Leave MuxingThreadProc()." << endl;
    }

private:
    string m_errMsg;
    ALogger* m_logger;
    recursive_mutex m_apiLock;
    bool m_vidPreferUseHw{true};
    bool m_quit{false};
    bool m_opened{false};
    bool m_started{false};

    AVFormatContext* m_avfmtCtx{nullptr};
    int m_vidStmIdx{-1};
    int m_audStmIdx{-1};
    AVCodecPtr m_videnc{nullptr};
    AVCodecPtr m_audenc{nullptr};
    AVStream* m_vidAvStm{nullptr};
    AVStream* m_audAvStm{nullptr};
    AVCodecContext* m_videncCtx{nullptr};
    mutex m_videncLock;
    AVHWDeviceType m_vidUseHwType{AV_HWDEVICE_TYPE_NONE};
    AVPixelFormat m_videncPixfmt{AV_PIX_FMT_NONE};
    AVCodecContext* m_audencCtx{nullptr};
    mutex m_audencLock;
    uint32_t m_audencFrameSamples{0};
    uint32_t m_audinpFrameSize{0};
    uint32_t m_audencFrameSize{0};
    AVSampleFormat m_audencSmpfmt{AV_SAMPLE_FMT_NONE};
    AVSampleFormat m_audinpSmpfmt{AV_SAMPLE_FMT_FLT};
    int64_t m_audfrmPts{0};
    SelfFreeAVFramePtr m_audencfrm;
    uint32_t m_audencfrmSmpOffset{0};
    SwrContext* m_swrCtx{nullptr};

    ImMatToAVFrameConverter m_imgCvter;

    double m_dataQCacheDur{0.02};
    // video encoding thread
    thread m_videncThread;
    list<VideoFrame::Holder> m_vfrmQ;
    uint32_t m_vmatQMaxSize;
    mutex m_vmatQLock;
    bool m_vidinpEof{false};
    bool m_vidNullFrameSent{false};
    bool m_videncEof{false};
    // audio encoding thread
    thread m_audencThread;
    list<SelfFreeAVFramePtr> m_audfrmQ;
    uint32_t m_audfrmQMaxSize;
    mutex m_audfrmQLock;
    bool m_audinpEof{false};
    bool m_audNullFrameSent{false};
    bool m_audencEof{false};
    // muxing thread
    thread m_muxThread;
    list<AVPacket*> m_vidpktQ;
    mutex m_vidpktQLock;
    list<AVPacket*> m_audpktQ;
    mutex m_audpktQLock;
    bool m_muxEof{false};
    bool m_encErr{false};
};

static const auto MEDIA_ENCODER_HOLDER_DELETER = [] (MediaEncoder* p) {
    MediaEncoder_Impl* ptr = dynamic_cast<MediaEncoder_Impl*>(p);
    ptr->Close();
    delete ptr;
};

MediaEncoder::Holder MediaEncoder::CreateInstance()
{
    return MediaEncoder::Holder(new MediaEncoder_Impl(), MEDIA_ENCODER_HOLDER_DELETER);
}

ALogger* MediaEncoder::GetLogger()
{
    return Logger::GetLogger("MEncoder");
}

ostream& operator<<(ostream& os, const MediaEncoder::Option::EnumValue& enumval)
{
    os << enumval.value;
    if (!enumval.name.empty())
        os << "(" << enumval.name << ")";
    return os;
}

ostream& operator<<(ostream& os, const MediaEncoder::Option::Description& optdesc)
{
    os << optdesc.name;
    if (!optdesc.tag.empty())
        os << "(" << optdesc.tag << ")";
    os << " - ";
    if (optdesc.valueType == Value::VT_INT)
        os << "INT";
    else if (optdesc.valueType == Value::VT_DOUBLE)
        os << "DOUBLE";
    else if (optdesc.valueType == Value::VT_BOOL)
        os << "BOOL";
    else if (optdesc.valueType == Value::VT_STRING)
        os << "STRING";
    else if (optdesc.valueType == Value::VT_FLAGS)
        os << "FLAGS";
    else if (optdesc.valueType == Value::VT_RATIO)
        os << "RATIO";
    else
        os << "UNKNOWN";
    os << " - default: " << optdesc.defaultValue;
    if (optdesc.limitType == MediaEncoder::Option::OPLT_RANGE)
        os << " - Range: [ " << optdesc.rangeMin << " ~ " << optdesc.rangeMax << " ]";
    else if (optdesc.limitType == MediaEncoder::Option::OPLT_ENUM)
    {
        os << " - Enum: { ";
        auto iter = optdesc.enumValues.begin();
        while (iter != optdesc.enumValues.end())
        {
            os << *iter++;
            if (iter != optdesc.enumValues.end())
                os << ", ";
        }
        os << " }";
    }
    if (!optdesc.desc.empty())
        os << " - desc: " << optdesc.desc;
    return os;
}

ostream& operator<<(ostream& os, const MediaEncoder::Description& encdesc)
{
    os << "Encoder: '" << encdesc.codecName << "' - ";
    if (encdesc.mediaType == MediaType::VIDEO)
        os << "VIDEO";
    else if (encdesc.mediaType == MediaType::AUDIO)
        os << "AUDIO";
    else
        os << "UNKNOWN(MediaType)";
    if (encdesc.isHardwareEncoder)
        os << " (Hardware)";
    if (!encdesc.longName.empty())
        os << " - " << encdesc.longName;
    os << endl;
    os << "Options:" << endl;
    for (auto& optdesc : encdesc.optDescList)
        os << "\t-" << optdesc << endl;
    return os;
}

static bool ConvertAVOptionToOptionDescription(AVCodecPtr cdcptr, const AVOption* opt, MediaEncoder::Option::Description& optdesc)
{
    ALogger* logger = MediaEncoder::GetLogger();
    if (opt->type == AV_OPT_TYPE_INT || opt->type == AV_OPT_TYPE_INT64 || opt->type == AV_OPT_TYPE_UINT64)
        optdesc.valueType = Value::VT_INT;
    else if (opt->type == AV_OPT_TYPE_FLOAT || opt->type == AV_OPT_TYPE_DOUBLE)
        optdesc.valueType = Value::VT_DOUBLE;
    else if (opt->type == AV_OPT_TYPE_BOOL)
        optdesc.valueType = Value::VT_BOOL;
    else if (opt->type == AV_OPT_TYPE_STRING)
        optdesc.valueType = Value::VT_STRING;
    else if (opt->type == AV_OPT_TYPE_FLAGS)
        optdesc.valueType = Value::VT_FLAGS;
    else if (opt->type == AV_OPT_TYPE_RATIONAL)
        optdesc.valueType = Value::VT_RATIO;
    else
    {
        //logger->Log(WARN) << "UNSUPPORTED ffmpeg option value type " << opt->type << " for option '" << opt->name
        //    << "' within encoder '" << cdcptr->name << "'! SKIP THIS OPTION!" << endl;
        return false;
    }
    optdesc.name = string(opt->name);
    if (opt->help) optdesc.desc = string(opt->help);
    if (opt->unit) optdesc.unit = string(opt->unit);
    optdesc.defaultValue.type = optdesc.rangeMin.type = optdesc.rangeMax.type = optdesc.valueType;
    optdesc.limitType = MediaEncoder::Option::OPLT_NONE;
    if (optdesc.valueType == Value::VT_INT)
    {
        optdesc.defaultValue.numval.i64 = opt->default_val.i64;
        optdesc.rangeMin.numval.i64 = (int64_t)opt->min;
        optdesc.rangeMax.numval.i64 = (int64_t)opt->max;
        optdesc.limitType = MediaEncoder::Option::OPLT_RANGE;
    }
    else if (optdesc.valueType == Value::VT_DOUBLE)
    {
        optdesc.defaultValue.numval.dbl = opt->default_val.dbl;
        optdesc.rangeMin.numval.dbl = opt->min;
        optdesc.rangeMax.numval.dbl = opt->max;
        optdesc.limitType = MediaEncoder::Option::OPLT_RANGE;
    }
    else if (optdesc.valueType == Value::VT_BOOL)
    {
        optdesc.defaultValue.numval.bln = opt->default_val.i64 != (int64_t)opt->min;
        optdesc.rangeMin.numval.bln = false;
        optdesc.rangeMax.numval.bln = true;
    }
    else if (optdesc.valueType == Value::VT_STRING)
    {
        if (opt->default_val.str)
            optdesc.defaultValue.strval = string(opt->default_val.str);
    }
    else if (optdesc.valueType == Value::VT_FLAGS)
    {
        optdesc.defaultValue.numval.i64 = opt->default_val.i64;
    }
    return true;
}

static bool ConvertAVOptionToOptionEnumValue(const AVOption* opt, MediaEncoder::Option::EnumValue& enumval)
{
    enumval.name = string(opt->name);
    if (opt->help) enumval.desc = string(opt->help);
    enumval.value = opt->default_val.i64;
    return true;
}

static void InitializeOptionDescList(AVCodecPtr cdcptr, vector<MediaEncoder::Option::Description>& optDescList)
{
    ALogger* logger = MediaEncoder::GetLogger();
    static vector<MediaEncoder::Option::Description> s_vidcdcOptDescList;
    static vector<MediaEncoder::Option::Description> s_audcdcOptDescList;
    if (cdcptr->type == AVMEDIA_TYPE_VIDEO)
    {
        if (s_vidcdcOptDescList.empty())
        {
            // add some fixed options
            s_vidcdcOptDescList.push_back({ "aspect", "sample aspect ratio", "", "", Value::VT_RATIO, Value(Ratio(1, 1)), MediaEncoder::Option::OPLT_NONE });

            const AVClass *cc = avcodec_get_class();
            const AVOption *opt = nullptr;
            int requairedOptFlags = AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM;
            static const char* s_vidcdcGeneralOptionNames[] = {
                "g", "bf", "colorspace", "color_trc"
            };
            const int optcnt = sizeof(s_vidcdcGeneralOptionNames)/sizeof(s_vidcdcGeneralOptionNames[0]);
            for (int i = 0; i < optcnt; i++)
            {
                if ((opt = av_opt_find(&cc, s_vidcdcGeneralOptionNames[i], nullptr, requairedOptFlags, AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ)))
                {
                    MediaEncoder::Option::Description optdesc;
                    if (ConvertAVOptionToOptionDescription(cdcptr, opt, optdesc))
                    {
                        while ((opt = av_opt_next(&cc, opt)))
                        {
                            if (opt->type != AV_OPT_TYPE_CONST || !opt->unit || strcmp(opt->unit, optdesc.unit.c_str()))
                                break;
                            optdesc.limitType = MediaEncoder::Option::OPLT_ENUM;
                            MediaEncoder::Option::EnumValue enumval;
                            ConvertAVOptionToOptionEnumValue(opt, enumval);
                            auto dupIter = find_if(optdesc.enumValues.begin(), optdesc.enumValues.end(),
                                [enumval] (const MediaEncoder::Option::EnumValue& v) {
                                    return v.value == enumval.value;
                                });
                            if (dupIter == optdesc.enumValues.end())
                                optdesc.enumValues.push_back(std::move(enumval));
                        }

                        // fix some option definitions
                        if (optdesc.name == "g")
                        {
                            optdesc.tag = "gop size";
                            optdesc.rangeMin.numval.i64 = 0;
                        }
                        if (optdesc.name == "bf")
                        {
                            optdesc.tag = "b frames";
                        }

                        s_vidcdcOptDescList.push_back(std::move(optdesc));
                    }
                }
            }
        }
        for (auto& optdesc : s_vidcdcOptDescList)
            optDescList.push_back(optdesc);
    }
    else if (cdcptr->type == AVMEDIA_TYPE_AUDIO)
    {
        if (s_audcdcOptDescList.empty())
        {
            const AVClass *cc = avcodec_get_class();
            const AVOption *opt = nullptr;
            int requairedOptFlags = AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_ENCODING_PARAM;
            static const char* s_audcdcGeneralOptionNames[] = {
                "profile"
            };
            const int optcnt = sizeof(s_audcdcGeneralOptionNames)/sizeof(s_audcdcGeneralOptionNames[0]);
            for (int i = 0; i < optcnt; i++)
            {
                if ((opt = av_opt_find(&cc, s_audcdcGeneralOptionNames[i], nullptr, requairedOptFlags, AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ)))
                {
                    MediaEncoder::Option::Description optdesc;
                    if (ConvertAVOptionToOptionDescription(cdcptr, opt, optdesc))
                    {
                        while ((opt = av_opt_next(&cc, opt)))
                        {
                            if (opt->type != AV_OPT_TYPE_CONST || !opt->unit || strcmp(opt->unit, optdesc.unit.c_str()))
                                break;
                            optdesc.limitType = MediaEncoder::Option::OPLT_ENUM;
                            MediaEncoder::Option::EnumValue enumval;
                            ConvertAVOptionToOptionEnumValue(opt, enumval);
                            auto dupIter = find_if(optdesc.enumValues.begin(), optdesc.enumValues.end(),
                                [enumval] (const MediaEncoder::Option::EnumValue& v) {
                                    return v.value == enumval.value;
                                });
                            if (dupIter == optdesc.enumValues.end())
                                optdesc.enumValues.push_back(std::move(enumval));
                        }

                        s_audcdcOptDescList.push_back(std::move(optdesc));
                    }
                }
            }
        }
        for (auto& optdesc : s_audcdcOptDescList)
            optDescList.push_back(optdesc);
    }
}

static MediaEncoder::Description ConvertAVCodecToEncoderDescription(AVCodecPtr cdcptr)
{
    ALogger* logger = MediaEncoder::GetLogger();

    MediaEncoder::Description encdesc;
    encdesc.codecName = string(cdcptr->name);
    if (cdcptr->long_name)
    {
        encdesc.longName = string(cdcptr->long_name);
        encdesc.fullName = encdesc.longName + " (" + encdesc.codecName + ")";
    }
    else
        encdesc.fullName = encdesc.codecName;

    encdesc.isHardwareEncoder = (cdcptr->capabilities&AV_CODEC_CAP_HARDWARE) != 0;
    if (cdcptr->type == AVMEDIA_TYPE_VIDEO)
        encdesc.mediaType = MediaType::VIDEO;
    else if (cdcptr->type == AVMEDIA_TYPE_AUDIO)
        encdesc.mediaType = MediaType::AUDIO;
    else
        encdesc.mediaType = MediaType::UNKNOWN;

    InitializeOptionDescList(cdcptr, encdesc.optDescList);

    const AVOption* opt = nullptr;
    while ((opt = av_opt_next(&cdcptr->priv_class, opt)))
    {
        if (opt->type == AV_OPT_TYPE_CONST)
        {
            string optunit = string(opt->unit);
            auto optdescIter = encdesc.optDescList.end();
            if (!optunit.empty())
                optdescIter = find_if(encdesc.optDescList.begin(), encdesc.optDescList.end(),
                    [optunit] (const MediaEncoder::Option::Description& optdesc) {
                        return optdesc.unit == optunit;
                    });
            if (optdescIter != encdesc.optDescList.end())
            {
                MediaEncoder::Option::EnumValue enumval;
                if (ConvertAVOptionToOptionEnumValue(opt, enumval))
                {
                    auto& optdesc = *optdescIter;
                    optdesc.limitType = MediaEncoder::Option::OPLT_ENUM;
                    optdesc.enumValues.push_back(std::move(enumval));
                }
            }
            else
                logger->Log(WARN) << "CANNOT find unit '" << opt->unit << "' in option list for option '"
                    << opt->name << "' within encoder '" << cdcptr->name << "'! SKIP THIS OPTION!" << endl;
        }
        else
        {
            MediaEncoder::Option::Description optdesc;
            if (ConvertAVOptionToOptionDescription(cdcptr, opt, optdesc))
                encdesc.optDescList.push_back(std::move(optdesc));
        }
    }
    return std::move(encdesc);
}

bool MediaEncoder::FindEncoder(const string& codecName, vector<MediaEncoder::Description>& encoderDescList)
{
    encoderDescList.clear();
    const AVCodecDescriptor* desc = avcodec_descriptor_get_by_name(codecName.c_str());
    if (!desc)
        return false;

    void* cdciter = 0;
    AVCodecPtr p;
    while ((p = (AVCodecPtr)av_codec_iterate(&cdciter)))
    {
        if (p->id != desc->id)
            continue;
        if (!av_codec_is_encoder(p))
            continue;
        if ((p->capabilities&AV_CODEC_CAP_EXPERIMENTAL) != 0)
            continue;
        encoderDescList.push_back(std::move(ConvertAVCodecToEncoderDescription(p)));
    }
    return true;
}
}