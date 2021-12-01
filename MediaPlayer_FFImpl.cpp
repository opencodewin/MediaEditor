#include <sstream>
#include <thread>
#include <mutex>
#include <list>
#include "MediaPlayer_FFImpl.h"
#include "Log.h"
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

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts);

class MediaPlayer_FFImpl : public MediaPlayer
{
public:
    using Clock = chrono::steady_clock;
    using Duration = chrono::duration<double, milli>;
    using TimePoint = chrono::time_point<Clock>;
    static const TimePoint CLOCK_MIN;

    bool Open(const string& url) override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (!OpenMedia(url))
        {
            Close();
            return false;
        }
        return true;
    }

    bool Close() override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        WaitAllThreadsQuit();
        FlushAllQueues();

        if (m_auddecCtx)
        {
            avcodec_free_context(&m_auddecCtx);
            m_auddecCtx = nullptr;
        }
        if (m_viddecCtx)
        {
            avcodec_free_context(&m_viddecCtx);
            m_viddecCtx = nullptr;
        }
        if (m_viddecHwDevCtx)
        {
            av_buffer_unref(&m_viddecHwDevCtx);
            m_viddecHwDevCtx = nullptr;
        }
        m_vidHwPixFmt = AV_PIX_FMT_NONE;
        m_viddecDevType = AV_HWDEVICE_TYPE_NONE;
        if (m_avfmtCtx)
        {
            avformat_close_input(&m_avfmtCtx);
            m_avfmtCtx = nullptr;
        }
        m_vidStmIdx = -1;
        m_audStmIdx = -1;
        m_vidStream = nullptr;
        m_audStream = nullptr;
        m_viddec = nullptr;
        m_auddec = nullptr;

        m_demuxEof = false;
        m_viddecEof = false;
        m_auddecEof = false;
        m_renderEof = false;

        m_vidpktQMaxSize = 0;
        m_audpktQMaxSize = 0;

        m_errMessage = "";
        return true;
    }

    bool Play() override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (m_isPlaying)
            return true;
        if (!IsOpened())
        {
            m_errMessage = "No media has been opened!";
            return false;
        }
        if (!HasVideo() && !HasAudio())
        {
            m_errMessage = "No video nor audio is to be played!";
            return false;
        }

        if (m_renderEof)
            if (!Reset())
                return false;

        if (m_runStartTp == CLOCK_MIN)
            m_runStartTp = Clock::now();
        if (m_pauseStartTp != CLOCK_MIN)
        {
            m_pausedDur += Clock::now()-m_pauseStartTp;
            m_pauseStartTp = CLOCK_MIN;
        }

        if (!m_renderThread.joinable())
        {
            m_demuxThread = thread(&MediaPlayer_FFImpl::DemuxThreadProc, this);
            if (HasVideo())
                m_viddecThread = thread(&MediaPlayer_FFImpl::DecodeThreadProc, this,
                    m_viddecCtx, &m_viddecEof, &m_vidpktQ, &m_vidpktQLock, &m_vidfrmQ, &m_vidfrmQLock, m_vidfrmQMaxSize);
            if (HasAudio())
                m_auddecThread = thread(&MediaPlayer_FFImpl::DecodeThreadProc, this,
                    m_auddecCtx, &m_auddecEof, &m_audpktQ, &m_audpktQLock, &m_audfrmQ, &m_audfrmQLock, m_audfrmQMaxSize);
            m_renderThread = thread(&MediaPlayer_FFImpl::RenderThreadProc, this);
        }
        m_isPlaying = true;
        return true;
    }

    bool Pause() override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (!IsOpened())
        {
            m_errMessage = "No media has been opened!";
            return false;
        }
        m_pauseStartTp = Clock::now();
        m_isPlaying = false;
        return true;
    }

    bool Reset() override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (!IsOpened())
        {
            m_errMessage = "No media has been opened!";
            return false;
        }

        WaitAllThreadsQuit();
        FlushAllQueues();

        if (m_viddecCtx)
            avcodec_flush_buffers(m_viddecCtx);
        if (m_auddecCtx)
            avcodec_flush_buffers(m_auddecCtx);

        m_demuxEof = false;
        m_viddecEof = false;
        m_auddecEof = false;
        m_renderEof = false;

        int fferr = avformat_seek_file(m_avfmtCtx, -1, INT64_MIN, m_avfmtCtx->start_time, m_avfmtCtx->start_time, 0);
        if (fferr < 0)
        {
            SetFFError("avformat_seek_file", fferr);
            return false;
        }
        return true;
    }

    bool Seek(uint64_t pos) override
    {
        return true;
    }

    bool SeekAsync(uint64_t pos) override
    {
        return true;
    }

    bool IsOpened() const override
    {
        return (bool)m_avfmtCtx;
    }

    bool IsPlaying() const override
    {
        return m_isPlaying;
    }

    bool HasVideo() const override
    {
        return m_vidStmIdx >= 0;
    }

    bool HasAudio() const override
    {
        return m_audStmIdx >= 0;
    }

    float GetPlaySpeed() const override
    {
        return 1.0f;
    }

    bool SetPlaySpeed(float speed) override
    {
        return false;
    }

    bool SetPreferHwDecoder(bool prefer) override
    {
        m_vidPreferUseHw = prefer;
        return true;
    }

    uint64_t GetDuration() const override
    {
        if (!m_avfmtCtx)
            return 0;
        int64_t dur = av_rescale(m_avfmtCtx->duration, 1000L, AV_TIME_BASE);
        return dur < 0 ? 0 : (uint64_t)dur;
    }

    uint64_t GetPlayPos() const override
    {
        return 0;
    }

    ImGui::ImMat GetVideo() const override
    {
        return m_vidMat;
    }

    string GetError() const override
    {
        return m_errMessage;
    }

    bool CheckHwPixFmt(AVPixelFormat pixfmt)
    {
        return pixfmt == m_vidHwPixFmt;
    }

private:
    static const AVRational MILLISEC_TIMEBASE;

    void SetFFError(const string& funcname, int fferr)
    {
        ostringstream oss;
        oss << "'" << funcname << "' returns " << fferr << ".";
        m_errMessage = oss.str();
    }

    bool OpenMedia(const string& url)
    {
        if (IsOpened())
            Close();

        int fferr = 0;
        fferr = avformat_open_input(&m_avfmtCtx, url.c_str(), nullptr, nullptr);
        if (fferr < 0)
        {
            SetFFError("avformat_open_input", fferr);
            return false;
        }
        fferr = avformat_find_stream_info(m_avfmtCtx, nullptr);
        if (fferr < 0)
        {
            SetFFError("avformat_find_stream_info", fferr);
            return false;
        }
        Log::Info("Open '%s' successfully. %d streams are found.", url.c_str(), m_avfmtCtx->nb_streams);

        m_vidStmIdx = av_find_best_stream(m_avfmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &m_viddec, 0);
        // m_audStmIdx = av_find_best_stream(m_avfmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &m_auddec, 0);
        if (m_vidStmIdx < 0 && m_audStmIdx < 0)
        {
            ostringstream oss;
            oss << "Neither video nor audio stream can be found in '" << url << "'.";
            m_errMessage = oss.str();
            return false;
        }
        m_vidStream = m_vidStmIdx >= 0 ? m_avfmtCtx->streams[m_vidStmIdx] : nullptr;
        m_audStream = m_audStmIdx >= 0 ? m_avfmtCtx->streams[m_audStmIdx] : nullptr;

        if (m_vidStream)
        {
            if (m_vidPreferUseHw)
            {
                if (!OpenHwVideoDecoder())
                    if (!OpenVideoDecoder())
                        return false;
            }
            else
            {
                if (!OpenVideoDecoder())
                    return false;
            }
            int qMaxSize = 0;
            if (m_vidStream->avg_frame_rate.den > 0)
                qMaxSize = (int)(m_vidpktQDuration*m_vidStream->avg_frame_rate.num/m_vidStream->avg_frame_rate.den);
            if (qMaxSize < 20)
                qMaxSize = 20;
            m_vidpktQMaxSize = qMaxSize;
        }
        if (m_audStream)
        {
            if (!OpenAudioDecoder())
                return false;
            m_audpktQMaxSize = 50;
        }
        return true;
    }

    bool OpenVideoDecoder()
    {
        m_viddecCtx = avcodec_alloc_context3(m_viddec);
        if (!m_viddecCtx)
        {
            m_errMessage = "FAILED to allocate new AVCodecContext!";
            return false;
        }
        m_viddecCtx->opaque = this;

        int fferr;
        fferr = avcodec_parameters_to_context(m_viddecCtx, m_vidStream->codecpar);
        if (fferr < 0)
        {
            SetFFError("avcodec_parameters_to_context", fferr);
            return false;
        }

        fferr = avcodec_open2(m_viddecCtx, m_viddec, nullptr);
        if (fferr < 0)
        {
            SetFFError("avcodec_open2", fferr);
            return false;
        }
        return true;
    }

    bool OpenHwVideoDecoder()
    {
        m_vidHwPixFmt = AV_PIX_FMT_NONE;
        for (int i = 0; ; i++)
        {
            const AVCodecHWConfig* config = avcodec_get_hw_config(m_viddec, i);
            if (!config)
            {
                ostringstream oss;
                oss << "Decoder '" << m_viddec->name << "' does NOT support hardware acceleration.";
                m_errMessage = oss.str();
                return false;
            }
            if ((config->methods&AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0)
            {
                if (m_vidUseHwType == AV_HWDEVICE_TYPE_NONE || m_vidUseHwType == config->device_type)
                {
                    m_vidHwPixFmt = config->pix_fmt;
                    m_viddecDevType = config->device_type;
                    break;
                }
            }
        }

        m_viddecCtx = avcodec_alloc_context3(m_viddec);
        if (!m_viddecCtx)
        {
            m_errMessage = "FAILED to allocate new AVCodecContext!";
            return false;
        }
        m_viddecCtx->opaque = this;

        int fferr;
        fferr = avcodec_parameters_to_context(m_viddecCtx, m_vidStream->codecpar);
        if (fferr < 0)
        {
            SetFFError("avcodec_parameters_to_context", fferr);
            return false;
        }
        m_viddecCtx->get_format = get_hw_format;

        fferr = av_hwdevice_ctx_create(&m_viddecHwDevCtx, m_viddecDevType, nullptr, nullptr, 0);
        if (fferr < 0)
        {
            SetFFError("av_hwdevice_ctx_create", fferr);
            return false;
        }
        m_viddecCtx->hw_device_ctx = av_buffer_ref(m_viddecHwDevCtx);

        fferr = avcodec_open2(m_viddecCtx, m_viddec, nullptr);
        if (fferr < 0)
        {
            SetFFError("avcodec_open2", fferr);
            return false;
        }
        return true;
    }

    bool OpenAudioDecoder()
    {
        m_auddecCtx = avcodec_alloc_context3(m_auddec);
        if (!m_auddecCtx)
        {
            m_errMessage = "FAILED to allocate new AVCodecContext!";
            return false;
        }
        m_auddecCtx->opaque = this;

        int fferr;
        fferr = avcodec_parameters_to_context(m_auddecCtx, m_audStream->codecpar);
        if (fferr < 0)
        {
            SetFFError("avcodec_parameters_to_context", fferr);
            return false;
        }

        fferr = avcodec_open2(m_auddecCtx, m_auddec, nullptr);
        if (fferr < 0)
        {
            SetFFError("avcodec_open2", fferr);
            return false;
        }
        return true;
    }

    void DemuxThreadProc()
    {
        AVPacket avpkt = {0};
        bool avpktLoaded = false;
        while (!m_quitPlay)
        {
            bool idleLoop = true;

            if (!avpktLoaded)
            {
                int fferr = av_read_frame(m_avfmtCtx, &avpkt);
                if (fferr == 0)
                {
                    avpktLoaded = true;
                    idleLoop = false;
                }
                else
                {
                    if (fferr == AVERROR_EOF)
                        Log::Info("Demuxer EOF.");
                    else
                        Log::Error("Demuxer ERROR! 'av_read_frame' returns %d.", fferr);
                    break;
                }
            }

            if (avpkt.stream_index == m_vidStmIdx)
            {
                if (m_vidpktQ.size() < m_vidpktQMaxSize)
                {
                    AVPacket* enqpkt = av_packet_clone(&avpkt);
                    if (!enqpkt)
                    {
                        Log::Error("FAILED to invoke 'av_packet_clone'!");
                        break;
                    }
                    lock_guard<mutex> lk(m_vidpktQLock);
                    m_vidpktQ.push_back(enqpkt);
                    av_packet_unref(&avpkt);
                    avpktLoaded = false;
                    idleLoop = false;
                }
            }
            else if (avpkt.stream_index == m_audStmIdx)
            {
                if (m_vidpktQMaxSize > 0 || m_audpktQ.size() < m_audpktQMaxSize)
                {
                    AVPacket* enqpkt = av_packet_clone(&avpkt);
                    if (!enqpkt)
                    {
                        Log::Error("FAILED to invoke 'av_packet_clone'!");
                        break;
                    }
                    lock_guard<mutex> lk(m_audpktQLock);
                    m_audpktQ.push_back(enqpkt);
                    av_packet_unref(&avpkt);
                    avpktLoaded = false;
                    idleLoop = false;
                }
            }
            else
            {
                av_packet_unref(&avpkt);
                avpktLoaded = false;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_demuxEof = true;
    }

    void DecodeThreadProc(
        AVCodecContext* decCtx, bool* decEof,
        list<AVPacket*>* inQ, mutex* inQLock,
        list<AVFrame*>* outQ, mutex* outQLock, int outQMaxSize)
    {
        AVFrame avfrm = {0};
        bool avfrmLoaded = false;
        bool inputEof = false;
        while (!m_quitPlay)
        {
            bool idleLoop = true;
            bool quitLoop = false;

            // retrieve output frame
            bool hasOutput;
            do{
                if (!avfrmLoaded)
                {
                    int fferr = avcodec_receive_frame(decCtx, &avfrm);
                    if (fferr == 0)
                    {
                        avfrmLoaded = true;
                        idleLoop = false;
                    }
                    else if (fferr != AVERROR(EAGAIN))
                    {
                        if (fferr != AVERROR_EOF)
                            Log::Error("FAILED to invoke 'avcodec_receive_frame'(AUDIO)! return code is %d.", fferr);
                        quitLoop = true;
                        break;
                    }
                }

                hasOutput = avfrmLoaded;
                if (avfrmLoaded)
                {
                    if (outQ->size() < outQMaxSize)
                    {
                        lock_guard<mutex> lk(*outQLock);
                        AVFrame* enqfrm = av_frame_clone(&avfrm);
                        outQ->push_back(enqfrm);
                        av_frame_unref(&avfrm);
                        avfrmLoaded = false;
                        idleLoop = false;
                    }
                    else
                        break;
                }
            } while (hasOutput);
            if (quitLoop)
                break;

            // input packet to decoder
            if (!inputEof)
            {
                while (inQ->size() > 0)
                {
                    AVPacket* avpkt = inQ->front();
                    int fferr = avcodec_send_packet(decCtx, avpkt);
                    if (fferr == 0)
                    {
                        lock_guard<mutex> lk(*inQLock);
                        inQ->pop_front();
                        av_packet_free(&avpkt);
                        idleLoop = false;
                    }
                    else
                    {
                        if (fferr != AVERROR(EAGAIN))
                        {
                            Log::Error("FAILED to invoke 'avcodec_send_packet'(AUDIO)! return code is %d.", fferr);
                            quitLoop = true;
                        }
                        break;
                    }
                }
                if (quitLoop)
                    break;

                if (inQ->size() == 0 && m_demuxEof)
                {
                    avcodec_send_packet(decCtx, nullptr);
                    idleLoop = false;
                    inputEof = true;
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        *decEof = true;
    }

    void RenderThreadProc()
    {
        while (!m_quitPlay)
        {
            if (!m_isPlaying)
            {
                this_thread::sleep_for(chrono::milliseconds(5));
                continue;
            }

            bool vidIdleRun = true;
            // Audio
            if (HasAudio())
            {
                m_playPos = chrono::duration_cast<Duration>(m_audioTs-Duration(m_audioOffset));
            }
            else
            {
                m_playPos = Clock::now()-m_runStartTp-m_pausedDur;
            }

            // Video
            if (HasVideo() && !m_vidfrmQ.empty())
            {
                AVFrame* vidfrm = m_vidfrmQ.front();
                int64_t mts = av_rescale_q(vidfrm->pts, m_vidStream->time_base, MILLISEC_TIMEBASE);
                if (m_playPos.count() >= mts)
                {
                    lock_guard<mutex> lk(m_vidfrmQLock);
                    m_vidfrmQ.pop_front();
                    ConvertAVFrameToImMat(vidfrm, m_vidMat);
                    m_vidMat.time_stamp = (double)mts/1000;
                    av_frame_free(&vidfrm);
                    vidIdleRun = false;
                }
            }

            if (vidIdleRun)
                this_thread::sleep_for(chrono::milliseconds(1));
        }
        m_renderEof = true;
    }

    bool ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& vmat)
    {
        const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get((AVPixelFormat)avfrm->format);
        AVFrame* swfrm = nullptr;
        if ((desc->flags&AV_PIX_FMT_FLAG_HWACCEL) > 0)
        {
            swfrm = av_frame_alloc();
            if (!swfrm)
            {
                m_errMessage = "FAILED to allocate new AVFrame for ImMat conversion!";
                return false;
            }
            int fferr = av_hwframe_transfer_data(swfrm, avfrm, 0);
            if (fferr < 0)
            {
                SetFFError("av_hwframe_transfer_data", fferr);
                av_frame_free(&swfrm);
                return false;
            }
            desc = av_pix_fmt_desc_get((AVPixelFormat)swfrm->format);
            avfrm = swfrm;
        }

#define ISYUV420P(format)   \
        (format == AV_PIX_FMT_YUV420P || \
        format == AV_PIX_FMT_YUVJ420P || \
        format == AV_PIX_FMT_YUV420P9 || \
        format == AV_PIX_FMT_YUV420P10 || \
        format == AV_PIX_FMT_YUV420P12 || \
        format == AV_PIX_FMT_YUV420P14 || \
        format == AV_PIX_FMT_YUV420P16)

#define ISYUV422P(format) \
        (format == AV_PIX_FMT_YUV422P || \
        format == AV_PIX_FMT_YUVJ422P || \
        format == AV_PIX_FMT_YUV422P9 || \
        format == AV_PIX_FMT_YUV422P10 || \
        format == AV_PIX_FMT_YUV422P12 || \
        format == AV_PIX_FMT_YUV422P14 || \
        format == AV_PIX_FMT_YUV422P16)

#define ISYUV444P(format) \
        (format == AV_PIX_FMT_YUV444P || \
        format == AV_PIX_FMT_YUVJ420P || \
        format == AV_PIX_FMT_YUV444P9 || \
        format == AV_PIX_FMT_YUV444P10 || \
        format == AV_PIX_FMT_YUV444P12 || \
        format == AV_PIX_FMT_YUV444P14 || \
        format == AV_PIX_FMT_YUV444P16)

#define ISNV12(format) \
        (format == AV_PIX_FMT_NV12 || \
        format == AV_PIX_FMT_NV21 || \
        format == AV_PIX_FMT_NV16 || \
        format == AV_PIX_FMT_NV20LE || \
        format == AV_PIX_FMT_NV20BE || \
        format == AV_PIX_FMT_P010LE || \
        format == AV_PIX_FMT_P010BE || \
        format == AV_PIX_FMT_P016LE || \
        format == AV_PIX_FMT_P016BE || \
        format == AV_PIX_FMT_NV24 || \
        format == AV_PIX_FMT_NV42 || \
        format == AV_PIX_FMT_NV20)

        int bitDepth = desc->comp[0].depth;
        ImColorSpace color_space =  avfrm->colorspace == AVCOL_SPC_BT470BG ||
                                    avfrm->colorspace == AVCOL_SPC_SMPTE170M ||
                                    avfrm->colorspace == AVCOL_SPC_BT470BG ? IM_CS_BT601 :
                                    avfrm->colorspace == AVCOL_SPC_BT709 ? IM_CS_BT709 :
                                    avfrm->colorspace == AVCOL_SPC_BT2020_NCL ||
                                    avfrm->colorspace == AVCOL_SPC_BT2020_CL ? IM_CS_BT2020 : IM_CS_BT709;
        ImColorRange color_range =  avfrm->color_range == AVCOL_RANGE_MPEG ? IM_CR_NARROW_RANGE :
                                    avfrm->color_range == AVCOL_RANGE_JPEG ? IM_CR_FULL_RANGE : IM_CR_NARROW_RANGE;
        ImColorFormat color_format = ISYUV420P(avfrm->format) ? IM_CF_YUV420 :
                                    ISYUV422P(avfrm->format) ? IM_CF_YUV422 :
                                    ISYUV444P(avfrm->format) ? IM_CF_YUV444 :
                                    ISNV12(avfrm->format) ? bitDepth == 10 ? IM_CF_P010LE : IM_CF_NV12 : IM_CF_YUV420;
        const int width = avfrm->width;
        const int height = avfrm->height;
        int bytesPerPix = bitDepth > 8 ? 2 : 1;

        ImGui::ImMat mat_V;
        mat_V.create_type(width, height, 4, bitDepth > 8 ? IM_DT_INT16 : IM_DT_INT8);
        ImGui::ImMat mat_Y = mat_V.channel(0);
        {
            uint8_t* src_data = avfrm->data[0]+bytesPerPix*desc->comp[0].offset;
            uint8_t* dst_data = (uint8_t*)mat_Y.data;
            int bytesPerLine = width*bytesPerPix*(desc->comp[0].step+1);
            for (int i = 0; i < height; i++)
            {
                memcpy(dst_data, src_data, bytesPerLine);
                src_data += avfrm->linesize[0];
                dst_data += bytesPerLine;
            }
        }
        ImGui::ImMat mat_Cb = mat_V.channel(1);
        if (desc->nb_components > 1)
        {
            uint8_t* src_data = avfrm->data[1]+bytesPerPix*desc->comp[1].offset;
            uint8_t* dst_data = (uint8_t*)mat_Cb.data;
            int width1 = width >> desc->log2_chroma_w;
            int height1 = height >> desc->log2_chroma_h;
            int bytesPerLine = width1*bytesPerPix*(desc->comp[1].step+1);
            for (int i = 0; i < height1; i++)
            {
                memcpy(dst_data, src_data, bytesPerLine);
                src_data += avfrm->linesize[1];
                dst_data += bytesPerLine;
            }
        }
        ImGui::ImMat mat_Cr = mat_V.channel(2);
        if (desc->nb_components > 2)
        {
            uint8_t* src_data = avfrm->data[2]+bytesPerPix*desc->comp[2].offset;
            uint8_t* dst_data = (uint8_t*)mat_Cr.data;
            int width1 = width >> desc->log2_chroma_w;
            int height1 = height >> desc->log2_chroma_h;
            int bytesPerLine = width1*bytesPerPix*(desc->comp[2].step+1);
            for (int i = 0; i < height1; i++)
            {
                memcpy(dst_data, src_data, bytesPerLine);
                src_data += avfrm->linesize[1];
                dst_data += bytesPerLine;
            }
        }
        mat_V.color_space = color_space;
        mat_V.color_range = color_range;
        mat_V.color_format = color_format;
        mat_V.depth = bitDepth;
        mat_V.flags = IM_MAT_FLAGS_VIDEO_FRAME;
        if (avfrm->pict_type == AV_PICTURE_TYPE_I) mat_V.flags |= IM_MAT_FLAGS_VIDEO_FRAME_I;
        if (avfrm->pict_type == AV_PICTURE_TYPE_P) mat_V.flags |= IM_MAT_FLAGS_VIDEO_FRAME_P;
        if (avfrm->pict_type == AV_PICTURE_TYPE_B) mat_V.flags |= IM_MAT_FLAGS_VIDEO_FRAME_B;
        if (avfrm->interlaced_frame) mat_V.flags |= IM_MAT_FLAGS_VIDEO_INTERLACED;

        vmat = mat_V;
        if (swfrm)
            av_frame_free(&swfrm);
        return true;
    }

    void WaitAllThreadsQuit()
    {
        m_quitPlay = true;
        if (m_demuxThread.joinable())
        {
            m_demuxThread.join();
            m_demuxThread = thread();
        }
        if (m_viddecThread.joinable())
        {
            m_viddecThread.join();
            m_viddecThread = thread();
        }
        if (m_auddecThread.joinable())
        {
            m_auddecThread.join();
            m_auddecThread = thread();
        }
        if (m_renderThread.joinable())
        {
            m_renderThread.join();
            m_renderThread = thread();
        }
        m_isPlaying = false;
    }

    void FlushAllQueues()
    {
        for (AVPacket* avpkt : m_vidpktQ)
            av_packet_free(&avpkt);
        m_vidpktQ.clear();
        for (AVPacket* avpkt : m_audpktQ)
            av_packet_free(&avpkt);
        m_audpktQ.clear();
        for (AVFrame* avfrm : m_vidfrmQ)
            av_frame_free(&avfrm);
        m_vidfrmQ.clear();
        for (AVFrame* avfrm : m_audfrmQ)
            av_frame_free(&avfrm);
        m_audfrmQ.clear();
    }

private:
    string m_errMessage;
    bool m_vidPreferUseHw;
    AVHWDeviceType m_vidUseHwType{AV_HWDEVICE_TYPE_NONE};

    AVFormatContext* m_avfmtCtx{nullptr};
    int m_vidStmIdx{-1};
    int m_audStmIdx{-1};
    AVStream* m_vidStream{nullptr};
    AVStream* m_audStream{nullptr};
    const AVCodec* m_viddec{nullptr};
    const AVCodec* m_auddec{nullptr};
    AVCodecContext* m_viddecCtx{nullptr};
    AVCodecContext* m_auddecCtx{nullptr};
    AVPixelFormat m_vidHwPixFmt{AV_PIX_FMT_NONE};
    AVHWDeviceType m_viddecDevType{AV_HWDEVICE_TYPE_NONE};
    AVBufferRef* m_viddecHwDevCtx{nullptr};

    // demuxing thread
    thread m_demuxThread;
    float m_vidpktQDuration{2.0f};
    int m_vidpktQMaxSize{0};
    list<AVPacket*> m_vidpktQ;
    mutex m_vidpktQLock;
    int m_audpktQMaxSize{0};
    list<AVPacket*> m_audpktQ;
    mutex m_audpktQLock;
    bool m_demuxEof{false};
    // video decoding thread
    thread m_viddecThread;
    int m_vidfrmQMaxSize{4};
    list<AVFrame*> m_vidfrmQ;
    mutex m_vidfrmQLock;
    bool m_viddecEof{false};
    // audio decoding thread
    thread m_auddecThread;
    int m_audfrmQMaxSize{4};
    list<AVFrame*> m_audfrmQ;
    mutex m_audfrmQLock;
    bool m_auddecEof{false};
    // rendering thread
    thread m_renderThread;
    bool m_renderEof{false};

    recursive_mutex m_ctlLock;
    bool m_quitPlay{false};
    bool m_isPlaying{false};

    Duration m_audioTs;
    int64_t m_audioOffset{0};
    Duration m_playPos, m_pausedDur;
    TimePoint m_runStartTp{CLOCK_MIN}, m_pauseStartTp{CLOCK_MIN};

    ImGui::ImMat m_vidMat;
};

constexpr MediaPlayer_FFImpl::TimePoint MediaPlayer_FFImpl::CLOCK_MIN = MediaPlayer_FFImpl::Clock::time_point::min();
const AVRational MediaPlayer_FFImpl::MILLISEC_TIMEBASE = { 1, 1000 };

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{
    MediaPlayer_FFImpl* mp = reinterpret_cast<MediaPlayer_FFImpl*>(ctx->opaque);
    const AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (mp->CheckHwPixFmt(*p))
            return *p;
    }
    return AV_PIX_FMT_NONE;
}

MediaPlayer* CreateMediaPlayer()
{
    return static_cast<MediaPlayer*>(new MediaPlayer_FFImpl());
}

void DestroyMediaPlayer(MediaPlayer** player)
{
    if (!player)
        return;
    MediaPlayer_FFImpl* ffPlayer = dynamic_cast<MediaPlayer_FFImpl*>(*player);
    ffPlayer->Close();
    delete ffPlayer;
    *player = nullptr;
}