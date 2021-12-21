#include <thread>
#include <mutex>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <list>
#include <atomic>
#include <memory>
#include <cmath>
#include <algorithm>
#include "MediaSnapshot.h"
#include "FFUtils.h"
#include "Logger.h"
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

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts);

class MediaSnapshot_Impl : public MediaSnapshot
{
public:
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
            if (m_vidStream->duration > 0)
                m_vidDuration = av_rescale_q(m_vidStream->duration, m_vidStream->time_base, MILLISEC_TIMEBASE);
            else
                m_vidDuration = av_rescale_q(m_avfmtCtx->duration, FFAV_TIMEBASE, MILLISEC_TIMEBASE);
            if (m_vidStream->nb_frames > 0)
                m_vidFrameCount = m_vidStream->nb_frames;
            else if (m_vidStream->r_frame_rate.den > 0)
                m_vidFrameCount = m_vidDuration / 1000.f * m_vidStream->r_frame_rate.num / m_vidStream->r_frame_rate.den;
            else if (m_vidStream->avg_frame_rate.den > 0)
                m_vidFrameCount = m_vidDuration / 1000.f * m_vidStream->avg_frame_rate.num / m_vidStream->avg_frame_rate.den;

            m_vidfrmIntvMts = av_q2d(av_inv_q(m_vidStream->avg_frame_rate))*1000.;
            m_vidfrmIntvMtsHalf = ceil(m_vidfrmIntvMts)/2;
            if (m_vidStream->avg_frame_rate.num*m_vidStream->time_base.num > 0)
                m_vidfrmIntvPts = (m_vidStream->avg_frame_rate.den*m_vidStream->time_base.den)/(m_vidStream->avg_frame_rate.num*m_vidStream->time_base.num);
            else
                m_vidfrmIntvPts = 0;
            m_snapWnd.startPos = (double)m_vidStartMts/1000.;
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

        m_audfrmQMaxSize = 5;
        m_swrfrmQMaxSize = 24;
        m_audfrmAvgDur = 0.021;

        m_vidKeyPtsList.clear();

        m_errMessage = "";
    }

    bool GetSnapshots(std::vector<ImGui::ImMat>& snapshots, double startPos) override
    {
        if (!IsOpened())
            return false;

        SnapWindow snapWnd = UpdateSnapWindow(startPos);

        snapshots.clear();
        lock_guard<mutex> lk(m_ssLock);
        auto iter = find_if(m_snapshots.begin(), m_snapshots.end(),
            [snapWnd](const Snapshot& ss) { return ss.index >= snapWnd.index0; });
        uint32_t i = snapWnd.index0;
        while (i <= snapWnd.index1)
        {
            if (iter == m_snapshots.end() || iter->index > i)
            {
                ImGui::ImMat vmat;
                vmat.time_stamp = CalcSnapshotTimestamp(i);
                snapshots.push_back(vmat);
            }
            else
            {
                snapshots.push_back(iter->img);
                iter++;
            }
            i++;
        }
        return true;
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

    int64_t GetVidoeMinPos() const override
    {
        return m_vidStartMts;
    }

    int64_t GetVidoeDuration() const override
    {
        return m_vidDuration;
    }

    int64_t GetVidoeFrameCount() const override
    {
        return m_vidFrameCount;
    }
    
    uint32_t GetVideoWidth() const override
    {
        if (m_vidStream)
        {
            return m_vidStream->codecpar->width;
        }
        return 0;
    }

    uint32_t GetVideoHeight() const override
    {
        if (m_vidStream)
        {
            return m_vidStream->codecpar->height;
        }
        return 0;
    }

    bool ConfigSnapWindow(double& windowSize, double frameCount) override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (frameCount < 1)
        {
            m_errMessage = "Argument 'frameCount' must be greater than 1!";
            return false;
        }
        double minWndSize = CalcMinWindowSize(frameCount);
        if (windowSize < minWndSize)
            windowSize = minWndSize;
        double maxWndSize = GetMaxWindowSize();
        if (windowSize > maxWndSize)
            windowSize = maxWndSize;
        if (m_snapWindowSize == windowSize && m_windowFrameCount == frameCount)
            return true;

        WaitAllThreadsQuit();
        FlushAllQueues();
        if (m_viddecCtx)
            avcodec_flush_buffers(m_viddecCtx);

        {
            lock_guard<mutex> lk2(m_ssLock);
            m_snapshots.clear();
        }

        m_snapWindowSize = windowSize;
        m_windowFrameCount = frameCount;
        m_snapshotInterval = m_snapWindowSize*1000./m_windowFrameCount;
        m_vidMaxIndex = (uint32_t)floor((double)m_vidDuration/m_snapshotInterval)+1;
        m_maxCacheSize = (uint32_t)ceil(m_windowFrameCount*m_cacheFactor);
        uint32_t intWndFrmCnt = (uint32_t)ceil(m_windowFrameCount);
        if (m_maxCacheSize < intWndFrmCnt)
            m_maxCacheSize = intWndFrmCnt;
        if (m_maxCacheSize > m_vidMaxIndex+1)
            m_maxCacheSize = m_vidMaxIndex+1;
        m_prevWndCacheSize = (m_maxCacheSize-intWndFrmCnt)/2;
        m_postWndCacheSize = m_maxCacheSize-intWndFrmCnt-m_prevWndCacheSize;

        UpdateSnapWindow(m_snapWnd.startPos);
        InitializeSnapshotBuildTask();
        m_snapWndUpdated = false;

        Log(DEBUG) << ">>>> Config window: m_snapWindowSize=" << m_snapWindowSize << ", m_windowFrameCount=" << m_windowFrameCount
            << ", m_vidMaxIndex=" << m_vidMaxIndex << ", m_maxCacheSize=" << m_maxCacheSize << ", m_prevWndCacheSize=" << m_prevWndCacheSize << endl;

        StartAllThreads();
        return true;
    }

    double GetMinWindowSize() const override
    {
        return CalcMinWindowSize(m_windowFrameCount);
    }

    double GetMaxWindowSize() const override
    {
        return (double)m_vidDuration/1000.;
    }

    bool SetSnapshotSize(uint32_t width, uint32_t height) override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (m_frmCvt.GetOutWidth() == width && m_frmCvt.GetOutHeight() == height)
            return true;
        if (!m_frmCvt.SetOutSize(width, height))
        {
            m_errMessage = m_frmCvt.GetError();
            return false;
        }
        lock_guard<mutex> lk2(m_ssLock);
        m_snapshots.clear();
        return true;
    }

    bool SetSnapshotResizeFactor(float widthFactor, float heightFactor) override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (widthFactor <= 0.f || heightFactor <= 0.f)
        {
            m_errMessage = "Resize factor must be a positive number!";
            return false;
        }
        if (!m_ssSizeChanged && m_ssWFacotr == widthFactor && m_ssHFacotr == heightFactor)
            return true;

        m_ssWFacotr = widthFactor;
        m_ssHFacotr = heightFactor;
        if (HasVideo())
        {
            if (widthFactor == 1.f && heightFactor == 1.f)
                return SetSnapshotSize(0, 0);

            uint32_t outWidth = (uint32_t)ceil(m_vidStream->codecpar->width*widthFactor);
            if ((outWidth&0x1) == 1)
                outWidth++;
            uint32_t outHeight = (uint32_t)ceil(m_vidStream->codecpar->height*heightFactor);
            if ((outHeight&0x1) == 1)
                outHeight++;
            return SetSnapshotSize(outWidth, outHeight);
        }
        m_ssSizeChanged = false;
        return true;
    }

    bool SetOutColorFormat(ImColorFormat clrfmt) override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (m_frmCvt.GetOutColorFormat() == clrfmt)
            return true;
        if (!m_frmCvt.SetOutColorFormat(clrfmt))
        {
            m_errMessage = m_frmCvt.GetError();
            return false;
        }
        lock_guard<mutex> lk2(m_ssLock);
        m_snapshots.clear();
        return true;
    }

    bool SetResizeInterpolateMode(ImInterpolateMode interp) override
    {
        lock_guard<recursive_mutex> lk(m_ctlLock);
        if (m_frmCvt.GetResizeInterpolateMode() == interp)
            return true;
        if (!m_frmCvt.SetResizeInterpolateMode(interp))
        {
            m_errMessage = m_frmCvt.GetError();
            return false;
        }
        lock_guard<mutex> lk2(m_ssLock);
        m_snapshots.clear();
        return true;
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

    double CalcMinWindowSize(double windowFrameCount) const
    {
        return m_vidfrmIntvMts*windowFrameCount/1000.;
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
        Log(DEBUG) << "Open '" << url << "' successfully. " << m_avfmtCtx->nb_streams << " streams are found." << endl;

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
        }
        if (m_audStream)
        {
            if (!OpenAudioDecoder())
                return false;
        }
        if (!ParseFile())
            return false;
        m_ssSizeChanged = true;
        if (!SetSnapshotResizeFactor(m_ssWFacotr, m_ssHFacotr))
            return false;
        return true;
    }

    bool ParseFile()
    {
        int fferr = 0;
        int64_t lastKeyPts = m_vidStream ? m_vidStream->start_time-2 : 0;
        while (true)
        {
            fferr = avformat_seek_file(m_avfmtCtx, m_vidStmIdx, lastKeyPts+1, lastKeyPts+1, INT64_MAX, 0);
            if (fferr < 0)
                Log(ERROR) << "avformat_seek_file(IN ParseFile) FAILED with fferr = " << fferr << "!" << endl;
            AVPacket avpkt = {0};
            do {
                fferr = av_read_frame(m_avfmtCtx, &avpkt);
                if (fferr == 0)
                {
                    if (avpkt.stream_index == m_vidStmIdx)
                    {
                        lastKeyPts = avpkt.pts;
                        av_packet_unref(&avpkt);
                        break;
                    }
                    av_packet_unref(&avpkt);
                }
            } while (fferr >= 0);
            if (fferr == 0)
                m_vidKeyPtsList.push_back(lastKeyPts);
            else
            {
                if (fferr != AVERROR_EOF)
                    Log(ERROR) << "Read frame from file FAILED! fferr = " << fferr << "." << endl;
                break;
            }
        }
        if (fferr != AVERROR_EOF)
            return false;

        Log(DEBUG) << "Parse key frames done. " << m_vidKeyPtsList.size() << " key frames are found." << endl;
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
        Log(DEBUG) << "Video decoder '" << m_viddec->name << "' opened." << " thread_count=" << m_viddecCtx->thread_count
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
        Log(DEBUG) << "Use hardware device type '" << av_hwdevice_get_type_name(m_viddecDevType) << "'." << endl;

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
        Log(DEBUG) << "Video decoder(HW) '" << m_viddecCtx->codec->name << "' opened." << endl;
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
        Log(DEBUG) << "Audio decoder '" << m_auddec->name << "' opened." << endl;

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
        Log(DEBUG) << "Enter DemuxThreadProc()..." << endl;
        bool fatalError = false;
        AVPacket avpkt = {0};
        bool avpktLoaded = false;
        SnapshotBuildTask currTask = nullptr;
        bool demuxEof = false;
        while (!m_quitScan)
        {
            bool idleLoop = true;

            UpdateSnapshotBuildTask();

            if (HasVideo())
            {
                bool taskChanged = false;
                if (!currTask || currTask->cancel || currTask->demuxerEof)
                {
                    currTask = FindNextDemuxTask();
                    if (currTask)
                    {
                        currTask->demuxing = true;
                        taskChanged = true;
                        Log(DEBUG) << "--> build ssTask updated, ssIdxAry=(" << currTask->ssIdxAry.front() << " ~ " << currTask->ssIdxAry.back()
                            << ", startPts=" << currTask->seekPts.first << "(" << av_rescale_q(currTask->seekPts.first, m_vidStream->time_base, MILLISEC_TIMEBASE) << ")"
                            << ", endPts=" << currTask->seekPts.second << "(" << av_rescale_q(currTask->seekPts.second, m_vidStream->time_base, MILLISEC_TIMEBASE) << ")" << endl;
                    }
                }

                if (currTask)
                {
                    if (taskChanged)
                    {
                        if (!avpktLoaded || avpkt.pts != currTask->seekPts.first)
                        {
                            if (avpktLoaded)
                            {
                                av_packet_unref(&avpkt);
                                avpktLoaded = false;
                            }
                            int fferr = avformat_seek_file(m_avfmtCtx, m_vidStmIdx, INT64_MIN, currTask->seekPts.first, currTask->seekPts.first, 0);
                            if (fferr < 0)
                            {
                                Log(ERROR) << "avformat_seek_file() FAILED for seeking to 'currTask->startPts'(" << currTask->seekPts.first << ")! fferr = " << fferr << "!" << endl;
                                fatalError = true;
                                break;
                            }
                            demuxEof = false;
                            int64_t ptsAfterSeek = INT64_MIN;
                            if (!ReadNextStreamPacket(m_vidStmIdx, &avpkt, &avpktLoaded, &ptsAfterSeek))
                            {
                                fatalError = true;
                                break;
                            }
                            if (ptsAfterSeek == INT64_MAX)
                                demuxEof = true;
                            else if (ptsAfterSeek != currTask->seekPts.first)
                            {
                                Log(DEBUG) << "WARNING! 'ptsAfterSeek'(" << ptsAfterSeek << ") != 'ssTask->startPts'(" << currTask->seekPts.first << ")!" << endl;
                                currTask->seekPts.first = ptsAfterSeek;
                            }
                        }
                    }

                    if (!demuxEof && !avpktLoaded)
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
                            {
                                currTask->demuxerEof = true;
                                demuxEof = true;
                            }
                            else
                                Log(ERROR) << "Demuxer ERROR! 'av_read_frame' returns " << fferr << "." << endl;
                        }
                    }

                    if (avpktLoaded && avpkt.stream_index == m_vidStmIdx)
                    {
                        if (avpkt.pts >= currTask->seekPts.second)
                            currTask->demuxerEof = true;

                        if (!currTask->demuxerEof)
                        {
                            AVPacket* enqpkt = av_packet_clone(&avpkt);
                            if (!enqpkt)
                            {
                                Log(ERROR) << "FAILED to invoke 'av_packet_clone(DemuxThreadProc)'!" << endl;
                                break;
                            }
                            {
                                lock_guard<mutex> lk(currTask->avpktQLock);
                                currTask->avpktQ.push_back(enqpkt);
                            }
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
            }
            else
            {
                Log(ERROR) << "Demux procedure to non-video media is NOT IMPLEMENTED yet!" << endl;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (currTask && !currTask->demuxerEof)
            currTask->demuxerEof = true;
        if (avpktLoaded)
            av_packet_unref(&avpkt);
        Log(DEBUG) << "Leave DemuxThreadProc()." << endl;
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
                    Log(ERROR) << "av_read_frame() FAILED! fferr = " << fferr << "." << endl;
                    return false;
                }
            }
        } while (fferr >= 0);
        return true;
    }

    void VideoDecodeThreadProc()
    {
        Log(DEBUG) << "Enter VideoDecodeThreadProc()..." << endl;
        SnapshotBuildTask currTask;
        AVFrame avfrm = {0};
        bool avfrmLoaded = false;
        bool inputEof = false;
        while (!m_quitScan)
        {
            bool idleLoop = true;
            bool quitLoop = false;

            if (currTask && currTask->cancel)
            {
                if (!currTask->decoderEof)
                    avcodec_flush_buffers(m_viddecCtx);
                if (avfrmLoaded)
                {
                    av_frame_unref(&avfrm);
                    avfrmLoaded = false;
                }
                currTask = nullptr;
            }

            if (!currTask || currTask->decoderEof)
            {
                currTask = FindNextDecoderTask();
                if (currTask)
                {
                    currTask->decoding = true;
                    inputEof = false;
                }
            }

            if (currTask)
            {
                // retrieve output frame
                bool hasOutput;
                do{
                    if (!avfrmLoaded)
                    {
                        int fferr = avcodec_receive_frame(m_viddecCtx, &avfrm);
                        if (fferr == 0)
                        {
                            // Log(DEBUG) << "<<< Get video frame pts=" << avfrm.pts << "(" << MillisecToString(av_rescale_q(avfrm.pts, m_vidStream->time_base, MILLISEC_TIMEBASE)) << ")." << endl;
                            avfrmLoaded = true;
                            idleLoop = false;
                        }
                        else if (fferr != AVERROR(EAGAIN))
                        {
                            if (fferr != AVERROR_EOF)
                            {
                                Log(ERROR) << "FAILED to invoke 'avcodec_receive_frame'(VideoDecodeThreadProc)! return code is "
                                    << fferr << "." << endl;
                                quitLoop = true;
                            }
                            else
                            {
                                idleLoop = false;
                                currTask->decoderEof = true;
                                avcodec_flush_buffers(m_viddecCtx);
                                // Log(DEBUG) << "Video decoder current task reaches EOF!" << endl;
                            }
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
                    if (!currTask->avpktQ.empty())
                    {
                        AVPacket* avpkt = currTask->avpktQ.front();
                        if (!avpkt)
                            Log(DEBUG) << "----------> ERROR! null avpacket ptr got from m_vidpktQ." << endl;
                        int fferr = avcodec_send_packet(m_viddecCtx, avpkt);
                        if (fferr == 0)
                        {
                            // Log(DEBUG) << ">>> Send video packet pts=" << avpkt->pts << "(" << MillisecToString(av_rescale_q(avpkt->pts, m_vidStream->time_base, MILLISEC_TIMEBASE)) << ")." << endl;
                            {
                                lock_guard<mutex> lk(currTask->avpktQLock);
                                currTask->avpktQ.pop_front();
                            }
                            av_packet_free(&avpkt);
                            idleLoop = false;
                            if (currTask->avpktQ.empty() && currTask->demuxerEof)
                            {
                                inputEof = true;
                                // Log(DEBUG) << ">>> Send NULL packet to Video decoder." << endl;
                                fferr = avcodec_send_packet(m_viddecCtx, nullptr);
                                if (fferr < 0)
                                    Log(ERROR) << ">>> FAILED to send NULL packet to Video decoder! fferr = " << fferr << "." << endl;
                            }
                        }
                        else if (fferr != AVERROR(EAGAIN))
                        {
                            Log(ERROR) << "FAILED to invoke 'avcodec_send_packet'(VideoDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
                            quitLoop = true;
                        }
                    }
                    if (quitLoop)
                        break;
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (currTask && !currTask->decoderEof)
            currTask->decoderEof = true;
        if (avfrmLoaded)
            av_frame_unref(&avfrm);
        Log(DEBUG) << "Leave VideoDecodeThreadProc()." << endl;
    }

    void AudioDecodeThreadProc()
    {
        Log(DEBUG) << "Enter AudioDecodeThreadProc()..." << endl;
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
                            Log(ERROR) << "FAILED to invoke 'avcodec_receive_frame'(AudioDecodeThreadProc)! return code is "
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
                            Log(ERROR) << "FAILED to invoke 'avcodec_send_packet'(AudioDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
                            quitLoop = true;
                        }
                        break;
                    }
                }
                if (quitLoop)
                    break;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (avfrmLoaded)
            av_frame_unref(&avfrm);
        Log(DEBUG) << "Leave AudioDecodeThreadProc()." << endl;
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

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_swrEof = true;
    }

    void UpdateSnapshotThreadProc()
    {
        Log(DEBUG) << "Enter UpdateSnapshotThreadProc()." << endl;
        while (!m_quitScan)
        {
            bool idleLoop = true;

            if (!m_vidfrmQ.empty())
            {
                AVFrame* frm = m_vidfrmQ.front();
                {
                    lock_guard<mutex> lk(m_vidfrmQLock);
                    m_vidfrmQ.pop_front();
                }
                int64_t mts = av_rescale_q(frm->pts, m_vidStream->time_base, MILLISEC_TIMEBASE);
                double ts = (double)mts/1000.;
                uint32_t index;
                bool isSnapshot = IsSnapshotFrame(mts, index, frm->pts);
                SnapWindow snapWnd = m_snapWnd;
                if (isSnapshot && index >= snapWnd.cacheIdx0 && index <= snapWnd.cacheIdx1)
                {
                    lock_guard<mutex> lk(m_ssLock);
                    auto iter = find_if(m_snapshots.begin(), m_snapshots.end(),
                        [index](const Snapshot& ss) { return ss.index >= index; });
                    if (iter != m_snapshots.end() && iter->index == index)
                    {
                        double targetTs = index*m_snapshotInterval/1000.;
                        if (abs(iter->img.time_stamp-ts) >= m_vidfrmIntvMtsHalf &&
                            abs(ts-targetTs) < abs(iter->img.time_stamp-targetTs))
                        {
                            Log(DEBUG) << "WARNING! Better snapshot is found for index " << index << "(ts=" << targetTs
                                << "), new frm ts = " << ts << ", old frm ts = " << iter->img.time_stamp << "." << endl;
                            if (!m_frmCvt.ConvertImage(frm, iter->img, ts))
                                Log(ERROR) << "FAILED to convert AVFrame to ImGui::ImMat! Message is '" << m_frmCvt.GetError() << "'." << endl;
                        }
                    }
                    else
                    {
                        ImGui::ImMat img;
                        if (!m_frmCvt.ConvertImage(frm, img, ts))
                            Log(ERROR) << "FAILED to convert AVFrame to ImGui::ImMat! Message is '" << m_frmCvt.GetError() << "'." << endl;
                        else
                        {
                            Snapshot ss = { img, index, false };
                            m_snapshots.insert(iter, ss);
                            Log(DEBUG) << "!!! Add new snapshot [index=" << index << ", ts=" << MillisecToString((int64_t)(ts*1000)) << "]." << endl;
                        }
                        if (m_snapshots.size() > m_maxCacheSize)
                        {
                            uint32_t oldSize = m_snapshots.size();
                            ShrinkSnapshots(snapWnd);
                            uint32_t newSize = m_snapshots.size();
                            Log(DEBUG) << "Shrink snapshots list from " << oldSize << " to " << newSize << "." << endl;
                        }
                    }
                }
                av_frame_free(&frm);
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(1));
        }
        Log(DEBUG) << "Leave UpdateSnapshotThreadProc()." << endl;
    }

    void StartAllThreads()
    {
        m_quitScan = false;
        m_demuxThread = thread(&MediaSnapshot_Impl::DemuxThreadProc, this);
        if (HasVideo())
            m_viddecThread = thread(&MediaSnapshot_Impl::VideoDecodeThreadProc, this);
        if (HasAudio())
        {
            m_auddecThread = thread(&MediaSnapshot_Impl::AudioDecodeThreadProc, this);
            m_audswrThread = thread(&MediaSnapshot_Impl::SwrThreadProc, this);
        }
        m_updateSsThread = thread(&MediaSnapshot_Impl::UpdateSnapshotThreadProc, this);
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
        m_bldtskByPriOrder.clear();
        m_bldtskTimeOrder.clear();
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

    struct Snapshot
    {
        ImGui::ImMat img;
        uint32_t index;
        bool fixed{false};
    };

    struct SnapWindow
    {
        double startPos;
        uint32_t index0;
        uint32_t index1;
        uint32_t cacheIdx0;
        uint32_t cacheIdx1;
    };

    struct _SnapshotBuildTask
    {
        ~_SnapshotBuildTask()
        {
            for (AVPacket* avpkt : avpktQ)
                av_packet_free(&avpkt);
        }

        list<uint32_t> ssIdxAry;
        vector<int64_t> ssPtsAry;
        pair<int64_t, int64_t> seekPts;
        list<AVPacket*> avpktQ;
        mutex avpktQLock;
        bool demuxing{false};
        bool demuxerEof{false};
        bool decoding{false};
        bool decoderEof{false};
        bool cancel{false};
    };
    using SnapshotBuildTask = shared_ptr<_SnapshotBuildTask>;

    SnapWindow UpdateSnapWindow(double startPos)
    {
        int64_t mts0 = (int64_t)round(startPos*1000.);
        if (mts0 < m_vidStartMts)
            mts0 = m_vidStartMts;
        uint32_t index0 = (int32_t)floor(double(mts0-m_vidStartMts)/m_snapshotInterval);
        int64_t mts1 = (int64_t)round((startPos+m_snapWindowSize)*1000.);
        uint32_t index1 = (int32_t)floor(double(mts1-m_vidStartMts)/m_snapshotInterval);
        if (index1 > m_vidMaxIndex)
            index1 = m_vidMaxIndex;
        uint32_t cacheIdx0 = index0 > m_prevWndCacheSize ? index0-m_prevWndCacheSize : 0;
        uint32_t cacheIdx1 = cacheIdx0+m_maxCacheSize-1;
        if (cacheIdx1 > m_vidMaxIndex)
        {
            cacheIdx1 = m_vidMaxIndex;
            cacheIdx0 = cacheIdx1+1-m_maxCacheSize;
        }
        SnapWindow snapWnd = m_snapWnd;
        if (snapWnd.index0 != index0 || snapWnd.index1 != index1 || snapWnd.cacheIdx0 != cacheIdx0 || snapWnd.cacheIdx1 != cacheIdx1)
            m_snapWndUpdated = true;
        snapWnd = { startPos, index0, index1, cacheIdx0, cacheIdx1 };
        if (!memcpy(&m_snapWnd, &snapWnd, sizeof(m_snapWnd)))
        {
            m_snapWnd = snapWnd;
            Log(DEBUG) << "Update snap-window: { " << startPos << "(" << MillisecToString((int64_t)(startPos*1000)) << "), "
                << index0 << ", " << index1 << ", " << cacheIdx0 << ", " << cacheIdx1 << " }" << endl;
        }
        return snapWnd;
    }

    bool IsSnapshotFrame(int64_t mts, uint32_t& index, int64_t pts)
    {
        index = (int32_t)round(double(mts-m_vidStartMts)/m_snapshotInterval);
        double diff = abs(index*m_snapshotInterval-mts);
        bool isSs = diff <= m_vidfrmIntvMtsHalf;
        // Log(DEBUG) << "---> IsSnapshotFrame : pts=" << pts << ", mts=" << mts << ", index=" << index << ", diff=" << diff << ", m_vidfrmIntvMtsHalf=" << m_vidfrmIntvMtsHalf << endl;
        return isSs;
    }

    bool IsSpecificSnapshotFrame(uint32_t index, int64_t mts)
    {
        double diff = abs(index*m_snapshotInterval-mts);
        return diff <= m_vidfrmIntvMtsHalf;
    }

    double CalcSnapshotTimestamp(uint32_t index)
    {
        uint32_t frameCount = (uint32_t)round(index*m_snapshotInterval/m_vidfrmIntvMts);
        return (frameCount*m_vidfrmIntvMts+m_vidStartMts)/1000.;
    }

    int64_t CalcSnapshotMts(uint32_t index)
    {
        uint32_t frameCount = (uint32_t)round(index*m_snapshotInterval/m_vidfrmIntvMts);
        return (int64_t)(frameCount*m_vidfrmIntvMts)+m_vidStartMts;
    }

    pair<int64_t, int64_t> GetSeekPosByMts(int64_t mts)
    {
        if (m_vidKeyPtsList.empty())
            return { mts, INT64_MAX };
        int64_t targetPts = av_rescale_q(mts, MILLISEC_TIMEBASE, m_vidStream->time_base);
        auto iter = find_if(m_vidKeyPtsList.begin(), m_vidKeyPtsList.end(),
            [targetPts](int64_t keyPts) { return keyPts > targetPts; });
        if (iter != m_vidKeyPtsList.begin())
            iter--;
        int64_t first = *iter++;
        int64_t second = iter == m_vidKeyPtsList.end() ? INT64_MAX : *iter;
        if (targetPts >= second || (second-targetPts) < m_vidfrmIntvPts/2)
        {
            first = second;
            iter++;
            second = iter == m_vidKeyPtsList.end() ? INT64_MAX : *iter;
        }
        return { first, second };
    }

    pair<int64_t, int64_t> GetSeekPosBySsIndex(uint32_t index)
    {
        return GetSeekPosByMts(CalcSnapshotMts(index));
    }

    void InitializeSnapshotBuildTask()
    {
        if (!m_bldtskTimeOrder.empty())
        {
            for (auto& tsk : m_bldtskTimeOrder)
                tsk->cancel = true;
            m_bldtskTimeOrder.clear();
        }
        SnapWindow currwnd = m_snapWnd;
        uint32_t buildIndex0 = currwnd.cacheIdx0;
        uint32_t buildIndex1 = currwnd.cacheIdx1;
        SnapshotBuildTask task = nullptr;
        while (buildIndex0 <= buildIndex1)
        {
            pair<int64_t, int64_t> seekPos = GetSeekPosBySsIndex(buildIndex0);
            if (!task || task->seekPts != seekPos)
            {
                if (task)
                    m_bldtskTimeOrder.push_back(task);
                task = make_shared<_SnapshotBuildTask>();
                task->seekPts = seekPos;
            }
            task->ssIdxAry.push_back(buildIndex0);
            buildIndex0++;
        }
        if (task)
            m_bldtskTimeOrder.push_back(task);
        m_bldtskSnapWnd = currwnd;

        UpdateBuildTaskByPriority();
    }

    void UpdateSnapshotBuildTask()
    {
        SnapWindow currwnd = m_snapWnd;
        if (currwnd.cacheIdx0 != m_bldtskSnapWnd.cacheIdx0)
        {
            SnapshotBuildTask task = nullptr;
            if (currwnd.cacheIdx0 > m_bldtskSnapWnd.cacheIdx0)
            {
                uint32_t buildIndex0 = currwnd.cacheIdx0;
                uint32_t buildIndex1 = currwnd.cacheIdx1;
                if (currwnd.cacheIdx0 <= m_bldtskSnapWnd.cacheIdx1)
                {
                    buildIndex0 = m_bldtskSnapWnd.cacheIdx1+1;
                    auto iter = m_bldtskTimeOrder.begin();
                    while (iter != m_bldtskTimeOrder.end())
                    {
                        auto& tsk = *iter;
                        if (tsk->ssIdxAry.back() < currwnd.cacheIdx0)
                        {
                            tsk->cancel = true;
                            iter = m_bldtskTimeOrder.erase(iter);
                        }
                        else
                            break;
                    }
                    task = m_bldtskTimeOrder.back();
                    m_bldtskTimeOrder.pop_back();
                }
                else
                {
                    for (auto& tsk : m_bldtskTimeOrder)
                        tsk->cancel = true;
                    m_bldtskTimeOrder.clear();
                }

                while (buildIndex0 <= buildIndex1)
                {
                    pair<int64_t, int64_t> seekPos = GetSeekPosBySsIndex(buildIndex0);
                    if (!task || task->seekPts != seekPos)
                    {
                        if (task)
                            m_bldtskTimeOrder.push_back(task);
                        task = make_shared<_SnapshotBuildTask>();
                        task->seekPts = seekPos;
                    }
                    task->ssIdxAry.push_back(buildIndex0);
                    buildIndex0++;
                }
                if (task)
                    m_bldtskTimeOrder.push_back(task);
            }
            else //(currwnd.cacheIdx0 < m_bldtskSnapWnd.cacheIdx0)
            {
                uint32_t buildIndex0 = currwnd.cacheIdx0;
                uint32_t buildIndex1 = currwnd.cacheIdx1;
                if (currwnd.cacheIdx1 >= m_bldtskSnapWnd.cacheIdx0)
                {
                    buildIndex1 = m_bldtskSnapWnd.cacheIdx0-1;
                    auto iter = m_bldtskTimeOrder.end();
                    iter--;
                    while (iter != m_bldtskTimeOrder.begin())
                    {
                        auto& tsk = *iter;
                        if (tsk->ssIdxAry.front() > currwnd.cacheIdx1)
                        {
                            tsk->cancel = true;
                            iter = m_bldtskTimeOrder.erase(iter);
                            iter--;
                        }
                        else
                            break;
                    }
                    task = m_bldtskTimeOrder.front();
                    m_bldtskTimeOrder.pop_front();
                }
                else
                {
                    for (auto& tsk : m_bldtskTimeOrder)
                        tsk->cancel = true;
                    m_bldtskTimeOrder.clear();
                }

                while (buildIndex1 >= buildIndex0)
                {
                    pair<int64_t, int64_t> seekPos = GetSeekPosBySsIndex(buildIndex1);
                    if (!task || task->seekPts != seekPos)
                    {
                        if (task)
                            m_bldtskTimeOrder.push_front(task);
                        task = make_shared<_SnapshotBuildTask>();
                        task->seekPts = seekPos;
                    }
                    task->ssIdxAry.push_front(buildIndex1);
                    if (buildIndex1 == 0)
                        break;
                    buildIndex1--;
                }
                if (task)
                    m_bldtskTimeOrder.push_front(task);
            }
        }
        bool windowPosChanged = currwnd.index0 != m_bldtskSnapWnd.index0;
        m_bldtskSnapWnd = currwnd;

        if (windowPosChanged)
            UpdateBuildTaskByPriority();
    }

    void UpdateBuildTaskByPriority()
    {
        lock_guard<mutex> lk(m_bldtskQLock);
        SnapWindow swnd = m_bldtskSnapWnd;
        m_bldtskByPriOrder = m_bldtskTimeOrder;
        m_bldtskByPriOrder.sort([swnd](const SnapshotBuildTask& a, const SnapshotBuildTask& b) {
            uint32_t aFront = a->ssIdxAry.front();
            uint32_t aBack = a->ssIdxAry.back();
            bool aInDisplayWindow = aFront >= swnd.index0 && aFront <= swnd.index1 ||
                aBack >= swnd.index0 && aBack <= swnd.index1;
            uint32_t bFront = b->ssIdxAry.front();
            uint32_t bBack = b->ssIdxAry.back();
            bool bInDisplayWindow = bFront >= swnd.index0 && bFront <= swnd.index1 ||
                bBack >= swnd.index0 && bBack <= swnd.index1;
            if (aInDisplayWindow && bInDisplayWindow)
                return aFront < bFront;
            else if (aInDisplayWindow)
                return true;
            else if (bInDisplayWindow)
                return false;
            else
            {
                uint32_t aDistance = aFront > swnd.index1 ? aFront-swnd.index1 : swnd.index0-aBack;
                uint32_t bDistance = bFront > swnd.index1 ? bFront-swnd.index1 : swnd.index0-bBack;
                return aDistance < bDistance;
            }
        });
    }

    SnapshotBuildTask FindNextDemuxTask()
    {
        SnapshotBuildTask nxttsk = nullptr;
        uint32_t pendingTaskCnt = 0;
        for (auto& tsk : m_bldtskByPriOrder)
            if (!tsk->demuxing)
            {
                nxttsk = tsk;
                break;
            }
            else if (!tsk->decoding)
            {
                pendingTaskCnt++;
                if (pendingTaskCnt > m_maxPendingTaskCountForDecoding)
                    break;
            }
        return nxttsk;
    }

    SnapshotBuildTask FindNextDecoderTask()
    {
        lock_guard<mutex> lk(m_bldtskQLock);
        SnapshotBuildTask nxttsk = nullptr;
        for (auto& tsk : m_bldtskByPriOrder)
            if (!tsk->decoding)
            {
                nxttsk = tsk;
                break;
            }
        return nxttsk;
    }

    void ShrinkSnapshots(const SnapWindow& swnd)
    {
        size_t oldsize, newsize;
        oldsize = m_snapshots.size();
        auto iter0 = m_snapshots.begin();
        auto iter1 = m_snapshots.end();
        iter1--;
        while (m_snapshots.size() > m_maxCacheSize)
        {
            uint32_t diff0 = 0, diff1 = 0;
            if (iter0->index < swnd.cacheIdx0)
                diff0 = swnd.cacheIdx0-iter0->index;
            if (iter1->index > swnd.cacheIdx1)
                diff1 = iter1->index-swnd.cacheIdx1;
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
        newsize = m_snapshots.size();
        Log(DEBUG) << "XXX Shrink snapshots size " << oldsize << " -> " << newsize << "." << endl;
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
#if LIBAVFORMAT_VERSION_MAJOR >= 59
    const AVCodec* m_viddec{nullptr};
    const AVCodec* m_auddec{nullptr};
#else
    AVCodec* m_viddec{nullptr};
    AVCodec* m_auddec{nullptr};
#endif
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
    mutex m_bldtskQLock;
    uint32_t m_maxPendingTaskCountForDecoding = 8;
    int m_audpktQMaxSize{64};
    list<AVPacket*> m_audpktQ;
    mutex m_audpktQLock;
    // video decoding thread
    thread m_viddecThread;
    int m_vidfrmQMaxSize{4};
    list<AVFrame*> m_vidfrmQ;
    mutex m_vidfrmQLock;
    // audio decoding thread
    thread m_auddecThread;
    int m_audfrmQMaxSize{5};
    list<AVFrame*> m_audfrmQ;
    mutex m_audfrmQLock;
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
    // update snapshots thread
    thread m_updateSsThread;

    recursive_mutex m_ctlLock;
    bool m_quitScan{false};

    float m_ssWFacotr{1.f}, m_ssHFacotr{1.f};
    bool m_ssSizeChanged{false};
    int64_t m_vidStartMts {0};
    int64_t m_vidDuration {0};
    int64_t m_vidFrameCount {0};
    uint32_t m_vidMaxIndex;
    double m_snapWindowSize;
    double m_windowFrameCount;
    double m_vidfrmIntvMts;
    double m_vidfrmIntvMtsHalf;
    int64_t m_vidfrmIntvPts;
    double m_snapshotInterval;
    double m_cacheFactor{10.0};
    list<Snapshot> m_snapshots;
    uint32_t m_maxCacheSize, m_prevWndCacheSize, m_postWndCacheSize;
    SnapWindow m_snapWnd;
    atomic_bool m_snapWndUpdated{false};
    SnapWindow m_bldtskSnapWnd;
    list<SnapshotBuildTask> m_bldtskTimeOrder;
    list<SnapshotBuildTask> m_bldtskByPriOrder;
    list<int64_t> m_vidKeyPtsList;
    mutex m_ssLock;
    double m_fixedSsInterval;
    uint32_t m_fixedSnapshotCount{100};

    AVFrameToImMatConverter m_frmCvt;
};

const AVRational MediaSnapshot_Impl::MILLISEC_TIMEBASE = { 1, 1000 };
const AVRational MediaSnapshot_Impl::FFAV_TIMEBASE = { 1, AV_TIME_BASE };

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{
    MediaSnapshot_Impl* ms = reinterpret_cast<MediaSnapshot_Impl*>(ctx->opaque);
    const AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (ms->CheckHwPixFmt(*p))
            return *p;
    }
    return AV_PIX_FMT_NONE;
}

MediaSnapshot* CreateMediaSnapshot()
{
    return new MediaSnapshot_Impl();
}

void ReleaseMediaSnapshot(MediaSnapshot** msrc)
{
    if (msrc == nullptr || *msrc == nullptr)
        return;
    MediaSnapshot_Impl* ms = dynamic_cast<MediaSnapshot_Impl*>(*msrc);
    ms->Close();
    delete ms;
    *msrc = nullptr;
}
