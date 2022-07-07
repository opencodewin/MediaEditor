#include <thread>
#include <mutex>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <list>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <cmath>
#include <algorithm>
#include "imgui_helper.h"
#include "MediaSnapshot.h"
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
using namespace Logger;

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts);

class SnapshotGenerator_Impl : public SnapshotGenerator
{
public:
    static ALogger* s_logger;

    SnapshotGenerator_Impl()
    {
        m_logger = GetSnapshotGeneratorLogger();
    }

    bool Open(const string& url) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (IsOpened())
            Close();

        MediaParserHolder hParser = CreateMediaParser();
        if (!hParser->Open(url))
        {
            m_errMsg = hParser->GetError();
            return false;
        }
        hParser->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);

        if (!OpenMedia(hParser))
        {
            Close();
            return false;
        }
        m_hParser = hParser;

        m_opened = true;
        return true;
    }

    bool Open(MediaParserHolder hParser) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!hParser || !hParser->IsOpened())
        {
            m_errMsg = "Argument 'hParser' is nullptr or not opened yet!";
            return false;
        }
        hParser->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);

        if (IsOpened())
            Close();

        if (!OpenMedia(hParser))
        {
            Close();
            return false;
        }
        m_hParser = hParser;

        m_opened = true;
        return true;
    }

    void Close() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        WaitAllThreadsQuit();
        FlushAllQueues();

        m_deprecatedTextures.clear();

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
        m_hParser = nullptr;
        m_hMediaInfo = nullptr;

        m_vidStartMts = 0;
        m_vidDurMts = 0;
        m_vidFrmCnt = 0;
        m_vidMaxIndex = 0;
        m_maxCacheSize = 0;

        m_hSeekPoints = nullptr;
        m_prepared = false;
        m_opened = false;

        m_errMsg = "";
    }

    bool GetSnapshots(double startPos, std::vector<ImageHolder>& images)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!IsOpened())
            return false;

        images.clear();

        int32_t idx0 = CalcSsIndexFromTs(startPos);
        if (idx0 < 0) idx0 = 0; if (idx0 > m_vidMaxIndex) idx0 = m_vidMaxIndex;
        int32_t idx1 = CalcSsIndexFromTs(startPos+m_snapWindowSize);
        if (idx1 < 0) idx1 = 0; if (idx1 > m_vidMaxIndex) idx1 = m_vidMaxIndex;
        if (idx0 > idx1)
            return true;

        // init 'images' with blank image
        images.reserve(idx1-idx0+1);
        for (int32_t i = idx0; i <= idx1; i++)
        {
            ImageHolder hIamge(new Image());
            hIamge->mTimestampMs = CalcSnapshotMts(i);
            images.push_back(hIamge);
        }

        lock_guard<mutex> readLock(m_goptskListReadLocks[0]);
        for (auto& goptsk : m_goptskList)
        {
            if (idx0 >= goptsk->TaskRange().SsIdx().second || idx1 < goptsk->TaskRange().SsIdx().first)
                continue;
            auto ssIter = goptsk->ssAry.begin();
            while (ssIter != goptsk->ssAry.end())
            {
                auto& ss = *ssIter++;
                if (ss.index < idx0 || ss.index > idx1)
                    continue;
                images[ss.index-idx0] = ss.img;
            }
        }
        return true;
    }

    MediaParserHolder GetMediaParser() const override
    {
        return m_hParser;
    }

    ViewerHolder CreateViewer(double pos) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        ViewerHolder hViewer(static_cast<Viewer*>(new Viewer_Impl(this, pos)));
        {
            lock_guard<mutex> lk(m_viewerListLock);
            m_viewers.push_back(hViewer);
        }
        return hViewer;
    }

    void ReleaseViewer(ViewerHolder& viewer) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        {
            lock_guard<mutex> lk(m_viewerListLock);
            auto iter = find(m_viewers.begin(), m_viewers.end(), viewer);
            if (iter != m_viewers.end())
                m_viewers.erase(iter);
        }
        viewer = nullptr;
    }

    void ReleaseViewer(Viewer* viewer)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        {
            lock_guard<mutex> lk(m_viewerListLock);
            auto iter = find_if(m_viewers.begin(), m_viewers.end(), [viewer] (const ViewerHolder& hViewer) {
                return hViewer.get() == viewer;
            });
            if (iter != m_viewers.end())
                m_viewers.erase(iter);
        }
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

    bool ConfigSnapWindow(double& windowSize, double frameCount) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (frameCount < 1)
        {
            m_errMsg = "Argument 'frameCount' must be greater than 1!";
            return false;
        }
        m_logger->Log(VERBOSE) << "---------------------------- Config snap window -----------------------------" << endl;
        double minWndSize = CalcMinWindowSize(frameCount);
        if (windowSize < minWndSize)
            windowSize = minWndSize;
        double maxWndSize = GetMaxWindowSize();
        if (windowSize > maxWndSize)
            windowSize = maxWndSize;
        if (m_snapWindowSize == windowSize && m_wndFrmCnt == frameCount)
            return true;

        WaitAllThreadsQuit();
        FlushAllQueues();
        if (m_viddecCtx)
            avcodec_flush_buffers(m_viddecCtx);

        m_snapWindowSize = windowSize;
        m_wndFrmCnt = frameCount;

        if (m_prepared)
        {
            CalcWindowVariables();
            ResetGopDecodeTaskList();
            for (auto& hViewer : m_viewers)
            {
                Viewer_Impl* viewer = dynamic_cast<Viewer_Impl*>(hViewer.get());
                viewer->UpdateSnapwnd(viewer->GetCurrWindowPos(), true);
            }
        }

        StartAllThreads();

        m_logger->Log(VERBOSE) << ">>>> Config window: m_snapWindowSize=" << m_snapWindowSize << ", m_wndFrmCnt=" << m_wndFrmCnt
            << ", m_vidMaxIndex=" << m_vidMaxIndex << ", m_maxCacheSize=" << m_maxCacheSize << ", m_prevWndCacheSize=" << m_prevWndCacheSize << endl;
        return true;
    }

    bool SetCacheFactor(double cacheFactor) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (cacheFactor < 1.)
        {
            m_errMsg = "Argument 'cacheFactor' must be greater or equal than 1.0!";
            return false;
        }
        m_cacheFactor = cacheFactor;
        m_maxCacheSize = (uint32_t)ceil(m_wndFrmCnt*m_cacheFactor);
        if (m_prepared)
            ResetGopDecodeTaskList();
        return true;
    }

    double GetMinWindowSize() const override
    {
        return CalcMinWindowSize(m_wndFrmCnt);
    }

    double GetMaxWindowSize() const override
    {
        return (double)m_vidDurMts/1000.;
    }

    bool SetSnapshotSize(uint32_t width, uint32_t height) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        m_useRszFactor = false;
        if (m_frmCvt.GetOutWidth() == width && m_frmCvt.GetOutHeight() == height)
            return true;
        if (!m_frmCvt.SetOutSize(width, height))
        {
            m_errMsg = m_frmCvt.GetError();
            return false;
        }
        if (m_prepared)
            ResetGopDecodeTaskList();
        return true;
    }

    bool SetSnapshotResizeFactor(float widthFactor, float heightFactor) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (widthFactor <= 0.f || heightFactor <= 0.f)
        {
            m_errMsg = "Resize factor must be a positive number!";
            return false;
        }
        if (!m_ssSizeChanged && m_useRszFactor && m_ssWFacotr == widthFactor && m_ssHFacotr == heightFactor)
            return true;

        m_ssWFacotr = widthFactor;
        m_ssHFacotr = heightFactor;
        m_useRszFactor = true;
        if (HasVideo())
        {
            if (widthFactor == 1.f && heightFactor == 1.f)
                return SetSnapshotSize(0, 0);

            auto vidStream = GetVideoStream();
            uint32_t outWidth = (uint32_t)ceil(vidStream->width*widthFactor);
            if ((outWidth&0x1) == 1)
                outWidth++;
            uint32_t outHeight = (uint32_t)ceil(vidStream->height*heightFactor);
            if ((outHeight&0x1) == 1)
                outHeight++;
            if (!SetSnapshotSize(outWidth, outHeight))
                return false;
            m_useRszFactor = true;
        }
        m_ssSizeChanged = false;
        return true;
    }

    bool SetOutColorFormat(ImColorFormat clrfmt) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_frmCvt.GetOutColorFormat() == clrfmt)
            return true;
        if (!m_frmCvt.SetOutColorFormat(clrfmt))
        {
            m_errMsg = m_frmCvt.GetError();
            return false;
        }
        if (m_prepared)
            ResetGopDecodeTaskList();
        return true;
    }

    bool SetResizeInterpolateMode(ImInterpolateMode interp) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_frmCvt.GetResizeInterpolateMode() == interp)
            return true;
        if (!m_frmCvt.SetResizeInterpolateMode(interp))
        {
            m_errMsg = m_frmCvt.GetError();
            return false;
        }
        if (m_prepared)
            ResetGopDecodeTaskList();
        return true;
    }

    MediaInfo::InfoHolder GetMediaInfo() const override
    {
        return m_hMediaInfo;
    }

    const MediaInfo::VideoStream* GetVideoStream() const override
    {
        MediaInfo::InfoHolder hInfo = m_hMediaInfo;
        if (!hInfo || !HasVideo())
            return nullptr;
        return dynamic_cast<MediaInfo::VideoStream*>(hInfo->streams[m_vidStmIdx].get());
    }

    const MediaInfo::AudioStream* GetAudioStream() const override
    {
        MediaInfo::InfoHolder hInfo = m_hMediaInfo;
        if (!hInfo || !HasAudio())
            return nullptr;
        return dynamic_cast<MediaInfo::AudioStream*>(hInfo->streams[m_audStmIdx].get());
    }

    uint32_t GetVideoWidth() const override
    {
        const MediaInfo::VideoStream* vidStream = GetVideoStream();
        if (vidStream)
            return vidStream->width;
        return 0;
    }

    uint32_t GetVideoHeight() const override
    {
        const MediaInfo::VideoStream* vidStream = GetVideoStream();
        if (vidStream)
            return vidStream->height;
        return 0;
    }

    int64_t GetVideoMinPos() const override
    {
        return 0;
    }

    int64_t GetVideoDuration() const override
    {
        return m_vidDurMts;
    }

    int64_t GetVideoFrameCount() const override
    {
        return m_vidFrmCnt;
    }

    string GetError() const override
    {
        return m_errMsg;
    }

    bool CheckHwPixFmt(AVPixelFormat pixfmt)
    {
        return pixfmt == m_vidHwPixFmt;
    }

private:
    string FFapiFailureMessage(const string& apiName, int fferr)
    {
        ostringstream oss;
        oss << "FF api '" << apiName << "' returns error! fferr=" << fferr << ".";
        return oss.str();
    }

    double CalcMinWindowSize(double windowFrameCount) const
    {
        return m_vidfrmIntvMts*windowFrameCount/1000.;
    }

    int64_t CvtVidPtsToMts(int64_t pts)
    {
        return av_rescale_q(pts-m_vidStream->start_time, m_vidStream->time_base, MILLISEC_TIMEBASE);
    }

    int64_t CvtVidMtsToPts(int64_t mts)
    {
        return av_rescale_q(mts, MILLISEC_TIMEBASE, m_vidStream->time_base)+m_vidStream->start_time;
    }

    void CalcWindowVariables()
    {
        m_ssIntvMts = m_snapWindowSize*1000./m_wndFrmCnt;
        if (m_ssIntvMts < m_vidfrmIntvMts)
            m_ssIntvMts = m_vidfrmIntvMts;
        else if (m_ssIntvMts-m_vidfrmIntvMts <= 0.5)
            m_ssIntvMts = m_vidfrmIntvMts;
        m_ssIntvPts = av_rescale_q(m_ssIntvMts*1000, MICROSEC_TIMEBASE, m_vidStream->time_base);
        m_vidMaxIndex = (uint32_t)floor(((double)m_vidDurMts-m_vidfrmIntvMts)/m_ssIntvMts);
        m_maxCacheSize = (uint32_t)ceil(m_wndFrmCnt*m_cacheFactor);
        uint32_t intWndFrmCnt = (uint32_t)ceil(m_wndFrmCnt);
        if (m_maxCacheSize < intWndFrmCnt)
            m_maxCacheSize = intWndFrmCnt;
        // if (m_maxCacheSize > m_vidMaxIndex+1)
        //     m_maxCacheSize = m_vidMaxIndex+1;
        m_prevWndCacheSize = (m_maxCacheSize-intWndFrmCnt)/2;
    }

    bool IsSsIdxValid(int32_t idx) const
    {
        return idx >= 0 && idx <= (int32_t)m_vidMaxIndex;
    }

    bool OpenMedia(MediaParserHolder hParser)
    {
        int fferr = avformat_open_input(&m_avfmtCtx, hParser->GetUrl().c_str(), nullptr, nullptr);
        if (fferr < 0)
        {
            m_avfmtCtx = nullptr;
            m_errMsg = FFapiFailureMessage("avformat_open_input", fferr);
            return false;
        }

        m_hMediaInfo = hParser->GetMediaInfo();
        m_vidStmIdx = hParser->GetBestVideoStreamIndex();
        m_audStmIdx = hParser->GetBestAudioStreamIndex();
        if (m_vidStmIdx < 0 && m_audStmIdx < 0)
        {
            ostringstream oss;
            oss << "Neither video nor audio stream can be found in '" << m_avfmtCtx->url << "'.";
            m_errMsg = oss.str();
            return false;
        }

        if (HasVideo())
        {
            MediaInfo::VideoStream* vidStream = dynamic_cast<MediaInfo::VideoStream*>(m_hMediaInfo->streams[m_vidStmIdx].get());
            m_vidStartMts = (int64_t)(vidStream->startTime*1000);
            m_vidDurMts = (int64_t)(vidStream->duration*1000);
            m_vidFrmCnt = vidStream->frameNum;
            AVRational avgFrmRate = { vidStream->avgFrameRate.num, vidStream->avgFrameRate.den };
            AVRational timebase = { vidStream->timebase.num, vidStream->timebase.den };
            m_vidfrmIntvMts = av_q2d(av_inv_q(avgFrmRate))*1000.;
            m_vidfrmIntvMtsHalf = ceil(m_vidfrmIntvMts)/2;
            m_vidfrmIntvPts = av_rescale_q(1, av_inv_q(avgFrmRate), timebase);
            m_vidfrmIntvPtsHalf = m_vidfrmIntvPts/2;

            if (m_useRszFactor)
            {
                uint32_t outWidth = (uint32_t)ceil(vidStream->width*m_ssWFacotr);
                if ((outWidth&0x1) == 1)
                    outWidth++;
                uint32_t outHeight = (uint32_t)ceil(vidStream->height*m_ssHFacotr);
                if ((outHeight&0x1) == 1)
                    outHeight++;
                if (!m_frmCvt.SetOutSize(outWidth, outHeight))
                {
                    m_errMsg = m_frmCvt.GetError();
                    return false;
                }
            }
        }

        return true;
    }

    bool Prepare()
    {
        bool lockAquired;
        while (!(lockAquired = m_apiLock.try_lock()) && !m_quit)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_quit)
        {
            if (lockAquired) m_apiLock.unlock();
            return false;
        }

        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        m_hParser->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);
        m_hSeekPoints = m_hParser->GetVideoSeekPoints();
        if (!m_hSeekPoints)
        {
            m_errMsg = "FAILED to retrieve video seek points!";
            m_logger->Log(Error) << m_errMsg << endl;
            return false;
        }

        int fferr;
        fferr = avformat_find_stream_info(m_avfmtCtx, nullptr);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avformat_open_input", fferr);
            return false;
        }

        if (HasVideo())
        {
            m_vidStream = m_avfmtCtx->streams[m_vidStmIdx];

            m_viddec = avcodec_find_decoder(m_vidStream->codecpar->codec_id);
            if (m_viddec == nullptr)
            {
                ostringstream oss;
                oss << "Can not find video decoder by codec_id " << m_vidStream->codecpar->codec_id << "!";
                m_errMsg = oss.str();
                return false;
            }

            if (m_vidPreferUseHw)
            {
                if (!OpenHwVideoDecoder())
                    if (!OpenVideoDecoder())
                        return false;
            }
            else if (!OpenVideoDecoder())
                return false;

            CalcWindowVariables();
            ResetGopDecodeTaskList();
        }

        if (HasAudio())
        {
            m_audStream = m_avfmtCtx->streams[m_audStmIdx];

            // wyvern: disable opening audio decoder because we don't use it now
            // if (!OpenAudioDecoder())
            //     return false;
        }

        {
            lock_guard<mutex> lk(m_viewerListLock);
            for (auto& hViewer : m_viewers)
            {
                Viewer_Impl* viewer = dynamic_cast<Viewer_Impl*>(hViewer.get());
                viewer->UpdateSnapwnd(viewer->GetCurrWindowPos(), true);
            }
        }

        m_logger->Log(DEBUG) << ">>>> Prepared: m_snapWindowSize=" << m_snapWindowSize << ", m_wndFrmCnt=" << m_wndFrmCnt
            << ", m_vidMaxIndex=" << m_vidMaxIndex << ", m_maxCacheSize=" << m_maxCacheSize << ", m_prevWndCacheSize=" << m_prevWndCacheSize << endl;
        m_prepared = true;
        return true;
    }

    bool OpenVideoDecoder()
    {
        m_viddecCtx = avcodec_alloc_context3(m_viddec);
        if (!m_viddecCtx)
        {
            m_errMsg = "FAILED to allocate new AVCodecContext!";
            return false;
        }
        m_viddecCtx->opaque = this;

        int fferr;
        fferr = avcodec_parameters_to_context(m_viddecCtx, m_vidStream->codecpar);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avcodec_parameters_to_context", fferr);
            return false;
        }

        m_viddecCtx->thread_count = 8;
        // m_viddecCtx->thread_type = FF_THREAD_FRAME;
        fferr = avcodec_open2(m_viddecCtx, m_viddec, nullptr);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avcodec_open2", fferr);
            return false;
        }
        m_logger->Log(DEBUG) << "Video decoder '" << m_viddec->name << "' opened." << " thread_count=" << m_viddecCtx->thread_count
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
                m_errMsg = oss.str();
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
        m_logger->Log(DEBUG) << "Use hardware device type '" << av_hwdevice_get_type_name(m_viddecDevType) << "'." << endl;

        m_viddecCtx = avcodec_alloc_context3(m_viddec);
        if (!m_viddecCtx)
        {
            m_errMsg = "FAILED to allocate new AVCodecContext!";
            return false;
        }
        m_viddecCtx->opaque = this;

        int fferr;
        fferr = avcodec_parameters_to_context(m_viddecCtx, m_vidStream->codecpar);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avcodec_parameters_to_context", fferr);
            return false;
        }
        m_viddecCtx->get_format = get_hw_format;

        fferr = av_hwdevice_ctx_create(&m_viddecHwDevCtx, m_viddecDevType, nullptr, nullptr, 0);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("av_hwdevice_ctx_create", fferr);
            return false;
        }
        m_viddecCtx->hw_device_ctx = av_buffer_ref(m_viddecHwDevCtx);

        fferr = avcodec_open2(m_viddecCtx, m_viddec, nullptr);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avcodec_open2", fferr);
            return false;
        }
        m_logger->Log(DEBUG) << "Video decoder(HW) '" << m_viddecCtx->codec->name << "' opened." << endl;
        return true;
    }

    void DemuxThreadProc()
    {
        m_logger->Log(VERBOSE) << "Enter DemuxThreadProc()..." << endl;

        if (!m_prepared && !Prepare())
        {
            if (!m_quit)
                m_logger->Log(Error) << "Prepare() FAILED! Error is '" << m_errMsg << "'." << endl;
            return;
        }

        AVPacket avpkt = {0};
        bool avpktLoaded = false;
        GopDecodeTaskHolder currTask = nullptr;
        int64_t lastGopSsPts;
        bool demuxEof = false;
        while (!m_quit)
        {
            bool idleLoop = true;

            UpdateGopDecodeTaskList();

            if (HasVideo())
            {
                bool taskChanged = false;
                if (!currTask || currTask->cancel || currTask->demuxerEof)
                {
                    if (currTask && currTask->cancel)
                        m_logger->Log(VERBOSE) << "~~~~ Current demux task canceled" << endl;
                    currTask = FindNextDemuxTask();
                    if (currTask)
                    {
                        currTask->demuxing = true;
                        taskChanged = true;
                        lastGopSsPts = INT64_MAX;
                        m_logger->Log(VERBOSE) << "--> Change demux task, ssIdxPair=[" << currTask->TaskRange().SsIdx().first << ", " << currTask->TaskRange().SsIdx().second
                            << "), seekPtsPair=[" << currTask->TaskRange().SeekPts().first << "{" << MillisecToString(CvtVidPtsToMts(currTask->TaskRange().SeekPts().first)) << "}"
                            << ", " << currTask->TaskRange().SeekPts().second << "{" << MillisecToString(CvtVidPtsToMts(currTask->TaskRange().SeekPts().second)) << "}" << endl;
                    }
                }

                if (currTask)
                {
                    if (taskChanged)
                    {
                        if (!avpktLoaded || avpkt.pts != currTask->TaskRange().SeekPts().first)
                        {
                            if (avpktLoaded)
                            {
                                av_packet_unref(&avpkt);
                                avpktLoaded = false;
                            }
                            const int64_t seekPts0 = currTask->TaskRange().SeekPts().first;
                            int fferr = avformat_seek_file(m_avfmtCtx, m_vidStmIdx, INT64_MIN, seekPts0, seekPts0, 0);
                            if (fferr < 0)
                            {
                                m_logger->Log(Error) << "avformat_seek_file() FAILED for seeking to 'currTask->startPts'(" << seekPts0 << ")! fferr = " << fferr << "!" << endl;
                                break;
                            }
                            demuxEof = false;
                            int64_t ptsAfterSeek = INT64_MIN;
                            if (!ReadNextStreamPacket(m_vidStmIdx, &avpkt, &avpktLoaded, &ptsAfterSeek))
                                break;
                            if (ptsAfterSeek == INT64_MAX)
                                demuxEof = true;
                            else if (ptsAfterSeek != seekPts0)
                            {
                                m_logger->Log(WARN) << "'ptsAfterSeek'(" << ptsAfterSeek << ") != 'ssTask->startPts'(" << seekPts0 << ")!" << endl;
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
                                m_logger->Log(Error) << "Demuxer ERROR! av_read_frame() returns " << fferr << "." << endl;
                        }
                    }

                    if (avpktLoaded)
                    {
                        if (avpkt.stream_index == m_vidStmIdx)
                        {
                            if (avpkt.pts >= currTask->TaskRange().SeekPts().second || avpkt.pts > lastGopSsPts)
                            {
                                for (auto& elem : currTask->ssCandidatePts)
                                {
                                    if (elem.second.first == INT64_MIN)
                                        m_logger->Log(WARN) << "!! ABNORMAL SS CANDIDATE !! Current demux task eof, but no candidate frame is found for SS #" << elem.first << "." << endl;
                                    else
                                        m_logger->Log(DEBUG) << "SS candidate #" << elem.first << ": pts=" << elem.second.first << ", bias=" << elem.second.second << endl;
                                }
                                currTask->demuxerEof = true;
                            }

                            if (!currTask->demuxerEof)
                            {
                                uint32_t bias{0};
                                int32_t ssIdx = checkFrameSsBias(avpkt.pts, bias);
                                // update SS candidates frame
                                auto candIter = currTask->ssCandidatePts.find(ssIdx);
                                if (candIter != currTask->ssCandidatePts.end())
                                {
                                    if (candIter->second.first == INT64_MIN || candIter->second.second > bias)
                                        candIter->second = { avpkt.pts, bias };
                                }
                                if (ssIdx == currTask->m_range.SsIdx().second-1 && bias <= m_vidfrmIntvPtsHalf)
                                    lastGopSsPts = avpkt.pts;

                                AVPacket* enqpkt = av_packet_clone(&avpkt);
                                if (!enqpkt)
                                {
                                    m_logger->Log(Error) << "FAILED to invoke [DEMUX]av_packet_clone()!" << endl;
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
            }
            else
            {
                m_logger->Log(Error) << "Demux procedure to non-video media is NOT IMPLEMENTED yet!" << endl;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (currTask && !currTask->demuxerEof)
            currTask->demuxerEof = true;
        if (avpktLoaded)
            av_packet_unref(&avpkt);
        m_logger->Log(VERBOSE) << "Leave DemuxThreadProc()." << endl;
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
                    m_logger->Log(Error) << "av_read_frame() FAILED! fferr = " << fferr << "." << endl;
                    return false;
                }
            }
        } while (fferr >= 0 && !m_quit);
        if (m_quit)
            return false;
        return true;
    }

    void VideoDecodeThreadProc()
    {
        m_logger->Log(VERBOSE) << "Enter VideoDecodeThreadProc()..." << endl;

        while (!m_prepared && !m_quit)
            this_thread::sleep_for(chrono::milliseconds(5));

        GopDecodeTaskHolder currTask;
        AVFrame avfrm = {0};
        bool avfrmLoaded = false;
        bool inputEof = false;
        bool needResetDecoder = false;
        bool sentNullPacket = false;
        while (!m_quit)
        {
            bool idleLoop = true;
            bool quitLoop = false;

            if (currTask && currTask->cancel)
            {
                m_logger->Log(VERBOSE) << "~~~~ Current video task canceled" << endl;
                if (avfrmLoaded)
                {
                    av_frame_unref(&avfrm);
                    avfrmLoaded = false;
                }
                currTask = nullptr;
            }

            if (!currTask || currTask->decoderEof)
            {
                GopDecodeTaskHolder oldTask = currTask;
                currTask = FindNextDecoderTask();
                if (currTask)
                {
                    currTask->decoding = true;
                    inputEof = false;
                    m_logger->Log(VERBOSE) << "==> Change [VIDEO]decoding task to build SS ["
                        << currTask->m_range.SsIdx().first << ", " << currTask->m_range.SsIdx().second << ")." << endl;
                }
                else if (oldTask)
                {
                    avcodec_send_packet(m_viddecCtx, nullptr);
                    sentNullPacket = true;
                }
            }

            if (needResetDecoder)
            {
                avcodec_flush_buffers(m_viddecCtx);
                needResetDecoder = false;
                sentNullPacket = false;
            }

            if (currTask || sentNullPacket)
            {
                // retrieve output frame
                bool hasOutput;
                do{
                    if (!avfrmLoaded)
                    {
                        int fferr = avcodec_receive_frame(m_viddecCtx, &avfrm);
                        if (fferr == 0)
                        {
                            m_logger->Log(VERBOSE) << "<<< [VIDEO]avcodec_receive_frame() pts=" << avfrm.pts << "(" << MillisecToString(CvtVidPtsToMts(avfrm.pts)) << ")." << endl;
                            avfrmLoaded = true;
                            idleLoop = false;
                        }
                        else if (fferr != AVERROR(EAGAIN))
                        {
                            if (fferr != AVERROR_EOF)
                            {
                                m_logger->Log(Error) << "FAILED to invoke [VIDEO]avcodec_receive_frame()! return code is " << fferr << "." << endl;
                                quitLoop = true;
                                break;
                            }
                            else
                            {
                                idleLoop = false;
                                needResetDecoder = true;
                                m_logger->Log(VERBOSE) << "Video decoder current task reaches EOF!" << endl;
                            }
                        }
                    }

                    hasOutput = avfrmLoaded;
                    if (avfrmLoaded)
                    {
                        int32_t ssIdx{-1};
                        GopDecodeTaskHolder ssGopTask = FindFrameSsPosition(avfrm.pts, ssIdx);
                        if (!ssGopTask)
                        {
                            m_logger->Log(VERBOSE) << "Drop video frame pts=" << avfrm.pts << ", ssIdx=" << ssIdx << ". No corresponding GopDecoderTask can be found." << endl;
                            av_frame_unref(&avfrm);
                            avfrmLoaded = false;
                            idleLoop = false;
                        }
                        else if (m_pendingVidfrmCnt < m_maxPendingVidfrmCnt)
                        {
                            m_logger->Log(DEBUG) << "Enqueue SS#" << ssIdx << ", pts=" << avfrm.pts << " to GopDecodeTask, which its range is ssIdxPair=["
                                << ssGopTask->m_range.SsIdx().first << ", " << ssGopTask->m_range.SsIdx().second << "), ptsPair=["
                                << ssGopTask->m_range.SeekPts().first << ", " << ssGopTask->m_range.SeekPts().second << ")." << endl;
                            if (!EnqueueSnapshotAVFrame(ssGopTask, &avfrm, ssIdx))
                                m_logger->Log(WARN) << "FAILED to enqueue SS#" << ssIdx << ", pts=" << avfrm.pts << "." << endl;
                            av_frame_unref(&avfrm);
                            avfrmLoaded = false;
                            idleLoop = false;
                        }
                    }
                } while (hasOutput && !m_quit);
                if (quitLoop)
                    break;

                // input packet to decoder
                if (!inputEof && !sentNullPacket)
                {
                    if (!currTask->avpktQ.empty())
                    {
                        bool popAvpkt = false;
                        AVPacket* avpkt = currTask->avpktQ.front();
                        int fferr = avcodec_send_packet(m_viddecCtx, avpkt);
                        if (fferr == 0)
                        {
                            m_logger->Log(VERBOSE) << ">>> [VIDEO]avcodec_send_packet() pts=" << avpkt->pts << "(" << MillisecToString(CvtVidPtsToMts(avpkt->pts)) << ")." << endl;
                            popAvpkt = true;
                        }
                        else if (fferr != AVERROR(EAGAIN) && fferr != AVERROR_INVALIDDATA)
                        {
                            m_logger->Log(Error) << "FAILED to invoke [VIDEO]avcodec_send_packet()! return code is " << fferr << "." << endl;
                            quitLoop = true;
                        }
                        else if (fferr == AVERROR_INVALIDDATA)
                        {
                            popAvpkt = true;
                        }
                        if (popAvpkt)
                        {
                            {
                                lock_guard<mutex> lk(currTask->avpktQLock);
                                currTask->avpktQ.pop_front();
                            }
                            av_packet_free(&avpkt);
                            idleLoop = false;
                        }
                    }
                    else if (currTask->demuxerEof)
                    {
                        currTask->decoderEof = true;
                        idleLoop = false;
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
        m_logger->Log(VERBOSE) << "Leave VideoDecodeThreadProc()." << endl;
    }

    void UpdateSnapshotThreadProc()
    {
        m_logger->Log(VERBOSE) << "Enter UpdateSnapshotThreadProc()." << endl;
        GopDecodeTaskHolder currTask;
        while (!m_quit)
        {
            bool idleLoop = true;

            if (!currTask || currTask->cancel || currTask->ssfrmCnt <= 0)
            {
                currTask = FindNextSsUpdateTask();
            }

            if (currTask)
            {
                for (Snapshot& ss : currTask->ssAry)
                {
                    if (ss.avfrm)
                    {
                        double ts = (double)CvtVidPtsToMts(ss.avfrm->pts)/1000.;
                        if (!m_frmCvt.ConvertImage(ss.avfrm, ss.img->mImgMat, ts))
                            m_logger->Log(Error) << "FAILED to convert AVFrame to ImGui::ImMat! Message is '" << m_frmCvt.GetError() << "'." << endl;
                        av_frame_free(&ss.avfrm);
                        ss.avfrm = nullptr;
                        currTask->ssfrmCnt--;
                        if (currTask->ssfrmCnt < 0)
                            m_logger->Log(Error) << "!! ABNORMAL !! Task [" << currTask->TaskRange().SsIdx().first << ", " << currTask->TaskRange().SsIdx().second
                                << ") has negative 'ssfrmCnt'(" << currTask->ssfrmCnt << ")!" << endl;
                        m_pendingVidfrmCnt--;
                        if (m_pendingVidfrmCnt < 0)
                            m_logger->Log(Error) << "Pending video AVFrame ptr count is NEGATIVE! " << m_pendingVidfrmCnt << endl;
                        ss.img->mTimestampMs = CalcSnapshotMts(ss.index);
                        idleLoop = false;
                    }
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_logger->Log(VERBOSE) << "Leave UpdateSnapshotThreadProc()." << endl;
    }

    void StartAllThreads()
    {
        m_quit = false;
        m_demuxThread = thread(&SnapshotGenerator_Impl::DemuxThreadProc, this);
        if (HasVideo())
            m_viddecThread = thread(&SnapshotGenerator_Impl::VideoDecodeThreadProc, this);
        m_updateSsThread = thread(&SnapshotGenerator_Impl::UpdateSnapshotThreadProc, this);
    }

    void WaitAllThreadsQuit()
    {
        m_quit = true;
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
        if (m_updateSsThread.joinable())
        {
            m_updateSsThread.join();
            m_updateSsThread = thread();
        }
    }

    void FlushAllQueues()
    {
        m_goptskPrepareList.clear();
        m_goptskList.clear();
    }

    struct Snapshot
    {
        Snapshot() : img(new Image()) {}
        AVFrame* avfrm{nullptr};
        ImageHolder img;
        uint32_t index;
        bool fixed{false};
    };

    struct SnapWindow
    {
        double wndpos;
        int32_t viewIdx0;
        int32_t viewIdx1;
        int32_t cacheIdx0;
        int32_t cacheIdx1;
        int64_t seekPos00;
        int64_t seekPos10;

        bool IsInView(int32_t idx) const
        { return idx >= viewIdx0 && idx <= viewIdx1; }
        bool IsInCache(int32_t idx) const
        { return idx >= cacheIdx0 && idx <= cacheIdx1; }
        bool IsInCache(int64_t pts) const
        { return pts >= seekPos00 && pts <= seekPos10; }
    };

    struct GopDecodeTask
    {
        class Range
        {
        public:
            Range(const pair<int64_t, int64_t>& seekPts, const pair<int32_t, int32_t>& ssIdx, bool isInView, int32_t distanceToViewWnd)
                : m_seekPts(seekPts), m_ssIdx(ssIdx), m_isInView(isInView), m_distanceToViewWnd(distanceToViewWnd)
            {}
            Range(const Range&) = default;
            Range(Range&&) = default;
            Range& operator=(const Range&) = default;

            const pair<int64_t, int64_t>& SeekPts() const { return m_seekPts; }
            const pair<int32_t, int32_t>& SsIdx() const { return m_ssIdx; }
            bool IsInView() const { return m_isInView; }
            void SetInView(bool isInView) { m_isInView = isInView; }
            int32_t DistanceToViewWindow() const { return m_distanceToViewWnd; }

            friend bool operator==(const Range& oprnd1, const Range& oprnd2)
            {
                if (oprnd1.m_seekPts.first == INT64_MIN && oprnd2.m_seekPts.second == INT64_MIN)
                    return false;
                bool e1 = oprnd1.m_seekPts.first == oprnd2.m_seekPts.first;
                bool e2 = oprnd1.m_seekPts.second == oprnd2.m_seekPts.second;
                if (e1^e2)
                    GetSnapshotGeneratorLogger()->Log(Error) << "!!! GopDecodeTask::Range compare ABNORMAL! ("
                        << oprnd1.m_seekPts.first << ", " << oprnd1.m_seekPts.second << ") VS ("
                        << oprnd2.m_seekPts.first << ", " << oprnd2.m_seekPts.second << ")." << endl;
                return e1 && e2;
            }

        private:
            pair<int64_t, int64_t> m_seekPts{INT64_MIN, INT64_MIN};
            pair<int32_t, int32_t> m_ssIdx{-1, -1};
            int32_t m_distanceToViewWnd{0};
            bool m_isInView{false};
        };

        GopDecodeTask(SnapshotGenerator_Impl* owner, const Range& range)
            : m_owner(owner), m_range(range)
        {
            for (int32_t ssIdx = range.SsIdx().first; ssIdx < range.SsIdx().second; ssIdx++)
            {
                ssCandidatePts[ssIdx] = { INT64_MIN, 0 };
            }
        }

        ~GopDecodeTask()
        {
            for (AVPacket* avpkt : avpktQ)
                av_packet_free(&avpkt);
            for (Snapshot& ss : ssAry)
            {
                if (ss.avfrm)
                {
                    av_frame_free(&ss.avfrm);
                    m_owner->m_pendingVidfrmCnt--;
                }
                if (ss.img->mTextureHolder)
                {
                    lock_guard<mutex> lk(m_owner->m_deprecatedTextureLock);
                    m_owner->m_deprecatedTextures.push_back(ss.img->mTextureHolder);
                }
            }
        }

        const Range& TaskRange() const { return m_range; }
        bool IsInView() const { return m_range.IsInView(); }
        int32_t DistanceToViewWnd() const { return m_range.DistanceToViewWindow(); }

        SnapshotGenerator_Impl* m_owner;
        Range m_range;
        unordered_map<int32_t, pair<int64_t, uint32_t>> ssCandidatePts;
        bool isEndOfGop{true};
        list<Snapshot> ssAry;
        atomic_int32_t ssfrmCnt{0};
        list<AVPacket*> avpktQ;
        mutex avpktQLock;
        bool demuxing{false};
        bool demuxerEof{false};
        bool decoding{false};
        bool decoderEof{false};
        bool cancel{false};
    };
    using GopDecodeTaskHolder = shared_ptr<GopDecodeTask>;

    SnapWindow CreateSnapWindow(double wndpos)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_prepared)
            return { wndpos, -1, -1, -1, -1, INT64_MIN, INT64_MIN };
        int32_t index0 = CalcSsIndexFromTs(wndpos);
        int32_t index1 = CalcSsIndexFromTs(wndpos+m_snapWindowSize);
        int32_t cacheIdx0 = index0-(int32_t)m_prevWndCacheSize;
        int32_t cacheIdx1 = cacheIdx0+(int32_t)m_maxCacheSize-1;
        pair<int64_t, int64_t> seekPos0 = GetSeekPosBySsIndex(cacheIdx0);
        pair<int64_t, int64_t> seekPos1 = GetSeekPosBySsIndex(cacheIdx1);
        return { wndpos, index0, index1, cacheIdx0, cacheIdx1, seekPos0.first, seekPos1.first };
    }

    GopDecodeTaskHolder FindFrameSsPosition(int64_t pts, int32_t& ssIdx)
    {
        ssIdx = (int32_t)round((double)pts/m_ssIntvPts);
        uint32_t bias = (uint32_t)abs(m_ssIntvPts*ssIdx-pts);
        GopDecodeTaskHolder task;
        {
            lock_guard<mutex> lk(m_goptskListReadLocks[0]);
            auto iter = find_if(m_goptskList.begin(), m_goptskList.end(), [pts, ssIdx, bias] (const GopDecodeTaskHolder& t) {
                if (ssIdx < t->TaskRange().SsIdx().first || ssIdx >= t->TaskRange().SsIdx().second)
                    return false;
                auto candIter = t->ssCandidatePts.find(ssIdx);
                if (candIter != t->ssCandidatePts.end() && (candIter->second.first == pts || candIter->second.second > bias))
                    return true;
                return false;
            });
            if (iter != m_goptskList.end())
                task = *iter;
        }
        return task;
    }

    int32_t checkFrameSsBias(int64_t pts, uint32_t& bias)
    {
        int32_t index = (int32_t)round((double)pts/m_ssIntvPts);
        bias = (uint32_t)abs(m_ssIntvPts*index-pts);
        return index;
    }

    bool IsSpecificSnapshotFrame(uint32_t index, int64_t mts)
    {
        double diff = abs(index*m_ssIntvMts-mts);
        return diff <= m_vidfrmIntvMtsHalf;
    }

    int64_t CalcSnapshotMts(int32_t index)
    {
        if (m_ssIntvPts > 0)
            return CvtVidPtsToMts(index*m_ssIntvPts);
        return 0;
    }

    double CalcSnapshotTimestamp(uint32_t index)
    {
        return (double)CalcSnapshotMts(index)/1000.;
    }

    int32_t CalcSsIndexFromTs(double ts)
    {
        return (int32_t)floor(ts*1000/m_ssIntvMts);
    }

    pair<int64_t, int64_t> GetSeekPosByMts(int64_t mts)
    {
        if (mts < 0)
            return { INT64_MIN, INT64_MIN };
        if (mts > m_vidDurMts)
            return { INT64_MAX, INT64_MAX };
        int64_t targetPts = CvtVidMtsToPts(mts);
        int64_t offsetCompensation = m_vidfrmIntvPtsHalf;
        auto iter = find_if(m_hSeekPoints->begin(), m_hSeekPoints->end(),
            [targetPts, offsetCompensation](int64_t keyPts) { return keyPts-offsetCompensation > targetPts; });
        if (iter != m_hSeekPoints->begin())
            iter--;
        int64_t first = *iter++;
        int64_t second = iter == m_hSeekPoints->end() ? INT64_MAX : *iter;
        if (targetPts >= second || (second-targetPts) < offsetCompensation)
        {
            first = second;
            iter++;
            second = iter == m_hSeekPoints->end() ? INT64_MAX : *iter;
        }
        return { first, second };
    }

    pair<int64_t, int64_t> GetSeekPosBySsIndex(int32_t index)
    {
        return GetSeekPosByMts(CalcSnapshotMts(index));
    }

    pair<int32_t, int32_t> CalcSsIndexPairFromPtsPair(const pair<int64_t, int64_t>& ptsPair)
    {
        int64_t mts0 = CvtVidPtsToMts(ptsPair.first);
        int32_t idx0 = (int32_t)ceil((double)(ptsPair.first-m_vidfrmIntvPtsHalf)/m_ssIntvPts);
        int32_t idx1;
        if (ptsPair.second == INT64_MAX)
            idx1 = m_vidMaxIndex+1;
        else
            idx1 = (int32_t)ceil((double)(ptsPair.second-m_vidfrmIntvPtsHalf)/m_ssIntvPts);
        if (idx1 == idx0) idx1++;
        return { idx0, idx1 };
    }

    void ResetGopDecodeTaskList()
    {
        {
            lock(m_goptskListReadLocks[0], m_goptskListReadLocks[1], m_goptskListReadLocks[2]);
            lock_guard<mutex> lk0(m_goptskListReadLocks[0], adopt_lock);
            lock_guard<mutex> lk1(m_goptskListReadLocks[1], adopt_lock);
            lock_guard<mutex> lk2(m_goptskListReadLocks[2], adopt_lock);
            m_goptskList.clear();
            m_goptskPrepareList.clear();
        }

        UpdateGopDecodeTaskList();
    }

    void UpdateGopDecodeTaskList()
    {
        list<ViewerHolder> viewers;
        {
            lock_guard<mutex> lk(m_viewerListLock);
            viewers = m_viewers;
        }
        // Check if view window changed
        bool taskRangeChanged = false;
        for (auto& hViewer : viewers)
        {
            Viewer_Impl* viewer = dynamic_cast<Viewer_Impl*>(hViewer.get());
            if (viewer->IsTaskRangeChanged())
            {
                taskRangeChanged = true;
                break;
            }
        }
        if (!taskRangeChanged)
            return;

        // Aggregate all GopDecodeTask::Range(s) from all the Viewer(s)
        list<GopDecodeTask::Range> totalTaskRanges;
        for (auto& hViewer : viewers)
        {
            Viewer_Impl* viewer = dynamic_cast<Viewer_Impl*>(hViewer.get());
            list<GopDecodeTask::Range> taskRanges = viewer->CheckTaskRanges();
            for (auto& tskrng : taskRanges)
            {
                auto iter = find(totalTaskRanges.begin(), totalTaskRanges.end(), tskrng);
                if (iter == totalTaskRanges.end())
                    totalTaskRanges.push_back(tskrng);
                else if (tskrng.IsInView())
                    iter->SetInView(true);
            }
        }
        m_logger->Log(DEBUG) << ">>>>> Aggregated task ranges <<<<<<<" << endl << "\t";
        for (auto& range : totalTaskRanges)
            m_logger->Log(DEBUG) << "[" << range.SsIdx().first << ", " << range.SsIdx().second << "), ";
        m_logger->Log(DEBUG) << endl;

        // Update GopDecodeTask prepare list
        bool updated = false;
        // 1. remove the tasks that are no longer in the cache range
        auto taskIter = m_goptskPrepareList.begin();
        while (taskIter != m_goptskPrepareList.end())
        {
            auto& task = *taskIter;
            auto iter = find_if(totalTaskRanges.begin(), totalTaskRanges.end(), [task] (const GopDecodeTask::Range& range) {
                return task->m_range == range;
            });
            if (iter == totalTaskRanges.end())
            {
                task->cancel = true;
                m_logger->Log(DEBUG) << "~~~~> Erase task range [" << (*taskIter)->TaskRange().SsIdx().first << ", " << (*taskIter)->TaskRange().SsIdx().second << ")" << endl;
                taskIter = m_goptskPrepareList.erase(taskIter);
                updated = true;
            }
            else
            {
                task->m_range.SetInView(iter->IsInView());
                totalTaskRanges.erase(iter);
                taskIter++;
            }
        }
        // 2. add the tasks with newly created ranges
        for (auto& range : totalTaskRanges)
        {
            GopDecodeTaskHolder hTask(new GopDecodeTask(this, range));
            m_goptskPrepareList.push_back(hTask);
            updated = true;
        }

        // Update GopDecodeTask list
        if (updated)
        {
            lock(m_goptskListReadLocks[0], m_goptskListReadLocks[1], m_goptskListReadLocks[2]);
            lock_guard<mutex> lk0(m_goptskListReadLocks[0], adopt_lock);
            lock_guard<mutex> lk1(m_goptskListReadLocks[1], adopt_lock);
            lock_guard<mutex> lk2(m_goptskListReadLocks[2], adopt_lock);
            m_goptskList = m_goptskPrepareList;
        }
    }

    GopDecodeTaskHolder FindNextDemuxTask()
    {
        GopDecodeTaskHolder candidateTask = nullptr;
        uint32_t pendingDecodingTaskCnt = 0;
        int32_t shortestDistanceToViewWnd = INT32_MAX;
        for (auto& task : m_goptskList)
        {
            if (!task->cancel && !task->demuxing)
            {
                if (task->IsInView())
                {
                    candidateTask = task;
                    break;
                }
                else if (shortestDistanceToViewWnd > task->DistanceToViewWnd())
                {
                    candidateTask = task;
                    shortestDistanceToViewWnd = task->DistanceToViewWnd();
                }
            }
            else if (!task->decoding)
            {
                pendingDecodingTaskCnt++;
                if (pendingDecodingTaskCnt > m_maxPendingTaskCountForDecoding)
                {
                    candidateTask = nullptr;
                    break;
                }
            }
        }
        return candidateTask;
    }

    GopDecodeTaskHolder FindNextDecoderTask()
    {
        lock_guard<mutex> lk(m_goptskListReadLocks[1]);
        GopDecodeTaskHolder nxttsk = nullptr;
        for (auto& tsk : m_goptskList)
            if (!tsk->cancel && tsk->demuxing && !tsk->decoding)
            {
                nxttsk = tsk;
                break;
            }
        return nxttsk;
    }

    GopDecodeTaskHolder FindNextSsUpdateTask()
    {
        lock_guard<mutex> lk(m_goptskListReadLocks[2]);
        GopDecodeTaskHolder nxttsk = nullptr;
        for (auto& tsk : m_goptskList)
            if (!tsk->cancel && tsk->ssfrmCnt > 0)
            {
                nxttsk = tsk;
                break;
            }
        return nxttsk;
    }

    bool EnqueueSnapshotAVFrame(GopDecodeTaskHolder ssGopTask, AVFrame* frm, int32_t ssIdx)
    {
        {
            lock_guard<mutex> lk(m_goptskListReadLocks[0]);
            auto iter = find(m_goptskList.begin(), m_goptskList.end(), ssGopTask);
            if (iter == m_goptskList.end())
                return false;
        }

        Snapshot ss;
        ss.index = ssIdx;
        ss.avfrm = av_frame_clone(frm);
        if (!ss.avfrm)
        {
            m_logger->Log(Error) << "FAILED to invoke 'av_frame_clone()' to allocate new AVFrame for SS!" << endl;
            return false;
        }

        // m_logger->Log(DEBUG) << "Adding SS#" << ssIdx << "." << endl;
        if (ssGopTask->ssAry.empty())
        {
            ssGopTask->ssAry.push_back(ss);
            ssGopTask->ssfrmCnt++;
            m_pendingVidfrmCnt++;
        }
        else
        {
            auto ssRvsIter = find_if(ssGopTask->ssAry.rbegin(), ssGopTask->ssAry.rend(), [ssIdx] (const Snapshot& ss) {
                return ss.index <= ssIdx;
            });
            if (ssRvsIter != ssGopTask->ssAry.rend() && ssRvsIter->index == ssIdx)
            {
                m_logger->Log(DEBUG) << "Found duplicated SS#" << ssIdx << ", dropping this SS. pts=" << frm->pts
                    << ", t=" << MillisecToString(CvtVidPtsToMts(frm->pts)) << "." << endl;
                if (ss.avfrm)
                    av_frame_free(&ss.avfrm);
            }
            else
            {
                auto ssFwdIter = ssRvsIter.base();
                ssGopTask->ssAry.insert(ssFwdIter, ss);
                ssGopTask->ssfrmCnt++;
                m_pendingVidfrmCnt++;
            }
        }
        return true;
    }

    class Viewer_Impl : public Viewer
    {
    public:
        Viewer_Impl(SnapshotGenerator_Impl* owner, double wndpos)
            : m_owner(owner)
        {
            m_logger = owner->m_logger;
            UpdateSnapwnd(wndpos, true);
        }

        bool Seek(double pos) override
        {
            UpdateSnapwnd(pos);
            return true;
        }

        double GetCurrWindowPos() const override
        {
            return m_snapwnd.wndpos;
        }

        bool GetSnapshots(double startPos, vector<SnapshotGenerator::ImageHolder>& snapshots) override
        {
            UpdateSnapwnd(startPos);
            return m_owner->GetSnapshots(startPos, snapshots);
        }

        ViewerHolder CreateViewer(double pos) override
        {
            return m_owner->CreateViewer(pos);
        }

        void Release() override
        {
            return m_owner->ReleaseViewer(this);
        }

        MediaParserHolder GetMediaParser() const override
        {
            return m_owner->GetMediaParser();
        }

        string GetError() const override
        {
            return m_owner->GetError();
        }

        bool UpdateSnapshotTexture(vector<SnapshotGenerator::ImageHolder>& snapshots) override
        {
            // free deprecated textures
            {
                m_logger->Log(VERBOSE) << "[3]===== Begin release texture" << endl;
                lock_guard<mutex> lktid(m_owner->m_deprecatedTextureLock);
                m_owner->m_deprecatedTextures.clear();
                m_logger->Log(VERBOSE) << "[3]===== End release texture" << endl;
            }

            m_logger->Log(VERBOSE) << "[2]----- Begin generate texture" << endl;
            for (auto& img : snapshots)
            {
                if (img->mTextureReady)
                    continue;
                if (!img->mImgMat.empty())
                {
                    img->mTextureHolder = TextureHolder(new ImTextureID(0), [this] (ImTextureID* pTid) {
                        if (*pTid)
                        {
                            GetMediaSnapshotLogger()->Log(VERBOSE) << "[3]\t\t\treleasing tid=" << *pTid << endl;
                            ImGui::ImDestroyTexture(*pTid);
                        }
                        delete pTid;
                    });
                    m_logger->Log(VERBOSE) << "[2]\tbefore generate tid=" << *(img->mTextureHolder) << endl;
                    ImMatToTexture(img->mImgMat, *(img->mTextureHolder));
                    m_logger->Log(VERBOSE) << "[2]\tgenerated tid=" << *(img->mTextureHolder) << endl;
                    img->mTextureReady = true;
                }
            }
            m_logger->Log(VERBOSE) << "[2]----- End generate texture" << endl;
            return true;
        }

        bool IsTaskRangeChanged() const { return m_taskRangeChanged; }

        list<GopDecodeTask::Range> CheckTaskRanges()
        {
            lock_guard<mutex> lk(m_taskRangeLock);
            list<GopDecodeTask::Range> taskRanges(m_taskRanges);
            m_taskRangeChanged = false;
            return move(taskRanges);
        }

        void UpdateSnapwnd(double wndpos, bool force = false)
        {
            SnapWindow snapwnd = m_owner->CreateSnapWindow(wndpos);
            list<GopDecodeTask::Range> taskRanges;
            bool taskRangeChanged = false;
            if ((force || snapwnd.viewIdx0 != m_snapwnd.viewIdx0 || snapwnd.viewIdx1 != m_snapwnd.viewIdx1) &&
                (snapwnd.seekPos00 != INT64_MIN || snapwnd.seekPos10 != INT64_MIN))
            {
                int32_t buildIdx0 = snapwnd.cacheIdx0 >= 0 ? snapwnd.cacheIdx0 : 0;
                int32_t buildIdx1 = snapwnd.cacheIdx1 <= m_owner->m_vidMaxIndex ? snapwnd.cacheIdx1 : m_owner->m_vidMaxIndex;
                list<GopDecodeTaskHolder> goptskList;
                while (buildIdx0 <= buildIdx1)
                {
                    auto ptsPair = m_owner->GetSeekPosBySsIndex(buildIdx0);
                    auto ssIdxPair = m_owner->CalcSsIndexPairFromPtsPair(ptsPair);
                    if (ssIdxPair.second <= buildIdx0)
                    {
                        m_logger->Log(WARN) << "Snap window DOESN'T PROCEED! 'buildIdx0'(" << buildIdx0 << ") is NOT INCLUDED in the next 'ssIdxPair'["
                            << ssIdxPair.first << ", " << ssIdxPair.second << ")." << endl;
                        buildIdx0++;
                        continue;
                    }
                    bool isInView = (snapwnd.IsInView(ssIdxPair.first) && m_owner->IsSsIdxValid(ssIdxPair.first)) ||
                                    (snapwnd.IsInView(ssIdxPair.second) && m_owner->IsSsIdxValid(ssIdxPair.second));
                    int32_t distanceToViewWnd = isInView ? 0 : (ssIdxPair.second <= snapwnd.viewIdx0 ?
                            snapwnd.viewIdx0-ssIdxPair.second : ssIdxPair.first-snapwnd.viewIdx1);
                    if (distanceToViewWnd < 0) distanceToViewWnd = -distanceToViewWnd;
                    taskRanges.push_back(GopDecodeTask::Range(ptsPair, ssIdxPair, isInView, distanceToViewWnd));
                    buildIdx0 = ssIdxPair.second;
                }
                taskRangeChanged = true;
            }
            else if (snapwnd.seekPos00 == INT64_MIN && snapwnd.seekPos10 == INT64_MIN && !m_taskRanges.empty())
            {
                taskRangeChanged = true;
            }
            if (taskRangeChanged || snapwnd.wndpos != m_snapwnd.wndpos)
            {
                m_snapwnd = snapwnd;
                m_logger->Log(DEBUG) << ">>>>> Snapwnd updated: { wndpos=" << snapwnd.wndpos
                    << ", viewIdx=[" << snapwnd.viewIdx0 << ", " << snapwnd.viewIdx1
                    << "], cacheIdx=[" << snapwnd.cacheIdx0 << ", " << snapwnd.cacheIdx1 << "] } <<<<<<<" << endl;
            }
            if (taskRangeChanged)
            {
                m_logger->Log(DEBUG) << ">>>>> Task range list CHANGED <<<<<<<<" << endl << "\t";
                for (auto& range : taskRanges)
                    m_logger->Log(DEBUG) << "[" << range.SsIdx().first << ", " << range.SsIdx().second << "), ";
                m_logger->Log(DEBUG) << endl;
                lock_guard<mutex> lk(m_taskRangeLock);
                m_taskRanges = taskRanges;
                m_taskRangeChanged = true;
            }
        }

    private:
        ALogger* m_logger;
        SnapshotGenerator_Impl* m_owner;
        SnapWindow m_snapwnd;
        list<GopDecodeTask::Range> m_taskRanges;
        mutex m_taskRangeLock;
        bool m_taskRangeChanged{false};
    };

private:
    ALogger* m_logger;
    string m_errMsg;

    MediaParserHolder m_hParser;
    MediaInfo::InfoHolder m_hMediaInfo;
    MediaParser::SeekPointsHolder m_hSeekPoints;
    bool m_opened{false};
    bool m_prepared{false};
    recursive_mutex m_apiLock;
    bool m_quit{false};

    AVFormatContext* m_avfmtCtx{nullptr};
    int m_vidStmIdx{-1};
    int m_audStmIdx{-1};
    AVStream* m_vidStream{nullptr};
    AVStream* m_audStream{nullptr};
    AVCodecPtr m_viddec{nullptr};
    AVCodecContext* m_viddecCtx{nullptr};
    bool m_vidPreferUseHw{true};
    AVHWDeviceType m_vidUseHwType{AV_HWDEVICE_TYPE_NONE};
    AVPixelFormat m_vidHwPixFmt{AV_PIX_FMT_NONE};
    AVHWDeviceType m_viddecDevType{AV_HWDEVICE_TYPE_NONE};
    AVBufferRef* m_viddecHwDevCtx{nullptr};

    // demuxing thread
    thread m_demuxThread;
    uint32_t m_maxPendingTaskCountForDecoding = 8;
    // video decoding thread
    thread m_viddecThread;
    // update snapshots thread
    thread m_updateSsThread;

    int64_t m_vidStartMts{0};
    int64_t m_vidDurMts{0};
    int64_t m_vidFrmCnt{0};
    uint32_t m_vidMaxIndex;
    double m_snapWindowSize;
    double m_wndFrmCnt;
    double m_vidfrmIntvMts{0};
    double m_vidfrmIntvMtsHalf{0};
    int64_t m_vidfrmIntvPts{0};
    int64_t m_vidfrmIntvPtsHalf{0};
    double m_ssIntvMts{0};
    int64_t m_ssIntvPts{0};
    double m_cacheFactor{10.0};
    uint32_t m_maxCacheSize{0};
    uint32_t m_prevWndCacheSize;
    list<ViewerHolder> m_viewers;
    mutex m_viewerListLock;
    list<GopDecodeTaskHolder> m_goptskPrepareList;
    list<GopDecodeTaskHolder> m_goptskList;
    mutex m_goptskListReadLocks[3];
    atomic_int32_t m_pendingVidfrmCnt{0};
    int32_t m_maxPendingVidfrmCnt{4};
    // textures
    list<TextureHolder> m_deprecatedTextures;
    mutex m_deprecatedTextureLock;

    bool m_useRszFactor{false};
    bool m_ssSizeChanged{false};
    float m_ssWFacotr{1.f}, m_ssHFacotr{1.f};
    AVFrameToImMatConverter m_frmCvt;
};

ALogger* SnapshotGenerator_Impl::s_logger = nullptr;

ALogger* GetSnapshotGeneratorLogger()
{
    if (!SnapshotGenerator_Impl::s_logger)
        SnapshotGenerator_Impl::s_logger = GetLogger("MSnapshotGenerator");
    return SnapshotGenerator_Impl::s_logger;
}

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{
    SnapshotGenerator_Impl* ms = reinterpret_cast<SnapshotGenerator_Impl*>(ctx->opaque);
    const AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (ms->CheckHwPixFmt(*p))
            return *p;
    }
    return AV_PIX_FMT_NONE;
}

SnapshotGeneratorHolder CreateSnapshotGenerator()
{
    return SnapshotGeneratorHolder(static_cast<SnapshotGenerator*>(new SnapshotGenerator_Impl()), [] (SnapshotGenerator* ssgen) {
        SnapshotGenerator_Impl* ssgenimpl = dynamic_cast<SnapshotGenerator_Impl*>(ssgen);
        ssgenimpl->Close();
        delete ssgenimpl;
    });
}
