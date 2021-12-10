#include <thread>
#include <mutex>
#include <iostream>
#include <iomanip>
#include <list>
#include <atomic>
#include <memory>
#include <cmath>
#include <algorithm>
#include "MediaSource.h"
#include "FFUtils.h"
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

class MediaSource_Impl : public MediaSource
{
public:
    bool ConfigureSnapWindow(double windowSize, double frameCount) override
    {
        if (windowSize <= 0)
        {
            m_errMessage = "Argument 'windowSize' must be POSITIVE!";
            return false;
        }
        m_snapWindowSize = windowSize;
        if (frameCount <= 0)
        {
            m_errMessage = "Argument 'frameCount' must be POSITIVE!";
            return false;
        }
        m_snapshotInterval = windowSize*1000./frameCount;
        m_maxCacheSize = (uint32_t)ceil(frameCount*m_cacheFactor);
        m_shrinkSize = (uint32_t)ceil(m_maxCacheSize*m_shrinkFactor);
        return true;
    }

    bool ConfigureSnapshot(uint32_t width, uint32_t height) override
    {
        m_ssWidth = width;
        m_ssHeight = height;
        return true;
    }

    bool Open(const string& url) override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (!OpenMedia(url))
        {
            Close();
            return false;
        }
        if (HasVideo())
        {
            m_vidStartMts = av_rescale_q(m_vidStream->start_time, m_vidStream->time_base, MILLISEC_TIMEBASE);
            int64_t vidDur = av_rescale_q(m_vidStream->duration, m_vidStream->time_base, MILLISEC_TIMEBASE);
            m_vidMaxIndex = (uint32_t)floor((double)vidDur/m_snapshotInterval);
            m_vidfrmInterval = av_q2d(av_inv_q(m_vidStream->avg_frame_rate))*1000.;
        }
        return true;
    }

    void Close() override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        WaitAllThreadsQuit();
        FlushAllQueues();

        if (m_swrCtx)
        {
            swr_free(&m_swrCtx);
            m_swrCtx = nullptr;
        }
        m_swrOutChannels = 0;
        m_swrOutChnLyt = 0;
        m_swrOutSmpfmt = AV_SAMPLE_FMT_FLTP;
        m_swrOutSampleRate = 0;
        m_swrPassThrough = false;
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
        m_audfrmQMaxSize = 5;
        m_swrfrmQMaxSize = 24;
        m_audfrmAvgDur = 0.021;

        m_errMessage = "";
    }

    bool IsOpened() const override
    {
        return (bool)m_avfmtCtx;
    }

    bool HasVideo() const override
    {
        return m_vidStmIdx >= 0;
    }

    bool HasAudio() const override
    {
        return m_audStmIdx >= 0;
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
    static const AVRational FFAV_TIMEBASE;

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
        cout << "Open '" << url << "' successfully. " << m_avfmtCtx->nb_streams << " streams are found." << endl;

        m_vidStmIdx = av_find_best_stream(m_avfmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &m_viddec, 0);
        m_audStmIdx = av_find_best_stream(m_avfmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &m_auddec, 0);
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

        m_viddecCtx->thread_count = 8;
        // m_viddecCtx->thread_type = FF_THREAD_FRAME;
        fferr = avcodec_open2(m_viddecCtx, m_viddec, nullptr);
        if (fferr < 0)
        {
            SetFFError("avcodec_open2", fferr);
            return false;
        }
        cout << "Video decoder '" << m_viddec->name << "' opened." << " thread_count=" << m_viddecCtx->thread_count
            << ", thread_type=" << m_viddecCtx->thread_type << endl;
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
        cout << "Use hardware device type '" << av_hwdevice_get_type_name(m_viddecDevType) << "'." << endl;

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
        cout << "Video decoder(HW) '" << m_viddecCtx->codec->name << "' opened." << endl;
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
        cout << "Audio decoder '" << m_auddec->name << "' opened." << endl;

        // setup sw resampler
        int inChannels = m_audStream->codecpar->channels;
        uint64_t inChnLyt = m_audStream->codecpar->channel_layout;
        int inSampleRate = m_audStream->codecpar->sample_rate;
        AVSampleFormat inSmpfmt = (AVSampleFormat)m_audStream->codecpar->format;
        m_swrOutChannels = inChannels > 2 ? 2 : inChannels;
        m_swrOutChnLyt = av_get_default_channel_layout(m_swrOutChannels);
        m_swrOutSmpfmt = AV_SAMPLE_FMT_S16;
        m_swrOutSampleRate = inSampleRate;
        if (inChnLyt <= 0)
            inChnLyt = av_get_default_channel_layout(inChannels);
        if (m_swrOutChnLyt != inChnLyt || m_swrOutSmpfmt != inSmpfmt || m_swrOutSampleRate != inSampleRate)
        {
            m_swrCtx = swr_alloc_set_opts(NULL, m_swrOutChnLyt, m_swrOutSmpfmt, m_swrOutSampleRate, inChnLyt, inSmpfmt, inSampleRate, 0, nullptr);
            if (!m_swrCtx)
            {
                m_errMessage = "FAILED to invoke 'swr_alloc_set_opts()' to create 'SwrContext'!";
                return false;
            }
            int fferr = swr_init(m_swrCtx);
            if (fferr < 0)
            {
                SetFFError("swr_init", fferr);
                return false;
            }
            m_swrPassThrough = false;
        }
        else
        {
            m_swrPassThrough = true;
        }
        return true;
    }

    void DemuxThreadProc()
    {
        cout << "Enter DemuxThreadProc()..." << endl;
        bool fatalError = false;
        AVPacket avpkt = {0};
        bool avpktLoaded = false;
        int64_t ptsAfterSeek = INT64_MIN;
        bool hasTask = false;
        uint32_t targetSsIndex = 0;
        int64_t ptsForTargetSs = INT64_MIN;
        bool toNextBuildTask = false;
        while (!m_quitScan)
        {
            bool idleLoop = true;

            if (HasVideo())
            {
                shared_ptr<SnapshotBuildTask> ssTask = GetSnapshotBuildTask(toNextBuildTask);
                if (ssTask)
                {
                    toNextBuildTask = false;
                    if (!hasTask || targetSsIndex != ssTask->targetSsIndex)
                    {
                        hasTask = true;
                        ptsForTargetSs = INT64_MIN;
                        targetSsIndex = ssTask->targetSsIndex;
                    }
                    if (ptsAfterSeek != ssTask->startPts)
                    {
                        int fferr = avformat_seek_file(m_avfmtCtx, m_vidStmIdx, INT64_MIN, ssTask->startPts, ssTask->startPts, 0);
                        if (fferr < 0)
                        {
                            cerr << "avformat_seek_file() FAILED for seeking to 'ssTask->startPts'(" << ssTask->startPts << ")! fferr = " << fferr << "!" << endl;
                            fatalError = true;
                            break;
                        }
                        if (avpktLoaded)
                        {
                            av_packet_unref(&avpkt);
                            avpktLoaded = false;
                        }
                        if (!ReadNextStreamPacket(m_vidStmIdx, &avpkt, &avpktLoaded, &ptsAfterSeek))
                        {
                            fatalError = true;
                            break;
                        }
                        if (ptsAfterSeek != ssTask->startPts)
                        {
                            cout << "WARNING! 'ptsAfterSeek'(" << ptsAfterSeek << ") != 'ssTask->startPts'(" << ssTask->startPts << ")!" << endl;
                            ssTask->startPts = ptsAfterSeek;
                        }
                        cout << "ssTask updated startPts to " << ssTask->startPts << "." << endl;
                    }

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
                                cout << "Demuxer EOF." << endl;
                            else
                                cerr << "Demuxer ERROR! 'av_read_frame' returns " << fferr << "." << endl;
                            break;
                        }
                    }

                    if (avpkt.stream_index == m_vidStmIdx)
                    {
                        if (ptsForTargetSs == INT64_MIN)
                        {
                            if (IsSpecificSnapshotFrame(targetSsIndex, av_rescale_q(avpkt.pts, m_vidStream->time_base, MILLISEC_TIMEBASE)))
                                ptsForTargetSs = avpkt.pts;
                        }
                        else if (ptsForTargetSs != avpkt.pts)
                            toNextBuildTask = true;

                        if (!toNextBuildTask && m_vidpktQ.size() < m_vidpktQMaxSize)
                        {
                            AVPacket* enqpkt = av_packet_clone(&avpkt);
                            if (!enqpkt)
                            {
                                cerr << "FAILED to invoke 'av_packet_clone(DemuxThreadProc)'!" << endl;
                                break;
                            }
                            lock_guard<mutex> lk(m_vidpktQLock);
                            m_vidpktQ.push_back(enqpkt);
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
                }
                else
                {
                    hasTask = false;
                }
            }
            else
            {
                cout << "Demux procedure to non-video media is NOT IMPLEMENTED yet!" << endl;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_demuxEof = true;
        if (avpktLoaded)
            av_packet_unref(&avpkt);
        cout << "Leave DemuxThreadProc()." << endl;
    }

    bool ReadNextStreamPacket(int stmIdx, AVPacket* avpkt, bool* avpktLoaded, int64_t* pts)
    {
        *avpktLoaded = false;
        int fferr;
        do {
            fferr = av_read_frame(m_avfmtCtx, avpkt);
            if (fferr == 0)
            {
                if (avpkt->stream_index == stmIdx)
                {
                    if (pts) *pts = avpkt->pts;
                    *avpktLoaded = true;
                    break;
                }
                av_packet_unref(avpkt);
            }
            else
            {
                if (fferr == AVERROR_EOF)
                {
                    if (pts) *pts = INT64_MAX;
                    break;
                }
                else
                {
                    cerr << "av_read_frame() FAILED! fferr = " << fferr << "." << endl;
                    return false;
                }
            }
        } while (fferr >= 0);
        return true;
    }

    void VideoDecodeThreadProc()
    {
        cout << "Enter VideoDecodeThreadProc()..." << endl;
        AVFrame avfrm = {0};
        bool avfrmLoaded = false;
        bool inputEof = false;
        while (!m_quitScan)
        {
            bool idleLoop = true;
            bool quitLoop = false;

            // retrieve output frame
            bool hasOutput;
            do{
                if (!avfrmLoaded)
                {
                    int fferr = avcodec_receive_frame(m_viddecCtx, &avfrm);
                    if (fferr == 0)
                    {
                        avfrmLoaded = true;
                        idleLoop = false;
                    }
                    else if (fferr != AVERROR(EAGAIN))
                    {
                        if (fferr != AVERROR_EOF)
                            cerr << "FAILED to invoke 'avcodec_receive_frame'(VideoDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
                        quitLoop = true;
                        break;
                    }
                }

                hasOutput = avfrmLoaded;
                if (avfrmLoaded)
                {
                    if (m_vidfrmQ.size() < m_vidfrmQMaxSize)
                    {
                        lock_guard<mutex> lk(m_vidfrmQLock);
                        AVFrame* enqfrm = av_frame_clone(&avfrm);
                        m_vidfrmQ.push_back(enqfrm);
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
                while (m_vidpktQ.size() > 0)
                {
                    AVPacket* avpkt = m_vidpktQ.front();
                    int fferr = avcodec_send_packet(m_viddecCtx, avpkt);
                    if (fferr == 0)
                    {
                        lock_guard<mutex> lk(m_vidpktQLock);
                        m_vidpktQ.pop_front();
                        av_packet_free(&avpkt);
                        idleLoop = false;
                    }
                    else
                    {
                        if (fferr != AVERROR(EAGAIN))
                        {
                            cerr << "FAILED to invoke 'avcodec_send_packet'(VideoDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
                            quitLoop = true;
                        }
                        break;
                    }
                }
                if (quitLoop)
                    break;

                if (m_vidpktQ.size() == 0 && m_demuxEof)
                {
                    avcodec_send_packet(m_viddecCtx, nullptr);
                    idleLoop = false;
                    inputEof = true;
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_viddecEof = true;
        if (avfrmLoaded)
            av_frame_unref(&avfrm);
        cout << "Leave VideoDecodeThreadProc()." << endl;
    }

    void AudioDecodeThreadProc()
    {
        cout << "Enter AudioDecodeThreadProc()..." << endl;
        AVFrame avfrm = {0};
        bool avfrmLoaded = false;
        bool inputEof = false;
        while (!m_quitScan)
        {
            bool idleLoop = true;
            bool quitLoop = false;

            // retrieve output frame
            bool hasOutput;
            do{
                if (!avfrmLoaded)
                {
                    int fferr = avcodec_receive_frame(m_auddecCtx, &avfrm);
                    if (fferr == 0)
                    {
                        avfrmLoaded = true;
                        idleLoop = false;
                        // update average audio frame duration, for calculating audio queue size
                        double frmDur = (double)avfrm.nb_samples/m_audStream->codecpar->sample_rate;
                        m_audfrmAvgDur = (m_audfrmAvgDur*(m_audfrmAvgDurCalcCnt-1)+frmDur)/m_audfrmAvgDurCalcCnt;
                        m_swrfrmQMaxSize = (int)ceil(m_audQDuration/m_audfrmAvgDur);
                        m_audfrmQMaxSize = (int)ceil((double)m_swrfrmQMaxSize/5);
                    }
                    else if (fferr != AVERROR(EAGAIN))
                    {
                        if (fferr != AVERROR_EOF)
                            cerr << "FAILED to invoke 'avcodec_receive_frame'(AudioDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
                        quitLoop = true;
                        break;
                    }
                }

                hasOutput = avfrmLoaded;
                if (avfrmLoaded)
                {
                    if (m_audfrmQ.size() < m_audfrmQMaxSize)
                    {
                        lock_guard<mutex> lk(m_audfrmQLock);
                        AVFrame* enqfrm = av_frame_clone(&avfrm);
                        m_audfrmQ.push_back(enqfrm);
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
                while (m_audpktQ.size() > 0)
                {
                    AVPacket* avpkt = m_audpktQ.front();
                    int fferr = avcodec_send_packet(m_auddecCtx, avpkt);
                    if (fferr == 0)
                    {
                        lock_guard<mutex> lk(m_audpktQLock);
                        m_audpktQ.pop_front();
                        av_packet_free(&avpkt);
                        idleLoop = false;
                    }
                    else
                    {
                        if (fferr != AVERROR(EAGAIN))
                        {
                            cerr << "FAILED to invoke 'avcodec_send_packet'(AudioDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
                            quitLoop = true;
                        }
                        break;
                    }
                }
                if (quitLoop)
                    break;

                if (m_audpktQ.size() == 0 && m_demuxEof)
                {
                    avcodec_send_packet(m_auddecCtx, nullptr);
                    idleLoop = false;
                    inputEof = true;
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_auddecEof = true;
        if (avfrmLoaded)
            av_frame_unref(&avfrm);
        cout << "Leave AudioDecodeThreadProc()." << endl;
    }

    void SwrThreadProc()
    {
        while (!m_quitScan)
        {
            bool idleLoop = true;
            if (!m_audfrmQ.empty())
            {
                if (m_swrfrmQ.size() < m_swrfrmQMaxSize)
                {
                    AVFrame* srcfrm = m_audfrmQ.front();;
                    AVFrame* dstfrm = nullptr;
                    if (m_swrPassThrough)
                    {
                        dstfrm = srcfrm;
                    }
                    else
                    {
                        dstfrm = av_frame_alloc();
                        if (!dstfrm)
                        {
                            m_errMessage = "FAILED to allocate new AVFrame for 'swr_convert()'!";
                            break;
                        }
                        dstfrm->format = (int)m_swrOutSmpfmt;
                        dstfrm->sample_rate = m_swrOutSampleRate;
                        dstfrm->channels = m_swrOutChannels;
                        dstfrm->channel_layout = m_swrOutChnLyt;
                        dstfrm->nb_samples = swr_get_out_samples(m_swrCtx, srcfrm->nb_samples);
                        int fferr = av_frame_get_buffer(dstfrm, 0);
                        if (fferr < 0)
                        {
                            SetFFError("av_frame_get_buffer(SwrThreadProc)", fferr);
                            break;
                        }
                        av_frame_copy_props(dstfrm, srcfrm);
                        dstfrm->pts = swr_next_pts(m_swrCtx, srcfrm->pts);
                        fferr = swr_convert(m_swrCtx, dstfrm->data, dstfrm->nb_samples, (const uint8_t **)srcfrm->data, srcfrm->nb_samples);
                        if (fferr < 0)
                        {
                            SetFFError("swr_convert(SwrThreadProc)", fferr);
                            break;
                        }
                    }
                    {
                        lock_guard<mutex> lk(m_audfrmQLock);
                        m_audfrmQ.pop_front();
                    }
                    {
                        lock_guard<mutex> lk(m_swrfrmQLock);
                        m_swrfrmQ.push_back(dstfrm);
                    }
                    if (srcfrm != dstfrm)
                        av_frame_free(&srcfrm);
                    idleLoop = false;
                }
            }
            else if (m_auddecEof)
                break;

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_swrEof = true;
    }

    void UpdateSnapshotThreadProc()
    {
        cout << "Enter UpdateSnapshotThreadProc()." << endl;
        const int MAX_CACHE_SIZE = 64;
        const int CACHE_SHRINK_SIZE = 48;
        const double MIN_CACHE_FRAME_INTERVAL = 0.5;
        list<ImGui::ImMat> vidMatCache;
        list<ImGui::ImMat>::iterator cacheIter = vidMatCache.begin();
        int64_t prevSeekPos = INT64_MIN;
        while (!m_quitScan)
        {
            bool idleLoop = true;

            if (!m_vidfrmQ.empty())
            {
                AVFrame* frm = m_vidfrmQ.front();
                int64_t mts = av_rescale_q(frm->pts, m_vidStream->time_base, MILLISEC_TIMEBASE);
                double ts = (double)mts/1000.;
                uint32_t index;
                if (IsSnapshotFrame(mts, index))
                {
                    lock_guard<mutex> lk(m_ssLock);
                    auto iter = find_if(m_snapshots.begin(), m_snapshots.end(),
                        [index](const Snapshot& ss) { return ss.index >= index; });
                    if (iter != m_snapshots.end() && iter->index == index)
                    {
                        double targetTs = index*m_snapshotInterval;
                        if (abs(iter->img.time_stamp-ts) >= m_vidfrmInterval/2 &&
                            abs(ts-targetTs) < abs(iter->img.time_stamp-targetTs))
                        {
                            cout << "WARNING! Better snapshot is found for index " << index << "(ts=" << targetTs
                                << "), new frm ts = " << ts << ", old frm ts = " << iter->img.time_stamp << "." << endl;
                            ConvertAVFrameToImMat(frm, iter->img, ts);
                        }
                    }
                    else
                    {
                        ImGui::ImMat img;
                        ConvertAVFrameToImMat(frm, img, ts);
                        Snapshot ss = { img, index, false };
                        m_snapshots.insert(iter, ss);
                        cout << "Add new snapshot (index=" << index << ", ts=" << ts << ")." << endl;

                        if (m_snapshots.size() >= m_maxCacheSize)
                        {
                            uint32_t oldSize = m_snapshots.size();
                            ShrinkSnapshots(m_snapWnd);
                            uint32_t newSize = m_snapshots.size();
                            cout << "Shrink snapshots list from " << oldSize << " to " << newSize << "." << endl;
                        }
                    }
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(1));
        }
        cout << "Leave UpdateSnapshotThreadProc()." << endl;
    }

    void StartAllThreads()
    {
        m_quitScan = false;
        m_demuxThread = thread(&MediaSource_Impl::DemuxThreadProc, this);
        if (HasVideo())
            m_viddecThread = thread(&MediaSource_Impl::VideoDecodeThreadProc, this);
        if (HasAudio())
        {
            m_auddecThread = thread(&MediaSource_Impl::AudioDecodeThreadProc, this);
            m_audswrThread = thread(&MediaSource_Impl::SwrThreadProc, this);
        }
        m_updateSsThread = thread(&MediaSource_Impl::UpdateSnapshotThreadProc, this);
    }

    void WaitAllThreadsQuit()
    {
        m_quitScan = true;
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
        if (m_audswrThread.joinable())
        {
            m_audswrThread.join();
            m_audswrThread = thread();
        }
        if (m_updateSsThread.joinable())
        {
            m_updateSsThread.join();
            m_updateSsThread = thread();
        }
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
        for (AVFrame* avfrm : m_swrfrmQ)
            av_frame_free(&avfrm);
        m_swrfrmQ.clear();
    }

    string MillisecToString(int64_t millisec)
    {
        ostringstream oss;
        if (millisec < 0)
        {
            oss << "-";
            millisec = -millisec;
        }
        uint64_t t = (uint64_t) millisec;
        uint32_t milli = (uint32_t)(t%1000); t /= 1000;
        uint32_t sec = (uint32_t)(t%60); t /= 60;
        uint32_t min = (uint32_t)(t%60); t /= 60;
        uint32_t hour = (uint32_t)t;
        oss << setfill('0') << setw(2) << hour << ':'
            << setw(2) << min << ':'
            << setw(2) << sec << '.'
            << setw(3) << milli;
        return oss.str();
    }

    struct Snapshot
    {
        ImGui::ImMat img;
        uint32_t index;
        bool fixed{false};
    };

    struct SnapWindow
    {
        int64_t mts0;
        int64_t mts1;
        uint32_t index0;
        uint32_t index1;
        double snapInterval;
    };

    struct SnapshotBuildTask
    {
        uint32_t targetSsIndex;
        int64_t startPts;
        int64_t endPts;
        list<uint32_t> usefulIndex;
    };

    bool IsSnapshotFrame(int64_t mts, uint32_t& index)
    {
        index = (int32_t)round(double(mts-m_vidStartMts)/1000./m_snapshotInterval);
        double diff = abs(index*m_snapshotInterval-mts);
        return diff <= m_vidfrmInterval/2;
    }

    bool IsSpecificSnapshotFrame(uint32_t index, int64_t mts)
    {
        double diff = abs(index*m_snapshotInterval-mts);
        return diff <= m_vidfrmInterval/2;
    }

    int64_t GetSeekPosByMts(int64_t mts)
    {
        if (m_vidKeyMtsList.empty())
            return m_vidStartMts;
        auto iter = find_if(m_vidKeyMtsList.begin(), m_vidKeyMtsList.end(),
            [mts](int64_t keyMts) { return keyMts > mts; });
        if (iter != m_vidKeyMtsList.begin())
            iter--;
        return *iter;
    }

    bool FindUnavailableSnapshot(uint32_t& index, bool searchForward)
    {
        if (m_snapshots.empty())
            return true;

        uint32_t searchIndex = index;
        auto iter = find_if(m_snapshots.begin(), m_snapshots.end(),
            [searchIndex](const Snapshot& ss) { return ss.index >= searchIndex; });
        if (iter == m_snapshots.end() || iter->index > searchIndex)
            return true;
        if (searchForward)
        {
            if (searchIndex >= m_vidMaxIndex)
                return false;
            while (searchIndex < m_vidMaxIndex)
            {
                iter++;
                searchIndex++;
                if (iter == m_snapshots.end())
                    break;
                if (iter->index > searchIndex)
                    break;
            }
            if (searchIndex == m_vidMaxIndex && iter != m_snapshots.end())
                return false;
            index = searchIndex;
            return true;
        }
        else
        {
            if (searchIndex == 0)
                return false;
            while (iter != m_snapshots.begin() && searchIndex > 0)
            {
                iter--;
                searchIndex--;
                if (iter->index < searchIndex)
                    break;
            }
            if (iter->index == searchIndex)
            {
                if (searchIndex == 0)
                    return false;
                searchIndex--;
            }
            index = searchIndex;
            return true;
        }
    }

    shared_ptr<SnapshotBuildTask> GetSnapshotBuildTask(bool next)
    {
        bool isUpdated = true;
        m_snapWndUpdated.compare_exchange_strong(isUpdated, false);
        if (!isUpdated && !next)
            return m_currBuildTask;

        SnapWindow swnd = m_snapWnd;
        SnapshotBuildTask newTask;
        lock_guard<mutex> lk(m_ssLock);
        newTask.targetSsIndex = swnd.index0;
        auto iter = m_snapshots.begin();
        while (iter != m_snapshots.end())
        {
            if (iter->index > newTask.targetSsIndex)
                break;
            else if (iter->index == newTask.targetSsIndex)
                newTask.targetSsIndex++;
            iter++;
        }
        if (newTask.targetSsIndex > swnd.index1)
        {
            // if snapshots in the window are all available, then build the snapshots before/after the window
            uint32_t prevWndTaskIndex = swnd.index0-1;
            bool prevWndHasTask = swnd.index0 == 0 ? false : FindUnavailableSnapshot(prevWndTaskIndex, false);
            uint32_t postWndTaskIndex = swnd.index1+1;
            bool postWndHasTask = swnd.index1 >= m_vidMaxIndex ? false : FindUnavailableSnapshot(postWndTaskIndex, true);
            if (prevWndHasTask && postWndHasTask)
            {
                if (swnd.index0-prevWndTaskIndex < postWndTaskIndex-swnd.index1)
                    newTask.targetSsIndex = prevWndTaskIndex;
                else
                    newTask.targetSsIndex = postWndTaskIndex;
            }
            else if (prevWndHasTask)
                newTask.targetSsIndex = prevWndTaskIndex;
            else if (postWndHasTask)
                newTask.targetSsIndex = postWndTaskIndex;
            else
            {
                m_currBuildTask = nullptr;
                cout << "No more build task!" << endl;
                return nullptr;
            }
        }
        newTask.startPts = GetSeekPosByMts((int64_t)round(newTask.targetSsIndex*swnd.snapInterval));
        m_currBuildTask = make_shared<SnapshotBuildTask>(newTask);
        return m_currBuildTask;
    }

    void ShrinkSnapshots(const SnapWindow& swnd)
    {
        auto iter0 = m_snapshots.begin();
        auto iter1 = m_snapshots.end();
        iter1--;
        while (m_snapshots.size() > m_shrinkSize)
        {
            uint32_t diff0 = 0, diff1 = 0;
            if (iter0->index < swnd.index0)
                diff0 = swnd.index0-iter0->index;
            if (iter1->index > swnd.index1)
                diff1 = iter1->index-swnd.index1;
            if (diff0 == 0 && diff1 == 0)
                break;
            if (diff1 < diff0)
                iter0 = m_snapshots.erase(iter0);
            else
            {
                iter1 = m_snapshots.erase(iter1);
                iter1--;
            }
        }
    }

private:
    string m_errMessage;
    bool m_vidPreferUseHw{true};
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
    SwrContext* m_swrCtx{nullptr};
    AVSampleFormat m_swrOutSmpfmt{AV_SAMPLE_FMT_S16};
    int m_swrOutSampleRate;
    int m_swrOutChannels;
    int64_t m_swrOutChnLyt;

    // demuxing thread
    thread m_demuxThread;
    float m_vidpktQDuration{2.0f};
    int m_vidpktQMaxSize{0};
    list<AVPacket*> m_vidpktQ;
    mutex m_vidpktQLock;
    int m_audpktQMaxSize{64};
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
    int m_audfrmQMaxSize{5};
    list<AVFrame*> m_audfrmQ;
    mutex m_audfrmQLock;
    bool m_auddecEof{false};
    double m_audfrmAvgDur{0.021};
    uint32_t m_audfrmAvgDurCalcCnt{10};
    // pcm format conversion thread
    thread m_audswrThread;
    float m_audQDuration{0.5f};
    // use 24 as queue max size is calculated by
    // 1024 samples per frame @ 48kHz, and audio queue duration is 0.5 seconds.
    // this max size will be updated while audio decoding procedure.
    int m_swrfrmQMaxSize{24};
    list<AVFrame*> m_swrfrmQ;
    mutex m_swrfrmQLock;
    bool m_swrPassThrough{false};
    bool m_swrEof{false};
    // rendering thread
    thread m_updateSsThread;
    bool m_renderEof{false};

    recursive_mutex m_ctlLock;
    bool m_quitScan{false};

    uint32_t m_ssWidth{0}, m_ssHeight{0};
    int64_t m_vidStartMts;
    uint32_t m_vidMaxIndex;
    double m_vidfrmInterval;
    double m_snapWindowSize;
    double m_snapshotInterval;
    double m_cacheFactor{10.0};
    double m_shrinkFactor{0.8};
    uint32_t m_maxCacheSize;
    uint32_t m_shrinkSize;
    shared_ptr<SnapshotBuildTask> m_currBuildTask;
    list<int64_t> m_vidKeyMtsList;
    atomic<SnapWindow> m_snapWnd;
    atomic_bool m_snapWndUpdated{false};
    list<Snapshot> m_snapshots;
    mutex m_ssLock;
    double m_fixedSsInterval;
    uint32_t m_fixedSnapshotCount{100};
};

const AVRational MediaSource_Impl::MILLISEC_TIMEBASE = { 1, 1000 };
const AVRational MediaSource_Impl::FFAV_TIMEBASE = { 1, AV_TIME_BASE };

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{
    MediaSource_Impl* ms = reinterpret_cast<MediaSource_Impl*>(ctx->opaque);
    const AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (ms->CheckHwPixFmt(*p))
            return *p;
    }
    return AV_PIX_FMT_NONE;
}
