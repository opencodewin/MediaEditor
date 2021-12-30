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

    bool GetSnapshots(std::vector<ImGui::ImMat>& snapshots, double startPos) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!IsOpened())
            return false;

        snapshots.clear();

        SnapWindow snapWnd;
        if (m_prepared)
            snapWnd = UpdateSnapWindow(startPos);
        else
        {
            m_snapWnd.startPos = startPos;
            snapWnd = m_snapWnd;
        }

        lock_guard<mutex> lk2(m_bldtskByTimeLock);
        uint32_t i = snapWnd.index0;
        auto tskIter = m_bldtskTimeOrder.begin();
        while (tskIter != m_bldtskTimeOrder.end() && i <= snapWnd.index1)
        {
            auto& tsk = *tskIter++;
            if (tsk->ssIdxPair.first > snapWnd.index1)
                break;
            else if (tsk->ssIdxPair.second >= i)
            {
                auto ssIter = tsk->ssAry.begin();
                while (ssIter != tsk->ssAry.end())
                {
                    auto& ss = *ssIter++;
                    if (ss.index == i)
                    {
                        snapshots.push_back(ss.img);
                        i++;
                    }
                    else if (ss.index > i)
                    {
                        uint32_t j = ss.index < snapWnd.index1 ? ss.index : snapWnd.index1;
                        while (i < j)
                        {
                            ImGui::ImMat vmat;
                            vmat.time_stamp = CalcSnapshotTimestamp(i);
                            snapshots.push_back(vmat);
                            i++;
                        }
                        if (i == ss.index)
                            snapshots.push_back(ss.img);
                        else
                        {
                            ImGui::ImMat vmat;
                            vmat.time_stamp = CalcSnapshotTimestamp(i);
                            snapshots.push_back(vmat);
                        }
                        i++;
                    }
                    if (i > snapWnd.index1)
                        break;
                }
            }
        }
        while (i <= snapWnd.index1)
        {
            ImGui::ImMat vmat;
            vmat.time_stamp = CalcSnapshotTimestamp(i);
            snapshots.push_back(vmat);
            i++;
        }
        return true;
    }

    MediaParserHolder GetMediaParser() const override
    {
        return m_hParser;
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
        double minWndSize = CalcMinWindowSize(frameCount);
        if (windowSize < minWndSize)
            windowSize = minWndSize;
        double maxWndSize = GetMaxWindowSize();
        if (windowSize > maxWndSize)
            windowSize = maxWndSize;
        if (m_snapWindowSize == windowSize && m_wndFrmCnt == frameCount)
            return true;

        if (m_prepared)
        {
            WaitAllThreadsQuit();
            FlushAllQueues();
            if (m_viddecCtx)
                avcodec_flush_buffers(m_viddecCtx);
        }

        m_snapWindowSize = windowSize;
        m_wndFrmCnt = frameCount;

        if (m_prepared)
        {
            CalcWindowVariables();
            UpdateSnapWindow(m_snapWnd.startPos);
            ResetSnapshotBuildTask();
            m_snapWndUpdated = false;
        }

        StartAllThreads();

        Log(DEBUG) << ">>>> Config window: m_snapWindowSize=" << m_snapWindowSize << ", m_wndFrmCnt=" << m_wndFrmCnt
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
        ResetSnapshotBuildTask();
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
        if (m_frmCvt.GetOutWidth() == width && m_frmCvt.GetOutHeight() == height)
            return true;
        if (!m_frmCvt.SetOutSize(width, height))
        {
            m_errMsg = m_frmCvt.GetError();
            return false;
        }
        ResetSnapshotBuildTask();
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
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_frmCvt.GetOutColorFormat() == clrfmt)
            return true;
        if (!m_frmCvt.SetOutColorFormat(clrfmt))
        {
            m_errMsg = m_frmCvt.GetError();
            return false;
        }
        ResetSnapshotBuildTask();
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
        ResetSnapshotBuildTask();
        return true;
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

    int64_t GetVidoeMinPos() const override
    {
        return 0;
    }

    int64_t GetVidoeDuration() const override
    {
        return m_vidDurMts;
    }

    int64_t GetVidoeFrameCount() const override
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
        m_vidMaxIndex = (uint32_t)floor(((double)m_vidDurMts-m_vidfrmIntvMts)/m_ssIntvMts);
        m_maxCacheSize = (uint32_t)ceil(m_wndFrmCnt*m_cacheFactor);
        uint32_t intWndFrmCnt = (uint32_t)ceil(m_wndFrmCnt);
        if (m_maxCacheSize < intWndFrmCnt)
            m_maxCacheSize = intWndFrmCnt;
        if (m_maxCacheSize > m_vidMaxIndex+1)
            m_maxCacheSize = m_vidMaxIndex+1;
        m_prevWndCacheSize = (m_maxCacheSize-intWndFrmCnt)/2;
        m_postWndCacheSize = m_maxCacheSize-intWndFrmCnt-m_prevWndCacheSize;
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

        MediaInfo::InfoHolder hInfo = hParser->GetMediaInfo();
        m_vidStmIdx = hParser->GetBestVideoStreamIndex();
        m_audStmIdx = hParser->GetBestAudioStreamIndex();
        if (m_vidStmIdx < 0 && m_audStmIdx < 0)
        {
            ostringstream oss;
            oss << "Neither video nor audio stream can be found in '" << m_avfmtCtx->url << "'.";
            m_errMsg = oss.str();
            return false;
        }

        MediaInfo::VideoStream* vidStream = dynamic_cast<MediaInfo::VideoStream*>(hInfo->streams[m_vidStmIdx].get());
        m_vidStartMts = (int64_t)(vidStream->startTime*1000);
        m_vidDurMts = (int64_t)(vidStream->duration*1000);
        m_vidFrmCnt = vidStream->frameNum;
        AVRational avgFrmRate = { vidStream->avgFrameRate.num, vidStream->avgFrameRate.den };
        AVRational timebase = { vidStream->timebase.num, vidStream->timebase.den };
        m_vidfrmIntvMts = av_q2d(av_inv_q(avgFrmRate))*1000.;
        m_vidfrmIntvMtsHalf = ceil(m_vidfrmIntvMts)/2;
        if (avgFrmRate.num*avgFrmRate.num > 0)
            m_vidfrmIntvPts = (avgFrmRate.den*timebase.den)/(avgFrmRate.num*timebase.num);
        else
            m_vidfrmIntvPts = 0;

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

        return true;
    }

    bool Prepare()
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        m_hParser->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);
        m_hSeekPoints = m_hParser->GetVideoSeekPoints();
        if (!m_hSeekPoints)
        {
            m_errMsg = "FAILED to retrieve video seek points!";
            Log(ERROR) << m_errMsg << endl;
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
            UpdateSnapWindow((double)m_snapWnd.startPos/1000.);
            ResetSnapshotBuildTask();
            m_snapWndUpdated = false;
        }

        if (HasAudio())
        {
            m_audStream = m_avfmtCtx->streams[m_audStmIdx];

            // wyvern: disable opening audio decoder because we don't use it now
            // if (!OpenAudioDecoder())
            //     return false;
        }

        Log(DEBUG) << ">>>> Prepared: m_snapWindowSize=" << m_snapWindowSize << ", m_wndFrmCnt=" << m_wndFrmCnt
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
        Log(DEBUG) << "Use hardware device type '" << av_hwdevice_get_type_name(m_viddecDevType) << "'." << endl;

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
        Log(DEBUG) << "Video decoder(HW) '" << m_viddecCtx->codec->name << "' opened." << endl;
        return true;
    }

    void DemuxThreadProc()
    {
        Log(DEBUG) << "Enter DemuxThreadProc()..." << endl;

        if (!m_prepared && !Prepare())
        {
            Log(ERROR) << "Prepare() FAILED! Error is '" << m_errMsg << "'." << endl;
            return;
        }

        bool fatalError = false;
        AVPacket avpkt = {0};
        bool avpktLoaded = false;
        SnapshotBuildTask currTask = nullptr;
        int64_t lastPktPts;
        int32_t currPktSsIdx;
        uint32_t gopSmallestIdx;
        bool currPktChecked;
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
                    if (currTask && currTask->cancel)
                        Log(DEBUG) << "~~~~ Current demux task canceled" << endl;
                    currTask = FindNextDemuxTask();
                    if (currTask)
                    {
                        currTask->demuxing = true;
                        taskChanged = true;
                        currPktChecked = false;
                        lastPktPts = INT64_MAX;
                        currPktSsIdx = -1;
                        gopSmallestIdx = INT32_MAX;
                        // Log(DEBUG) << "--> Change demux task, ssIdxPair=(" << currTask->ssIdxPair.first << " ~ " << currTask->ssIdxPair.second
                        //     << ", startPts=" << currTask->seekPts.first << "(" << MillisecToString(CvtVidPtsToMts(currTask->seekPts.first)) << ")"
                        //     << ", endPts=" << currTask->seekPts.second << "(" << MillisecToString(CvtVidPtsToMts(currTask->seekPts.second)) << ")" << endl;
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
                                Log(WARN) << "WARNING! 'ptsAfterSeek'(" << ptsAfterSeek << ") != 'ssTask->startPts'(" << currTask->seekPts.first << ")!" << endl;
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
                            currPktChecked = false;
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

                    if (avpktLoaded)
                    {
                        if (avpkt.stream_index == m_vidStmIdx)
                        {
                            if (!currPktChecked)
                            {
                                int64_t mts = CvtVidPtsToMts(avpkt.pts);
                                uint32_t ssIdx;
                                bool isSs = IsSnapshotFrame(mts, ssIdx, avpkt.pts);
                                if (isSs && gopSmallestIdx > ssIdx)
                                    gopSmallestIdx = ssIdx;
                                if (currTask->isEndOfGop && isSs && ssIdx == currTask->ssIdxPair.second)
                                    lastPktPts = avpkt.pts;
                                currPktChecked = true;
                            }
                            if (avpkt.pts >= currTask->seekPts.second || avpkt.pts > lastPktPts)
                            {
                                if (gopSmallestIdx > currTask->ssIdxPair.first)
                                {
                                    Log(ERROR) << "!!!!!! ABNORMAL SS INDEX (1) !!!!!! demux task eof, but the smallest gop ss index is larger than task's first index, "
                                        << gopSmallestIdx << " > " << currTask->ssIdxPair.first << "." << endl;
                                }
                                if (currPktSsIdx < currTask->ssIdxPair.second)
                                {
                                    bool gopEnd = avpkt.pts >= currTask->seekPts.second;
                                    Log(ERROR) << "!!!!!! ABNORMAL SS INDEX (2) !!!!!! demux task eof, but last pkt index is " << currPktSsIdx
                                        << ", which is smaller than task's second index " << currTask->ssIdxPair.second
                                        << ", REASON is " << (gopEnd ? "GOP-END" : "SSIDX-END") << endl;
                                }
                                currTask->demuxerEof = true;
                            }

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
        } while (fferr >= 0 && !m_quitScan);
        if (m_quitScan)
            return false;
        return true;
    }

    void VideoDecodeThreadProc()
    {
        Log(DEBUG) << "Enter VideoDecodeThreadProc()..." << endl;

        while (!m_prepared && !m_quitScan)
            this_thread::sleep_for(chrono::milliseconds(5));

        SnapshotBuildTask currTask;
        AVFrame avfrm = {0};
        bool avfrmLoaded = false;
        bool inputEof = false;
        bool needResetDecoder = false;
        bool sentNullPacket = false;
        while (!m_quitScan)
        {
            bool idleLoop = true;
            bool quitLoop = false;

            if (currTask && currTask->cancel)
            {
                Log(DEBUG) << "~~~~ Current video task canceled" << endl;
                if (avfrmLoaded)
                {
                    av_frame_unref(&avfrm);
                    avfrmLoaded = false;
                }
                currTask = nullptr;
            }

            if (!currTask || currTask->decoderEof)
            {
                SnapshotBuildTask oldTask = currTask;
                currTask = FindNextDecoderTask();
                if (currTask)
                {
                    currTask->decoding = true;
                    inputEof = false;
                    // Log(DEBUG) << "==> Change decoding task to build index (" << currTask->ssIdxPair.first << " ~ " << currTask->ssIdxPair.second << ")." << endl;
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
                            // Log(DEBUG) << "<<< Get video frame pts=" << avfrm.pts << "(" << MillisecToString(CvtVidPtsToMts(avfrm.pts)) << ")." << endl;
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
                                break;
                            }
                            else
                            {
                                idleLoop = false;
                                needResetDecoder = true;
                                // Log(DEBUG) << "Video decoder current task reaches EOF!" << endl;
                            }
                        }
                    }

                    hasOutput = avfrmLoaded;
                    if (avfrmLoaded)
                    {
                        int64_t mts = CvtVidPtsToMts(avfrm.pts);
                        uint32_t index;
                        bool isSnapshot = IsSnapshotFrame(mts, index, avfrm.pts);
                        if (!isSnapshot)
                        {
                            av_frame_unref(&avfrm);
                            avfrmLoaded = false;
                            idleLoop = false;
                        }
                        else if (m_pendingVidfrmCnt < m_maxPendingVidfrmCnt)
                        {
                            EnqueueSnapshotAVFrame(&avfrm, index);
                            av_frame_unref(&avfrm);
                            avfrmLoaded = false;
                            idleLoop = false;
                        }
                    }
                } while (hasOutput && !m_quitScan);
                if (quitLoop)
                    break;

                // input packet to decoder
                if (!inputEof && !sentNullPacket)
                {
                    if (!currTask->avpktQ.empty())
                    {
                        AVPacket* avpkt = currTask->avpktQ.front();
                        int fferr = avcodec_send_packet(m_viddecCtx, avpkt);
                        if (fferr == 0)
                        {
                            // Log(DEBUG) << ">>> Send video packet pts=" << avpkt->pts << "(" << MillisecToString(CvtVidPtsToMts(avpkt->pts)) << ")." << endl;
                            {
                                lock_guard<mutex> lk(currTask->avpktQLock);
                                currTask->avpktQ.pop_front();
                            }
                            av_packet_free(&avpkt);
                            idleLoop = false;
                        }
                        else if (fferr != AVERROR(EAGAIN) && fferr != AVERROR_INVALIDDATA)
                        {
                            Log(ERROR) << "FAILED to invoke 'avcodec_send_packet'(VideoDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
                            quitLoop = true;
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
        Log(DEBUG) << "Leave VideoDecodeThreadProc()." << endl;
    }

    void UpdateSnapshotThreadProc()
    {
        Log(DEBUG) << "Enter UpdateSnapshotThreadProc()." << endl;
        SnapshotBuildTask currTask;
        while (!m_quitScan)
        {
            bool idleLoop = true;

            if (!currTask || currTask->cancel || currTask->ssfrmCnt <= 0)
            {
                currTask = FindNextSsUpdateTask();
            }

            if (currTask)
            {
                AVFrame* ssfrm = nullptr;
                for (Snapshot& ss : currTask->ssAry)
                {
                    if (ss.avfrm)
                    {
                        double ts = (double)CvtVidPtsToMts(ss.avfrm->pts)/1000.;
                        if (!m_frmCvt.ConvertImage(ss.avfrm, ss.img, ts))
                            Log(ERROR) << "FAILED to convert AVFrame to ImGui::ImMat! Message is '" << m_frmCvt.GetError() << "'." << endl;
                        av_frame_free(&ss.avfrm);
                        ss.avfrm = nullptr;
                        currTask->ssfrmCnt--;
                        if (currTask->ssfrmCnt < 0)
                            Log(ERROR) << "!! ABNORMAL !! Task [" << currTask->ssIdxPair.first << ", " << currTask->ssIdxPair.second << "] has negative 'ssfrmCnt'("
                                << currTask->ssfrmCnt << ")!" << endl;
                        m_pendingVidfrmCnt--;
                        if (m_pendingVidfrmCnt < 0)
                            Log(ERROR) << "Pending video AVFrame ptr count is NEGATIVE! " << m_pendingVidfrmCnt << endl;
                        idleLoop = false;
                    }
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        Log(DEBUG) << "Leave UpdateSnapshotThreadProc()." << endl;
    }

    void StartAllThreads()
    {
        m_quitScan = false;
        m_demuxThread = thread(&MediaSnapshot_Impl::DemuxThreadProc, this);
        if (HasVideo())
            m_viddecThread = thread(&MediaSnapshot_Impl::VideoDecodeThreadProc, this);
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
        if (m_updateSsThread.joinable())
        {
            m_updateSsThread.join();
            m_updateSsThread = thread();
        }
    }

    void FlushAllQueues()
    {
        m_bldtskPriOrder.clear();
        m_bldtskTimeOrder.clear();
        for (AVPacket* avpkt : m_audpktQ)
            av_packet_free(&avpkt);
        m_audpktQ.clear();
    }

    struct Snapshot
    {
        AVFrame* avfrm{nullptr};
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
        int64_t seekPos00;
        int64_t seekPos10;
    };

    struct _SnapshotBuildTask
    {
        _SnapshotBuildTask(MediaSnapshot_Impl& obj) : outterObj(obj) {}

        ~_SnapshotBuildTask()
        {
            for (AVPacket* avpkt : avpktQ)
                av_packet_free(&avpkt);
            for (Snapshot& ss : ssAry)
            {
                if (ss.avfrm)
                {
                    av_frame_free(&ss.avfrm);
                    outterObj.m_pendingVidfrmCnt--;
                }
            }
        }

        MediaSnapshot_Impl& outterObj;
        pair<int64_t, int64_t> seekPts;
        pair<uint32_t, uint32_t> ssIdxPair;
        bool isEndOfGop{false};
        list<Snapshot> ssAry;
        atomic_int32_t ssfrmCnt{0};
        // mutex ssAryLock;
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
        int64_t mts0 = (int64_t)round(startPos*1000);
        if (mts0 < 0)
            mts0 = 0;
        uint32_t index0 = (int32_t)floor((double)mts0/m_ssIntvMts);
        int64_t mts1 = (int64_t)round(mts0+m_snapWindowSize*1000);
        uint32_t index1 = (int32_t)floor((double)mts1/m_ssIntvMts);
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
        pair<int64_t, int64_t> seekPos0 = GetSeekPosBySsIndex(cacheIdx0);
        pair<int64_t, int64_t> seekPos1 = GetSeekPosBySsIndex(cacheIdx1);
        snapWnd = { startPos, index0, index1, cacheIdx0, cacheIdx1, seekPos0.first, seekPos1.first };
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
        index = (int32_t)round((double)mts/m_ssIntvMts);
        double diff = abs(index*m_ssIntvMts-mts);
        bool isSs = diff <= m_vidfrmIntvMtsHalf;
        // Log(DEBUG) << "---> IsSnapshotFrame : pts=" << pts << ", mts=" << mts << ", index=" << index << ", diff=" << diff << ", m_vidfrmIntvMtsHalf=" << m_vidfrmIntvMtsHalf << endl;
        return isSs;
    }

    bool IsSpecificSnapshotFrame(uint32_t index, int64_t mts)
    {
        double diff = abs(index*m_ssIntvMts-mts);
        return diff <= m_vidfrmIntvMtsHalf;
    }

    int64_t CalcSnapshotMts(uint32_t index)
    {
        uint32_t frameCount = (uint32_t)round(index*m_ssIntvMts/m_vidfrmIntvMts);
        return (int64_t)round(frameCount*m_vidfrmIntvMts);
    }

    double CalcSnapshotTimestamp(uint32_t index)
    {
        return (double)CalcSnapshotMts(index)/1000.;
    }

    pair<int64_t, int64_t> GetSeekPosByMts(int64_t mts)
    {
        int64_t targetPts = CvtVidMtsToPts(mts);
        auto iter = find_if(m_hSeekPoints->begin(), m_hSeekPoints->end(),
            [targetPts](int64_t keyPts) { return keyPts > targetPts; });
        if (iter != m_hSeekPoints->begin())
            iter--;
        int64_t first = *iter++;
        int64_t second = iter == m_hSeekPoints->end() ? INT64_MAX : *iter;
        if (targetPts >= second || (second-targetPts) < m_vidfrmIntvPts/2)
        {
            first = second;
            iter++;
            second = iter == m_hSeekPoints->end() ? INT64_MAX : *iter;
        }
        return { first, second };
    }

    pair<int64_t, int64_t> GetSeekPosBySsIndex(uint32_t index)
    {
        return GetSeekPosByMts(CalcSnapshotMts(index));
    }

    void ResetSnapshotBuildTask()
    {
        if (m_maxCacheSize == 0)
            return;

        SnapWindow currwnd = m_snapWnd;
        lock_guard<mutex> lk(m_bldtskByTimeLock);
        if (!m_bldtskTimeOrder.empty())
        {
            for (auto& tsk : m_bldtskTimeOrder)
                tsk->cancel = true;
            m_bldtskTimeOrder.clear();
        }
        uint32_t buildIndex0 = currwnd.cacheIdx0;
        uint32_t buildIndex1 = currwnd.cacheIdx1;
        SnapshotBuildTask task = nullptr;
        while (buildIndex0 <= buildIndex1)
        {
            pair<int64_t, int64_t> seekPos = GetSeekPosBySsIndex(buildIndex0);
            if (!task || task->seekPts != seekPos)
            {
                if (task)
                {
                    task->isEndOfGop = true;
                    m_bldtskTimeOrder.push_back(task);
                }
                task = make_shared<_SnapshotBuildTask>(*this);
                task->seekPts = seekPos;
                task->ssIdxPair = { buildIndex0, buildIndex0 };
            }
            else
                task->ssIdxPair.second = buildIndex0;
            buildIndex0++;
        }
        if (task)
            m_bldtskTimeOrder.push_back(task);
        m_bldtskSnapWnd = currwnd;
        Log(DEBUG) << "^^^ Initialized build task, index = ["
            << m_bldtskSnapWnd.cacheIdx0 << ", (" << m_bldtskSnapWnd.index0 << ", " << m_bldtskSnapWnd.index1 << "), " << m_bldtskSnapWnd.cacheIdx1 << "]." << endl;

        UpdateBuildTaskByPriority();
    }

    void UpdateSnapshotBuildTask()
    {
        SnapWindow currwnd = m_snapWnd;
        if (currwnd.cacheIdx0 != m_bldtskSnapWnd.cacheIdx0)
        {
            Log(DEBUG) << "^^^ Updating build task, index changed from ["
                << m_bldtskSnapWnd.cacheIdx0 << ", (" << m_bldtskSnapWnd.index0 << ", " << m_bldtskSnapWnd.index1 << "), " << m_bldtskSnapWnd.cacheIdx1 << "] to ["
                << currwnd.cacheIdx0 << ", (" << currwnd.index0 << ", " << currwnd.index1 << "), " << currwnd.cacheIdx1 << "]." << endl;
            lock_guard<mutex> lk(m_bldtskByTimeLock);
            SnapshotBuildTask task = nullptr;
            if (currwnd.cacheIdx0 > m_bldtskSnapWnd.cacheIdx0)
            {
                uint32_t buildIndex0 = currwnd.cacheIdx0;
                uint32_t buildIndex1 = currwnd.cacheIdx1;
                if (currwnd.seekPos00 <= m_bldtskSnapWnd.seekPos10)
                {
                    buildIndex0 = m_bldtskSnapWnd.cacheIdx1+1;
                    auto iter = m_bldtskTimeOrder.begin();
                    while (iter != m_bldtskTimeOrder.end())
                    {
                        auto& tsk = *iter;
                        if (tsk->seekPts.first < currwnd.seekPos00)
                        {
                            tsk->cancel = true;
                            iter = m_bldtskTimeOrder.erase(iter);
                        }
                        else
                        {
                            tsk->ssIdxPair.first = currwnd.cacheIdx0;
                            break;
                        }
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

                pair<uint32_t, uint32_t> oldSsIdxPair;
                bool hasOldTask = task != nullptr;
                if (hasOldTask) oldSsIdxPair = task->ssIdxPair;
                while (buildIndex0 <= buildIndex1)
                {
                    pair<int64_t, int64_t> seekPos = GetSeekPosBySsIndex(buildIndex0);
                    if (!task || task->seekPts != seekPos)
                    {
                        if (task)
                        {
                            if (hasOldTask)
                            {
                                if (oldSsIdxPair != task->ssIdxPair)
                                    Log(DEBUG) << "~~~ Update old task, ss index pair updated from [" << oldSsIdxPair.first << ", " << oldSsIdxPair.second << "] to ["
                                        << task->ssIdxPair.first << ", " << task->ssIdxPair.second << "]." << endl;
                                hasOldTask = false;
                            }
                            task->isEndOfGop = true;
                            m_bldtskTimeOrder.push_back(task);
                        }
                        task = make_shared<_SnapshotBuildTask>(*this);
                        task->seekPts = seekPos;
                        task->ssIdxPair = { buildIndex0, buildIndex0 };
                    }
                    else
                        task->ssIdxPair.second = buildIndex0;
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
                        if (tsk->seekPts.first > currwnd.seekPos10)
                        {
                            tsk->cancel = true;
                            iter = m_bldtskTimeOrder.erase(iter);
                            iter--;
                        }
                        else
                        {
                            if (tsk->ssIdxPair.second > currwnd.cacheIdx1)
                            {
                                tsk->isEndOfGop = false;
                                tsk->ssIdxPair.second = currwnd.cacheIdx1;
                            }
                            break;
                        }
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

                pair<uint32_t, uint32_t> oldSsIdxPair;
                bool hasOldTask = task != nullptr;
                if (hasOldTask) oldSsIdxPair = task->ssIdxPair;
                bool firstTask = true;
                while (buildIndex1 >= buildIndex0)
                {
                    pair<int64_t, int64_t> seekPos = GetSeekPosBySsIndex(buildIndex1);
                    if (!task || task->seekPts != seekPos)
                    {
                        if (task)
                        {
                            if (hasOldTask)
                            {
                                if (oldSsIdxPair != task->ssIdxPair)
                                    Log(DEBUG) << "~~~ Update old task, ss index pair updated from [" << oldSsIdxPair.first << ", " << oldSsIdxPair.second << "] to ["
                                        << task->ssIdxPair.first << ", " << task->ssIdxPair.second << "]." << endl;
                                hasOldTask = false;
                            }
                            m_bldtskTimeOrder.push_front(task);
                            firstTask = false;
                        }
                        task = make_shared<_SnapshotBuildTask>(*this);
                        task->seekPts = seekPos;
                        task->isEndOfGop = firstTask ? false : true;
                        task->ssIdxPair = { buildIndex1, buildIndex1 };
                    }
                    else
                        task->ssIdxPair.first = buildIndex1;
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
        lock_guard<mutex> lk(m_bldtskByPriLock);
        SnapWindow swnd = m_bldtskSnapWnd;
        m_bldtskPriOrder = m_bldtskTimeOrder;
        m_bldtskPriOrder.sort([swnd](const SnapshotBuildTask& a, const SnapshotBuildTask& b) {
            uint32_t aFront = a->ssIdxPair.first;
            uint32_t aBack = a->ssIdxPair.second;
            bool aInDisplayWindow = aFront >= swnd.index0 && aFront <= swnd.index1 ||
                aBack >= swnd.index0 && aBack <= swnd.index1;
            uint32_t bFront = b->ssIdxPair.first;
            uint32_t bBack = b->ssIdxPair.second;
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
        for (auto& tsk : m_bldtskPriOrder)
            if (!tsk->cancel && !tsk->demuxing)
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
        lock_guard<mutex> lk(m_bldtskByPriLock);
        SnapshotBuildTask nxttsk = nullptr;
        for (auto& tsk : m_bldtskPriOrder)
            if (!tsk->cancel && tsk->demuxing && !tsk->decoding)
            {
                nxttsk = tsk;
                break;
            }
        return nxttsk;
    }

    SnapshotBuildTask FindNextSsUpdateTask()
    {
        lock_guard<mutex> lk(m_bldtskByPriLock);
        SnapshotBuildTask nxttsk = nullptr;
        for (auto& tsk : m_bldtskPriOrder)
            if (!tsk->cancel && tsk->ssfrmCnt > 0)
            {
                nxttsk = tsk;
                break;
            }
        return nxttsk;
    }

    bool EnqueueSnapshotAVFrame(AVFrame* frm, uint32_t ssIdx)
    {
        lock_guard<mutex> lk(m_bldtskByPriLock);
        auto iter = find_if(m_bldtskPriOrder.begin(), m_bldtskPriOrder.end(), [frm](const SnapshotBuildTask& task) {
            return frm->pts >= task->seekPts.first && frm->pts < task->seekPts.second;
        });
        if (iter != m_bldtskPriOrder.end())
        {
            Snapshot ss;
            ss.index = ssIdx;
            ss.avfrm = av_frame_clone(frm);
            if (!ss.avfrm)
            {
                Log(ERROR) << "FAILED to invoke 'av_frame_clone()' to allocate new AVFrame for SS!" << endl;
                return false;
            }
            // Log(DEBUG) << "Adding SS#" << ssIdx << "." << endl;
            auto& task = *iter;
            if (task->ssAry.empty())
            {
                task->ssAry.push_back(ss);
                task->ssfrmCnt++;
                m_pendingVidfrmCnt++;
            }
            else
            {
                auto ssRvsIter = find_if(task->ssAry.rbegin(), task->ssAry.rend(), [ssIdx](const Snapshot& ss) {
                    return ss.index <= ssIdx;
                });
                if (ssRvsIter != task->ssAry.rend() && ssRvsIter->index == ssIdx)
                {
                    Log(DEBUG) << "Found duplicated SS#" << ssIdx << ", dropping this SS. pts=" << frm->pts
                        << ", t=" << MillisecToString(CvtVidPtsToMts(frm->pts)) << "." << endl;
                    if (ss.avfrm)
                        av_frame_free(&ss.avfrm);
                }
                else
                {
                    auto ssFwdIter = ssRvsIter.base();
                    task->ssAry.insert(ssFwdIter, ss);
                    task->ssfrmCnt++;
                    m_pendingVidfrmCnt++;
                }
            }
            return true;
        }
        else
        {
            Log(DEBUG) << "Dropping SS#" << ssIdx << " due to no matching task is found." << endl;
        }
        return false;
    }

private:
    string m_errMsg;

    MediaParserHolder m_hParser;
    MediaParser::SeekPointsHolder m_hSeekPoints;
    bool m_opened{false};
    bool m_prepared{false};

    AVFormatContext* m_avfmtCtx{nullptr};
    int m_vidStmIdx{-1};
    int m_audStmIdx{-1};
    AVStream* m_vidStream{nullptr};
    AVStream* m_audStream{nullptr};
#if LIBAVFORMAT_VERSION_MAJOR >= 59
    const AVCodec* m_viddec{nullptr};
#else
    AVCodec* m_viddec{nullptr};
#endif
    AVCodecContext* m_viddecCtx{nullptr};
    bool m_vidPreferUseHw{true};
    AVHWDeviceType m_vidUseHwType{AV_HWDEVICE_TYPE_NONE};
    AVPixelFormat m_vidHwPixFmt{AV_PIX_FMT_NONE};
    AVHWDeviceType m_viddecDevType{AV_HWDEVICE_TYPE_NONE};
    AVBufferRef* m_viddecHwDevCtx{nullptr};

    // demuxing thread
    thread m_demuxThread;
    uint32_t m_maxPendingTaskCountForDecoding = 8;
    int m_audpktQMaxSize{64};
    list<AVPacket*> m_audpktQ;
    mutex m_audpktQLock;
    // video decoding thread
    thread m_viddecThread;
    // update snapshots thread
    thread m_updateSsThread;

    recursive_mutex m_apiLock;
    bool m_quitScan{false};

    float m_ssWFacotr{1.f}, m_ssHFacotr{1.f};
    bool m_ssSizeChanged{false};
    int64_t m_vidStartMts{0};
    int64_t m_vidDurMts{0};
    int64_t m_vidFrmCnt{0};
    uint32_t m_vidMaxIndex;
    double m_snapWindowSize;
    double m_wndFrmCnt;
    double m_vidfrmIntvMts;
    double m_vidfrmIntvMtsHalf;
    int64_t m_vidfrmIntvPts;
    double m_ssIntvMts;
    double m_cacheFactor{10.0};
    uint32_t m_maxCacheSize{0};
    uint32_t m_prevWndCacheSize, m_postWndCacheSize;
    SnapWindow m_snapWnd;
    atomic_bool m_snapWndUpdated{false};
    SnapWindow m_bldtskSnapWnd;
    list<SnapshotBuildTask> m_bldtskTimeOrder;
    mutex m_bldtskByTimeLock;
    list<SnapshotBuildTask> m_bldtskPriOrder;
    mutex m_bldtskByPriLock;
    atomic_int32_t m_pendingVidfrmCnt{0};
    int32_t m_maxPendingVidfrmCnt{4};
    // list<int64_t> m_vidKeyPtsList;
    double m_fixedSsInterval;
    uint32_t m_fixedSnapshotCount{100};

    AVFrameToImMatConverter m_frmCvt;
};

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
