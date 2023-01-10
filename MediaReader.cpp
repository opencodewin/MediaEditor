#include <thread>
#include <mutex>
#include <sstream>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <list>
#include "MediaReader.h"
#include "FFUtils.h"
extern "C"
{
    #include "libavutil/avutil.h"
    #include "libavutil/avstring.h"
    #include "libavutil/pixdesc.h"
    #include "libavutil/channel_layout.h"
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

class MediaReader_Impl : public MediaReader
{
public:
    static ALogger* s_logger;

    MediaReader_Impl(const string& loggerName = "")
    {
        m_id = s_idCounter++;
        if (loggerName.empty())
        {
            m_logger = GetMediaReaderLogger();
        }
        else
        {
            m_logger = GetLogger(loggerName);
            m_logger->SetShowLevels(DEBUG);
        }
    }

    MediaReader_Impl(const MediaReader_Impl&) = delete;
    MediaReader_Impl(MediaReader_Impl&&) = delete;
    MediaReader_Impl& operator=(const MediaReader_Impl&) = delete;

    virtual ~MediaReader_Impl() {}

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

        if (!OpenMedia(hParser))
        {
            Close();
            return false;
        }
        m_hParser = hParser;
        m_close = false;
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

        if (IsOpened())
            Close();

        if (!OpenMedia(hParser))
        {
            Close();
            return false;
        }
        m_hParser = hParser;
        m_close = false;
        m_opened = true;
        return true;
    }

    MediaParserHolder GetMediaParser() const override
    {
        return m_hParser;
    }

    bool ConfigVideoReader(
            uint32_t outWidth, uint32_t outHeight,
            ImColorFormat outClrfmt, ImInterpolateMode rszInterp) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_opened)
        {
            m_errMsg = "This 'MediaReader' instance is NOT OPENED yet!";
            return false;
        }
        if (m_started)
        {
            m_errMsg = "This 'MediaReader' instance is ALREADY STARTED!";
            return false;
        }
        if (m_vidStmIdx < 0)
        {
            m_errMsg = "Can NOT configure this 'MediaReader' as video reader since no video stream is found!";
            return false;
        }

        auto vidStream = GetVideoStream();
        m_isImage = vidStream->isImage;

        if (!m_frmCvt.SetOutSize(outWidth, outHeight))
        {
            m_errMsg = m_frmCvt.GetError();
            return false;
        }
        if (!m_frmCvt.SetOutColorFormat(outClrfmt))
        {
            m_errMsg = m_frmCvt.GetError();
            return false;
        }
        if (!m_frmCvt.SetResizeInterpolateMode(rszInterp))
        {
            m_errMsg = m_frmCvt.GetError();
            return false;
        }

        m_vidDurTs = vidStream->duration;
        AVRational avgFrmRate = { vidStream->avgFrameRate.num, vidStream->avgFrameRate.den };
        AVRational timebase = { vidStream->timebase.num, vidStream->timebase.den };
        m_vidfrmIntvMts = av_q2d(av_inv_q(avgFrmRate))*1000.;

        m_isVideoReader = true;
        m_configured = true;
        return true;
    }

    bool ConfigVideoReader(
            float outWidthFactor, float outHeightFactor,
            ImColorFormat outClrfmt, ImInterpolateMode rszInterp) override
    {
        if (!m_opened)
        {
            m_errMsg = "Can NOT configure a 'MediaReader' until it's been configured!";
            return false;
        }
        if (m_started)
        {
            m_errMsg = "Can NOT configure a 'MediaReader' after it's already started!";
            return false;
        }
        if (m_vidStmIdx < 0)
        {
            m_errMsg = "Can NOT configure this 'MediaReader' as video reader since no video stream is found!";
            return false;
        }
        lock_guard<recursive_mutex> lk(m_apiLock);

        auto vidStream = GetVideoStream();
        m_isImage = vidStream->isImage;

        m_ssWFacotr = outWidthFactor;
        m_ssHFacotr = outHeightFactor;
        uint32_t outWidth = (uint32_t)ceil(vidStream->width*outWidthFactor);
        if ((outWidth&0x1) == 1)
            outWidth++;
        uint32_t outHeight = (uint32_t)ceil(vidStream->height*outHeightFactor);
        if ((outHeight&0x1) == 1)
            outHeight++;
        if (!m_frmCvt.SetOutSize(outWidth, outHeight))
        {
            m_errMsg = m_frmCvt.GetError();
            return false;
        }
        if (!m_frmCvt.SetOutColorFormat(outClrfmt))
        {
            m_errMsg = m_frmCvt.GetError();
            return false;
        }
        if (!m_frmCvt.SetResizeInterpolateMode(rszInterp))
        {
            m_errMsg = m_frmCvt.GetError();
            return false;
        }

        m_vidDurTs = vidStream->duration;
        AVRational avgFrmRate = { vidStream->avgFrameRate.num, vidStream->avgFrameRate.den };
        AVRational timebase = { vidStream->timebase.num, vidStream->timebase.den };
        m_vidfrmIntvMts = av_q2d(av_inv_q(avgFrmRate))*1000.;

        m_isVideoReader = true;
        m_configured = true;
        return true;
    }

    bool ConfigAudioReader(uint32_t outChannels, uint32_t outSampleRate, const string& outPcmFormat, uint32_t audioStreamIndex) override
    {
        if (!m_opened)
        {
            m_errMsg = "Can NOT configure a 'MediaReader' until it's been configured!";
            return false;
        }
        if (m_started)
        {
            m_errMsg = "Can NOT configure a 'MediaReader' after it's already started!";
            return false;
        }
        if (m_audStmIdx < 0)
        {
            m_errMsg = "Can NOT configure this 'MediaReader' as audio reader since no audio stream is found!";
            return false;
        }
        AVSampleFormat outSmpfmt = av_get_sample_fmt(outPcmFormat.c_str());
        if (outSmpfmt == AV_SAMPLE_FMT_NONE)
        {
            ostringstream oss;
            oss << "Invalid argument 'outPcmFormat'! '" << outPcmFormat << "' is NOT a SAMPLE FORMAT.";
            m_errMsg = oss.str();
            return false;
        }
        lock_guard<recursive_mutex> lk(m_apiLock);
        m_swrOutSmpfmt = outSmpfmt;
        m_isOutFmtPlanar = av_sample_fmt_is_planar(outSmpfmt);

        m_swrOutSampleRate = outSampleRate;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        m_swrOutChannels = outChannels;
#else
        av_channel_layout_default(&m_swrOutChlyt, outChannels);
#endif

        uint32_t index = 0;
        int audioIndexInStreams = -1;
        for (auto i = 0; i < m_avfmtCtx->nb_streams; i++)
        {
            if (m_avfmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                if (audioStreamIndex == index)
                {
                    audioIndexInStreams = i;
                    break;
                }
                index++;
            }
        }
        if (index != audioStreamIndex)
        {
            ostringstream oss;
            oss << "Invalid argument 'audioStreamIndex'(" << audioStreamIndex << "), NO SUCH AUDIO stream in the source!";
            m_errMsg = oss.str();
            return false;
        }
        m_audStmIdx = audioIndexInStreams;

        if (m_audStmIdx >= 0)
        {
            MediaInfo::AudioStream* audStream = dynamic_cast<MediaInfo::AudioStream*>(m_hMediaInfo->streams[m_audStmIdx].get());
            m_audDurTs = audStream->duration;
            m_audFrmSize = (audStream->bitDepth>>3)*audStream->channels;
        }

        m_isVideoReader = false;
        m_configured = true;

        if (m_dumpPcm)
        {
            if (m_fpPcmFile)
            {
                fclose(m_fpPcmFile);
                m_fpPcmFile = nullptr;
            }
            m_fpPcmFile = fopen("MediaReaderDump.pcm", "wb");
        }
        return true;
    }

    bool Start(bool suspend) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_configured)
        {
            m_errMsg = "This 'MediaReader' instance is NOT CONFIGURED yet!";
            return false;
        }
        if (m_started)
            return true;

        if (!suspend || !m_isVideoReader)
            StartAllThreads();
        else
            ReleaseVideoResource();
        m_started = true;
        return true;
    }

    bool Stop() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_configured)
        {
            m_errMsg = "This 'MediaReader' instance is NOT CONFIGURED yet!";
            return false;
        }
        if (!m_started)
            return true;

        WaitAllThreadsQuit();
        FlushAllQueues();

        if (m_swrCtx)
        {
            swr_free(&m_swrCtx);
            m_swrCtx = nullptr;
        }
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        m_swrOutChannels = 0;
        m_swrOutChnLyt = 0;
#else
        m_swrOutChlyt = {AV_CHANNEL_ORDER_UNSPEC, 0};
#endif
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
        m_vidAvStm = nullptr;
        m_audAvStm = nullptr;
        m_viddec = nullptr;
        m_auddec = nullptr;

        m_prevReadPos = 0;
        m_prevReadImg.release();
        m_readForward = true;
        m_seekPosUpdated = false;
        m_seekPosTs = 0;
        m_vidfrmIntvMts = 0;
        m_hSeekPoints = nullptr;
        m_vidDurTs = 0;
        m_audDurTs = 0;
        m_audFrmSize = 0;
        m_audReadTask = nullptr;
        m_audReadOffset = -1;
        m_audReadEof = false;
        m_audReadNextTaskSeekPts0 = INT64_MIN;
        m_swrFrmSize = 0;
        m_swrOutStartTime = 0;

        m_prepared = false;
        m_started = false;
        m_configured = false;
        m_errMsg = "";

        if (m_fpPcmFile)
        {
            fclose(m_fpPcmFile);
            m_fpPcmFile = NULL;
        }
        return true;
    }

    void Close() override
    {
        m_close = true;
        lock_guard<recursive_mutex> lk(m_apiLock);
        WaitAllThreadsQuit();
        FlushAllQueues();

        if (m_swrCtx)
        {
            swr_free(&m_swrCtx);
            m_swrCtx = nullptr;
        }
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        m_swrOutChannels = 0;
        m_swrOutChnLyt = 0;
#else
        m_swrOutChlyt = {AV_CHANNEL_ORDER_UNSPEC, 0};
#endif
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
        m_vidAvStm = nullptr;
        m_audAvStm = nullptr;
        m_viddec = nullptr;
        m_auddec = nullptr;
        m_hParser = nullptr;
        m_hMediaInfo = nullptr;

        m_prevReadPos = 0;
        m_prevReadImg.release();
        m_readForward = true;
        m_seekPosUpdated = false;
        m_seekPosTs = 0;
        m_vidfrmIntvMts = 0;
        m_hSeekPoints = nullptr;
        m_vidDurTs = 0;
        m_audDurTs = 0;
        m_audFrmSize = 0;
        m_audReadTask = nullptr;
        m_audReadOffset = -1;
        m_audReadEof = false;
        m_audReadNextTaskSeekPts0 = INT64_MIN;
        m_swrFrmSize = 0;
        m_swrOutStartTime = 0;

        m_prepared = false;
        m_started = false;
        m_configured = false;
        m_streamInfoFound = false;
        m_opened = false;
        m_errMsg = "";

        if (m_fpPcmFile)
        {
            fclose(m_fpPcmFile);
            m_fpPcmFile = NULL;
        }
    }

    bool SeekTo(double ts) override
    {
        if (!m_configured)
        {
            m_errMsg = "Can NOT use 'SeekTo' until the 'MediaReader' obj is configured!";
            return false;
        }

        double stmdur = m_isVideoReader ? m_vidDurTs : m_audDurTs;
        if (ts < 0 || ts > stmdur)
        {
            m_errMsg = "INVALID argument 'ts'! Can NOT be negative or exceed the duration.";
            return false;
        }

        if (m_isVideoReader)
        {
            bool deferred = true;
            if (m_apiLock.try_lock())
            {
                if (m_prepared)
                {
                    UpdateCacheWindow(ts);
                    deferred = false;
                }
                m_apiLock.unlock();
            }
            if (deferred)
            {
                lock_guard<mutex> lk(m_seekPosLock);
                m_seekPosTs = ts;
                m_seekPosUpdated = true;
            }
        }
        else
        {
            lock_guard<recursive_mutex> lk(m_apiLock);
            if (m_prepared)
            {
                UpdateCacheWindow(ts);
                m_audReadTask = nullptr;
                m_audReadEof = false;
                m_audReadNextTaskSeekPts0 = INT64_MIN;
            }
            else
            {
                m_seekPosTs = ts;
                m_seekPosUpdated = true;
            }
        }
        return true;
    }

    void SetDirection(bool forward) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_opened)
        {
            m_errMsg = "This 'MediaReader' instance is NOT OPENED yet!";
            return;
        }
        if (m_readForward != forward)
        {
            m_readForward = forward;
            if (m_prepared)
                UpdateCacheWindow(m_cacheWnd.readPos, true);
            m_audReadEof = false;
        }
    }

    void Suspend() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This 'MediaReader' is NOT started yet!";
            return;
        }
        if (m_quitThread || !m_isVideoReader || m_isImage)
            return;

        ReleaseVideoResource();
    }

    void Wakeup() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This 'MediaReader' is NOT started yet!";
            return;
        }
        if (!m_quitThread || !m_isVideoReader || m_isImage)
            return;

        double readPos = m_prevReadPos;
        if (m_seekPosUpdated)
            readPos = m_seekPosTs;

        if (!OpenMedia(m_hParser))
        {
            m_logger->Log(Error) << "FAILED to re-open media when waking up this MediaReader!" << endl;
            return;
        }

        m_seekPosTs = readPos;
        m_seekPosUpdated = true;

        StartAllThreads();
    }

    bool IsSuspended() const override
    {
        return m_started && m_quitThread;
    }

    bool IsPlanar() const override
    {
        return m_isOutFmtPlanar;
    }

    bool IsDirectionForward() const override
    {
        return m_readForward;
    }

    bool ReadVideoFrame(double pos, ImGui::ImMat& m, bool& eof, bool wait) override
    {
        if (!m_started)
        {
            m_errMsg = "This 'MediaReader' instance is NOT STARTED yet!";
            return false;
        }
        if (pos < 0 || pos > m_vidDurTs)
        {
            m_errMsg = "Invalid argument! 'pos' can NOT be negative or larger than video's duration.";
            eof = true;
            return false;
        }
        while (!m_quitThread && !m_prepared)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_close)
        {
            m_errMsg = "This 'MediaReader' instance is CLOSED!";
            return false;
        }
        lock_guard<recursive_mutex> lk(m_apiLock);
        eof = false;
        if (pos == m_prevReadPos && !m_prevReadImg.empty())
        {
            m = m_prevReadImg;
            return true;
        }
        if (IsSuspended() && !m_isImage)
        {
            m_errMsg = "This 'MediaReader' instance is SUSPENDED!";
            return false;
        }

        bool success = ReadVideoFrame_Internal(pos, m, wait);
        if (success)
        {
            m_prevReadPos = pos;
            m_prevReadImg = m;
        }

        if (m_seekPosUpdated)
        {
            lock_guard<mutex> lk(m_seekPosLock);
            UpdateCacheWindow(m_seekPosTs);
            m_seekPosUpdated = false;
        }

        return success;
    }

    bool ReadAudioSamples(uint8_t* buf, uint32_t& size, double& pos, bool& eof, bool wait) override
    {
        if (!m_started)
        {
            m_errMsg = "This 'MediaReader' instance is NOT STARTED yet!";
            return false;
        }
        while (!m_quitThread && !m_prepared)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_close)
        {
            m_errMsg = "This 'MediaReader' instance is closed!";
            return false;
        }
        lock_guard<recursive_mutex> lk(m_apiLock);
        eof = false;

        if (m_audReadEof)
        {
            size = 0;
            eof = true;
            pos = m_cacheWnd.readPos;
            return true;
        }

        bool success = ReadAudioSamples_Internal(buf, size, pos, wait);
        double readDur = m_swrPassThrough ?
                (double)size/m_audFrmSize/m_swrOutSampleRate :
                (double)size/m_swrFrmSize/m_swrOutSampleRate;
        double nextReadPos = pos+(m_readForward ? readDur : -readDur);
        if (nextReadPos < 0)
        {
            m_audReadEof = true;
            nextReadPos = 0;
        }
        else if (nextReadPos > m_audDurTs)
        {
            m_audReadEof = true;
            nextReadPos = m_audDurTs;
        }
        UpdateCacheWindow(nextReadPos);
        eof = m_audReadEof;
        if (eof && size == 0)
            pos = m_readForward ? m_audDurTs : 0;

        return success;
    }

    uint32_t Id() const override
    {
        return m_id;
    }

    bool IsOpened() const override
    {
        return m_opened;
    }

    bool IsStarted() const override
    {
        return m_started;
    }

    bool IsVideoReader() const override
    {
        return m_isVideoReader;
    }

    bool SetCacheDuration(double forwardDur, double backwardDur) override
    {
        if (forwardDur < 0 || backwardDur < 0)
        {
            m_errMsg = "Argument 'forwardDur' and 'backwardDur' must be positive!";
            return false;
        }
        lock_guard<recursive_mutex> lk(m_apiLock);
        m_forwardCacheDur = forwardDur;
        m_backwardCacheDur = backwardDur;
        if (m_prepared)
        {
            UpdateCacheWindow(m_cacheWnd.readPos, true);
            ResetBuildTask();
        }
        return true;
    }

    pair<double, double> GetCacheDuration() const override
    {
        return { m_forwardCacheDur, m_backwardCacheDur };
    }

    MediaInfo::InfoHolder GetMediaInfo() const override
    {
        return m_hMediaInfo;
    }

    const MediaInfo::VideoStream* GetVideoStream() const override
    {
        MediaInfo::InfoHolder hInfo = m_hMediaInfo;
        if (!hInfo || m_vidStmIdx < 0)
            return nullptr;
        return dynamic_cast<MediaInfo::VideoStream*>(hInfo->streams[m_vidStmIdx].get());
    }

    const MediaInfo::AudioStream* GetAudioStream() const override
    {
        MediaInfo::InfoHolder hInfo = m_hMediaInfo;
        if (!hInfo || m_audStmIdx < 0)
            return nullptr;
        return dynamic_cast<MediaInfo::AudioStream*>(hInfo->streams[m_audStmIdx].get());
    }

    uint32_t GetVideoOutWidth() const override
    {
        const MediaInfo::VideoStream* vidStream = GetVideoStream();
        if (!vidStream)
            return 0;
        uint32_t w = m_frmCvt.GetOutWidth();
        if (w > 0)
            return w;
        w = vidStream->width;
        return w;
    }

    uint32_t GetVideoOutHeight() const override
    {
        const MediaInfo::VideoStream* vidStream = GetVideoStream();
        if (!vidStream)
            return 0;
        uint32_t h = m_frmCvt.GetOutHeight();
        if (h > 0)
            return h;
        h = vidStream->height;
        return h;
    }

    uint32_t GetAudioOutChannels() const override
    {
        const MediaInfo::AudioStream* audStream = GetAudioStream();
        if (!audStream)
            return 0;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        return m_swrOutChannels;
#else
        return m_swrOutChlyt.nb_channels;
#endif
    }

    uint32_t GetAudioOutSampleRate() const override
    {
        const MediaInfo::AudioStream* audStream = GetAudioStream();
        if (!audStream)
            return 0;
        return m_swrOutSampleRate;
    }

    uint32_t GetAudioOutFrameSize() const override
    {
        const MediaInfo::AudioStream* audStream = GetAudioStream();
        if (!audStream)
            return 0;
        uint32_t frameSize = m_swrPassThrough ? m_audFrmSize : m_swrFrmSize;
        if (frameSize > 0 || !m_started)
            return frameSize;

        while (!m_quitThread && !m_prepared)
            this_thread::sleep_for(chrono::milliseconds(5));
        frameSize = m_swrPassThrough ? m_audFrmSize : m_swrFrmSize;
        return frameSize;
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
        return pixfmt == m_vidHwPixFmt;
    }

private:
    string FFapiFailureMessage(const string& apiName, int fferr)
    {
        ostringstream oss;
        oss << "FF api '" << apiName << "' returns error! fferr=" << fferr << ".";
        return oss.str();
    }

    int64_t CvtVidPtsToMts(int64_t pts)
    {
        return av_rescale_q(pts-m_vidStartTime, m_vidTimeBase, MILLISEC_TIMEBASE);
    }

    int64_t CvtVidMtsToPts(int64_t mts)
    {
        return av_rescale_q(mts, MILLISEC_TIMEBASE, m_vidTimeBase)+m_vidStartTime;
    }

    int64_t CvtAudPtsToMts(int64_t pts)
    {
        return av_rescale_q(pts-m_audAvStm->start_time, m_audAvStm->time_base, MILLISEC_TIMEBASE);
    }

    int64_t CvtAudMtsToPts(int64_t mts)
    {
        return av_rescale_q(mts, MILLISEC_TIMEBASE, m_audAvStm->time_base)+m_audAvStm->start_time;
    }

    int64_t CvtPtsToMts(int64_t pts)
    {
        return m_isVideoReader ? CvtVidPtsToMts(pts) : CvtAudPtsToMts(pts);
    }

    int64_t CvtMtsToPts(int64_t mts)
    {
        return m_isVideoReader ? CvtVidMtsToPts(mts) : CvtAudMtsToPts(mts);
    }

    int64_t CvtSwrPtsToMts(int64_t pts)
    {
        return av_rescale_q(pts-m_swrOutStartTime, m_swrOutTimebase, MILLISEC_TIMEBASE);
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

        m_seekPosTs = 0;
        m_seekPosUpdated = true;

        return true;
    }

    void ReleaseVideoResource()
    {
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
        m_vidAvStm = nullptr;
        m_viddec = nullptr;

        m_prepared = false;
    }

    bool Prepare()
    {
        bool locked = false;
        do {
            locked = m_apiLock.try_lock();
            if (!locked)
                this_thread::sleep_for(chrono::milliseconds(5));
        } while (!locked && !m_quitThread);
        if (m_quitThread)
        {
            m_logger->Log(WARN) << "Abort 'Prepare' procedure! 'm_quitThread' is set!" << endl;
            return false;
        }

        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        int fferr;
        if (!m_streamInfoFound)
        {
            fferr = avformat_find_stream_info(m_avfmtCtx, nullptr);
            if (fferr < 0)
            {
                m_errMsg = FFapiFailureMessage("avformat_find_stream_info", fferr);
                return false;
            }
            m_streamInfoFound = true;
        }

        if (m_isVideoReader)
        {
            if (!m_isImage)
            {
                m_hParser->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);
                m_hSeekPoints = m_hParser->GetVideoSeekPoints();
                if (!m_hSeekPoints)
                {
                    m_errMsg = "FAILED to retrieve video seek points!";
                    m_logger->Log(Error) << m_errMsg << endl;
                    return false;
                }
            }
            else
            {
                vector<int64_t>* seekPoints = new vector<int64_t>(1);
                seekPoints->at(0) = 0;
                m_hSeekPoints = MediaParser::SeekPointsHolder(seekPoints);
            }

            m_vidAvStm = m_avfmtCtx->streams[m_vidStmIdx];
            m_vidStartTime = m_vidAvStm->start_time != AV_NOPTS_VALUE ? m_vidAvStm->start_time : 0;
            m_vidTimeBase = m_vidAvStm->time_base;

            m_viddec = avcodec_find_decoder(m_vidAvStm->codecpar->codec_id);
            if (m_viddec == nullptr)
            {
                ostringstream oss;
                oss << "Can not find video decoder by codec_id " << m_vidAvStm->codecpar->codec_id << "!";
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
        }
        else
        {
            m_audAvStm = m_avfmtCtx->streams[m_audStmIdx];
            if (m_audAvStm->start_time == INT64_MIN)
                m_audAvStm->start_time = 0;
            m_audDurPts = CvtAudMtsToPts((int64_t)(m_audDurTs*1000));
            m_audioTaskPtsGap = CvtAudMtsToPts(1000);  // set 1-second long task duration

            m_auddec = avcodec_find_decoder(m_audAvStm->codecpar->codec_id);
            if (m_auddec == nullptr)
            {
                ostringstream oss;
                oss << "Can not find audio decoder by codec_id " << m_audAvStm->codecpar->codec_id << "!";
                m_errMsg = oss.str();
                return false;
            }

            if (!OpenAudioDecoder())
                return false;
        }

        if (m_seekPosUpdated)
        {
            lock_guard<mutex> lk(m_seekPosLock);
            UpdateCacheWindow(m_seekPosTs, true);
            m_seekPosUpdated = false;
        }
        else
        {
            UpdateCacheWindow(0, true);
        }
        ResetBuildTask();

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
        fferr = avcodec_parameters_to_context(m_viddecCtx, m_vidAvStm->codecpar);
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
        int fferr;
        AVHWDeviceType hwDevType = AV_HWDEVICE_TYPE_NONE;
        AVCodecContext* hwDecCtx = nullptr;
        AVBufferRef* devCtx;
        for (int i = 0; ; i++)
        {
            const AVCodecHWConfig* config = avcodec_get_hw_config(m_viddec, i);
            if (!config)
            {
                m_vidHwPixFmt = AV_PIX_FMT_NONE;
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
                    hwDevType = config->device_type;
                    hwDecCtx = avcodec_alloc_context3(m_viddec);
                    if (!hwDecCtx)
                        continue;
                    hwDecCtx->opaque = this;
                    fferr = avcodec_parameters_to_context(hwDecCtx, m_vidAvStm->codecpar);
                    if (fferr < 0)
                    {
                        avcodec_free_context(&hwDecCtx);
                        continue;
                    }
                    hwDecCtx->get_format = get_hw_format;
                    devCtx = nullptr;
                    fferr = av_hwdevice_ctx_create(&devCtx, hwDevType, nullptr, nullptr, 0);
                    if (fferr < 0)
                    {
                        avcodec_free_context(&hwDecCtx);
                        if (devCtx) av_buffer_unref(&devCtx);
                        continue;
                    }
                    hwDecCtx->hw_device_ctx = av_buffer_ref(devCtx);
                    fferr = avcodec_open2(hwDecCtx, m_viddec, nullptr);
                    if (fferr < 0)
                    {
                        avcodec_free_context(&hwDecCtx);
                        av_buffer_unref(&devCtx);
                        continue;
                    }
                    break;
                }
            }
        }

        m_viddecDevType = hwDevType;
        m_viddecCtx = hwDecCtx;
        m_viddecHwDevCtx = devCtx;
        m_logger->Log(DEBUG) << "Use hardware device type '" << av_hwdevice_get_type_name(m_viddecDevType) << "'." << endl;
        m_logger->Log(DEBUG) << "Video decoder(HW) '" << m_viddecCtx->codec->name << "' opened." << endl;
        return true;
    }

    bool OpenAudioDecoder()
    {
        m_auddecCtx = avcodec_alloc_context3(m_auddec);
        if (!m_auddecCtx)
        {
            m_errMsg = "FAILED to allocate new AVCodecContext!";
            return false;
        }

        int fferr;
        fferr = avcodec_parameters_to_context(m_auddecCtx, m_audAvStm->codecpar);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avcodec_parameters_to_context", fferr);
            return false;
        }

        fferr = avcodec_open2(m_auddecCtx, m_auddec, nullptr);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avcodec_open2", fferr);
            return false;
        }
        m_logger->Log(DEBUG) << "Audio decoder '" << m_auddec->name << "' opened." << endl;

        // setup sw resampler
        AVSampleFormat inSmpfmt = (AVSampleFormat)m_audAvStm->codecpar->format;
        int inSampleRate = m_audAvStm->codecpar->sample_rate;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        int inChannels = m_audAvStm->codecpar->channels;
        uint64_t inChnLyt = m_audAvStm->codecpar->channel_layout;
        m_swrOutChnLyt = av_get_default_channel_layout(m_swrOutChannels);
        if (inChnLyt <= 0)
            inChnLyt = av_get_default_channel_layout(inChannels);
        if (m_swrOutChnLyt != inChnLyt || m_swrOutSmpfmt != inSmpfmt || m_swrOutSampleRate != inSampleRate)
        {
            m_swrCtx = swr_alloc_set_opts(NULL, m_swrOutChnLyt, m_swrOutSmpfmt, m_swrOutSampleRate, inChnLyt, inSmpfmt, inSampleRate, 0, nullptr);
            if (!m_swrCtx)
#else
        auto& inChlyt = m_audAvStm->codecpar->ch_layout;
        if (av_channel_layout_compare(&m_swrOutChlyt, &inChlyt) || m_swrOutSmpfmt != inSmpfmt || m_swrOutSampleRate != inSampleRate)
        {
            fferr = swr_alloc_set_opts2(&m_swrCtx, &m_swrOutChlyt, m_swrOutSmpfmt, m_swrOutSampleRate, &inChlyt, inSmpfmt, inSampleRate, 0, nullptr);
            if (fferr < 0)
#endif
            {
                m_errMsg = "FAILED to invoke 'swr_alloc_set_opts()' to create 'SwrContext'!";
                return false;
            }
            int fferr;
            fferr = swr_init(m_swrCtx);
            if (fferr < 0)
            {
                m_errMsg = FFapiFailureMessage("swr_init", fferr);
                return false;
            }
            m_swrOutTimebase = { 1, m_swrOutSampleRate };
            m_swrOutStartTime = av_rescale_q(m_audAvStm->start_time, m_audAvStm->time_base, m_swrOutTimebase);
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
            m_swrFrmSize = av_get_bytes_per_sample(m_swrOutSmpfmt)*m_swrOutChannels;
#else
            m_swrFrmSize = av_get_bytes_per_sample(m_swrOutSmpfmt)*m_swrOutChlyt.nb_channels;
#endif
            m_swrPassThrough = false;
        }
        else
        {
            m_swrPassThrough = true;
        }
        return true;
    }

    void StartAllThreads()
    {
        m_quitThread = false;
        m_demuxThread = thread(&MediaReader_Impl::DemuxThreadProc, this);
        if (m_isVideoReader)
        {
            m_viddecThread = thread(&MediaReader_Impl::VideoDecodeThreadProc, this);
            m_updateCfThread = thread(&MediaReader_Impl::GenerateVideoFrameThreadProc, this);
        }
        else
        {
            m_auddecThread = thread(&MediaReader_Impl::AudioDecodeThreadProc, this);
            m_swrThread = thread(&MediaReader_Impl::GenerateAudioSamplesThreadProc, this);
        }
        if (m_isImage)
        {
            m_releaseThread = thread(&MediaReader_Impl::ReleaseResourceProc, this);
        }
    }

    void WaitAllThreadsQuit(bool callFromReleaseProc = false)
    {
        m_quitThread = true;
        if (!callFromReleaseProc && m_releaseThread.joinable())
        {
            m_releaseThread.join();
            m_releaseThread = thread();
        }
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
        if (m_updateCfThread.joinable())
        {
            m_updateCfThread.join();
            m_updateCfThread = thread();
        }
        if (m_auddecThread.joinable())
        {
            m_auddecThread.join();
            m_auddecThread = thread();
        }
        if (m_swrThread.joinable())
        {
            m_swrThread.join();
            m_swrThread = thread();
        }
    }

    void FlushAllQueues()
    {
        m_bldtskPriOrder.clear();
        m_bldtskTimeOrder.clear();
    }

    bool ReadVideoFrame_Internal(double ts, ImGui::ImMat& m, bool wait)
    {
        UpdateCacheWindow(ts);

        GopDecodeTaskHolder targetTask;
        do
        {
            {
                lock_guard<mutex> lk(m_bldtskByTimeLock);
                auto iter = find_if(m_bldtskTimeOrder.begin(), m_bldtskTimeOrder.end(), [this, ts](const GopDecodeTaskHolder& task) {
                    int64_t pts = CvtMtsToPts(ts*1000);
                    return task->seekPts.first <= pts && task->seekPts.second > pts;
                });
                if (iter != m_bldtskTimeOrder.end())
                {
                    targetTask = *iter;
                    break;
                }
                else if (!wait)
                    break;
            }
            this_thread::sleep_for(chrono::milliseconds(5));
        } while (!m_close);
        if (!targetTask)
        {
            m.time_stamp = ts;
            return true;
        }
        if (targetTask->demuxEof && targetTask->frmPtsAry.empty())
        {
            ostringstream oss;
            oss << "Current task [" << targetTask->seekPts.first << "(" << MillisecToString(CvtPtsToMts(targetTask->seekPts.first)) << "), "
                << targetTask->seekPts.second << "(" << MillisecToString(CvtPtsToMts(targetTask->seekPts.second)) << ")) has NO FRM PTS!";
            m_errMsg = oss.str();
            return false;
        }
        if (targetTask->vfAry.empty() && !wait)
        {
            m.time_stamp = ts;
            return true;
        }

        bool foundBestFrame = false;
        list<VideoFrame>::iterator bestfrmIter;
        do
        {
            if (!targetTask->vfAry.empty())
            {
                bestfrmIter= find_if(targetTask->vfAry.begin(), targetTask->vfAry.end(), [ts](const VideoFrame& frm) {
                    return frm.ts > ts;
                });
                bool lastFrmInAry = false;
                if (bestfrmIter == targetTask->vfAry.end())
                    lastFrmInAry = true;
                if (bestfrmIter != targetTask->vfAry.begin())
                    bestfrmIter--;
                if (!lastFrmInAry || targetTask->decodeEof)
                {
                    foundBestFrame = true;
                    break;
                }
                else if (ts-bestfrmIter->ts <= m_vidfrmIntvMts/1000)
                {
                    foundBestFrame = true;
                    break;
                }
                else if (!wait)
                    break;
            }
            this_thread::sleep_for(chrono::milliseconds(5));
        } while (!m_close);

        if (foundBestFrame)
        {
            if (wait)
                while(!m_close && !bestfrmIter->ownfrm)
                    this_thread::sleep_for(chrono::milliseconds(5));
            if (bestfrmIter->ownfrm)
            {
                if (!m_frmCvt.ConvertImage(bestfrmIter->ownfrm.get(), m, bestfrmIter->ts))
                    m_logger->Log(Error) << "FAILED to convert AVFrame to ImGui::ImMat! Message is '" << m_frmCvt.GetError() << "'." << endl;
            }
            else
                m.time_stamp = bestfrmIter->ts;
        }
        else
            m.time_stamp = ts;

        return true;
    }

    bool ReadAudioSamples_Internal(uint8_t* buf, uint32_t& size, double& pos, bool wait)
    {
        uint8_t* dstptr = buf;
        const uint32_t outChannels = GetAudioOutChannels();
        uint32_t readSize = 0, toReadSize = IsPlanar() ? size/outChannels : size;
        uint32_t skipSize;
        skipSize = m_audReadOffset > 0 ? m_audReadOffset : 0;
        unique_ptr<list<AudioFrame>::iterator> fwditerPtr;
        unique_ptr<list<AudioFrame>::reverse_iterator> bwditerPtr;
        bool isIterSet = false;
        bool isPosSet = false;
        bool needLoop;
        do
        {
            bool idleLoop = true;

            GopDecodeTaskHolder readTask = m_audReadTask;
            if (!readTask)
            {
                readTask = FindNextAudioReadTask();
                skipSize = 0;
                fwditerPtr = nullptr;
                bwditerPtr = nullptr;
                isIterSet = false;
            }

            if (readTask)
            {
                if (!readTask->afAry.empty() && (m_readForward || readTask->afAry.back().endOfGop))
                {
                    auto& afAry = readTask->afAry;
                    if (!isIterSet)
                    {
                        if (m_readForward)
                            fwditerPtr = unique_ptr<list<AudioFrame>::iterator>(new list<AudioFrame>::iterator(afAry.begin()));
                        else
                            bwditerPtr = unique_ptr<list<AudioFrame>::reverse_iterator>(new list<AudioFrame>::reverse_iterator(afAry.rbegin()));
                        isIterSet = true;
                    }

                    auto& fwditer = *fwditerPtr.get();
                    auto& bwditer = *bwditerPtr.get();
                    SelfFreeAVFramePtr readfrm = m_readForward ? fwditer->fwdfrm : bwditer->bwdfrm;
                    if (readfrm)
                    {
                        if (!isPosSet)
                        {
                            double startts = m_swrPassThrough ?
                                    (double)CvtPtsToMts(readfrm->pts)/1000 :
                                    (double)CvtSwrPtsToMts(readfrm->pts)/1000;
                            double offset = m_swrPassThrough ?
                                    (double)m_audReadOffset/m_audFrmSize/m_swrOutSampleRate :
                                    (double)m_audReadOffset/m_swrFrmSize/m_swrOutSampleRate;
                            pos = m_readForward ? startts+offset : startts-offset;
                            m_prevReadPos = pos;
                            isPosSet = true;
                        }
                        uint32_t dataSizePerPlan = readfrm->nb_samples*GetAudioOutFrameSize();
                        if (IsPlanar()) dataSizePerPlan /= outChannels;
                        if (skipSize >= dataSizePerPlan)
                        {
                            bool reachEnd;
                            if (m_readForward)
                            {
                                fwditer++;
                                reachEnd = fwditer == afAry.end();
                            }
                            else
                            {
                                bwditer++;
                                reachEnd = bwditer == afAry.rend();
                            }
                            if (!reachEnd)
                            {
                                skipSize -= dataSizePerPlan;
                                idleLoop = false;
                            }
                            else
                            {
                                readTask = FindNextAudioReadTask();
                                skipSize = 0;
                                fwditerPtr = nullptr;
                                bwditerPtr = nullptr;
                                isIterSet = false;
                            }
                        }
                        else
                        {
                            uint32_t copySize = dataSizePerPlan-skipSize;
                            bool moveToNext = true;
                            if (copySize > toReadSize-readSize)
                            {
                                copySize = toReadSize-readSize;
                                moveToNext = false;
                            }
                            if (IsPlanar())
                            {
                                uint8_t* planBufPtr = dstptr+readSize;
                                for (uint32_t i = 0; i < outChannels; i++)
                                {
                                    memcpy(planBufPtr, readfrm->data[i]+skipSize, copySize);
                                    planBufPtr += toReadSize;
                                }
                            }
                            else
                            {
                                memcpy(dstptr+readSize, readfrm->data[0]+skipSize, copySize);
                            }
                            readSize += copySize;
                            skipSize = 0;
                            m_audReadOffset += copySize;

                            if (moveToNext)
                            {
                                bool reachEnd;
                                if (m_readForward)
                                {
                                    fwditer++;
                                    reachEnd = fwditer == afAry.end();
                                }
                                else
                                {
                                    bwditer++;
                                    reachEnd = bwditer == afAry.rend();
                                }
                                if (reachEnd)
                                {
                                    readTask = FindNextAudioReadTask();
                                    skipSize = 0;
                                    fwditerPtr = nullptr;
                                    bwditerPtr = nullptr;
                                    isIterSet = false;
                                }
                            }
                            idleLoop = false;
                        }
                    }
                }
                else if (readTask->afAry.empty() && readTask->decInputEof)
                {
                    m_logger->Log(WARN) << "Audio read task has its 'decInputEof' already set to 'true', but the 'afAry' is still empty. Skip this task." << endl;
                    readTask = FindNextAudioReadTask();
                    skipSize = 0;
                    fwditerPtr = nullptr;
                    bwditerPtr = nullptr;
                    isIterSet = false;
                }
            }

            needLoop = ((readTask && !readTask->cancel) || (!readTask && wait) || !idleLoop) && toReadSize > readSize && !m_audReadEof && !m_close;
            if (needLoop && idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        } while (needLoop);
        size = IsPlanar() ? readSize*outChannels : readSize;
        return true;
    }

    struct VideoFrame
    {
        SelfFreeAVFramePtr decfrm;
        SelfFreeAVFramePtr ownfrm;
        double ts;
    };

    struct AudioFrame
    {
        SelfFreeAVFramePtr decfrm;
        SelfFreeAVFramePtr fwdfrm;
        SelfFreeAVFramePtr bwdfrm;
        double ts;
        int64_t pts;
        bool endOfGop{false};
        // int64_t frmDur;
    };

    struct CacheWindow
    {
        double readPos;
        double cacheBeginTs;
        double cacheEndTs;
        int64_t seekPosShow;
        int64_t seekPos00;
        int64_t seekPos10;
    };

    struct GopDecodeTask
    {
        GopDecodeTask(MediaReader_Impl& obj) : outterObj(obj) {}

        ~GopDecodeTask()
        {
            for (AVPacket* avpkt : avpktQ)
                av_packet_free(&avpkt);
            for (VideoFrame& vf : vfAry)
                if (vf.decfrm)
                    outterObj.m_pendingVidfrmCnt--;
            vfAry.clear();
        }

        MediaReader_Impl& outterObj;
        pair<int64_t, int64_t> seekPts;
        list<VideoFrame> vfAry;
        list<AudioFrame> afAry;
        atomic_int32_t frmCnt{0};
        list<AVPacket*> avpktQ;
        list<int64_t> frmPtsAry;
        mutex avpktQLock;
        bool demuxing{false};
        bool demuxEof{false};
        bool mediaBegin{false};
        bool mediaEnd{false};
        bool decoding{false};
        bool decInputEof{false};
        bool decodeEof{false};
        bool cancel{false};
    };
    using GopDecodeTaskHolder = shared_ptr<GopDecodeTask>;

    GopDecodeTaskHolder FindNextDemuxTask()
    {
        GopDecodeTaskHolder nxttsk = nullptr;
        uint32_t pendingTaskCnt = 0;
        for (auto& tsk : m_bldtskPriOrder)
            if (!tsk->cancel && !tsk->demuxing)
            {
                nxttsk = tsk;
                break;
            }
        return nxttsk;
    }

    void DemuxThreadProc()
    {
        m_logger->Log(DEBUG) << "Enter DemuxThreadProc()..." << endl;

        if (!m_prepared && !Prepare())
        {
            m_logger->Log(Error) << "Prepare() FAILED! Error is '" << m_errMsg << "'." << endl;
            return;
        }

        AVPacket avpkt = {0};
        bool avpktLoaded = false;
        GopDecodeTaskHolder currTask = nullptr;
        int64_t lastPktPts, lastTaskSeekPts1;
        lastTaskSeekPts1 = INT64_MIN;
        bool demuxEof = false;
        int stmidx = m_isVideoReader ? m_vidStmIdx : m_audStmIdx;
        while (!m_quitThread)
        {
            bool idleLoop = true;

            UpdateBuildTask();

            bool taskChanged = false;
            if (!currTask || currTask->cancel || currTask->demuxEof)
            {
                if (currTask)
                {
                    lastTaskSeekPts1 = currTask->seekPts.second;
                    if (currTask->cancel)
                    {
                        m_logger->Log(DEBUG) << "~~~~ Old demux task canceled, startPts=" 
                            << currTask->seekPts.first << "(" << MillisecToString(CvtPtsToMts(currTask->seekPts.first)) << ")"
                            << ", endPts=" << currTask->seekPts.second << "(" << MillisecToString(CvtPtsToMts(currTask->seekPts.second)) << ")" << endl;
                    }
                }
                currTask = FindNextDemuxTask();
                if (currTask)
                {
                    currTask->demuxing = true;
                    taskChanged = true;
                    m_logger->Log(DEBUG) << "--> Change demux task, startPts=" 
                        << currTask->seekPts.first << "(" << MillisecToString(CvtPtsToMts(currTask->seekPts.first)) << ")"
                        << ", endPts=" << currTask->seekPts.second << "(" << MillisecToString(CvtPtsToMts(currTask->seekPts.second)) << ")" << endl;
                }
            }

            if (currTask)
            {
                if (taskChanged)
                {
                    if (!avpktLoaded || lastTaskSeekPts1 != currTask->seekPts.first || avpkt.pts < currTask->seekPts.first)
                    {
                        if (avpktLoaded)
                        {
                            av_packet_unref(&avpkt);
                            avpktLoaded = false;
                        }
                        lastPktPts = INT64_MIN;
                        int fferr = 0;
                        if (!m_isImage)
                        {
                            int fferr = avformat_seek_file(m_avfmtCtx, stmidx, INT64_MIN, currTask->seekPts.first, currTask->seekPts.first, 0);
                            if (fferr < 0)
                            {
                                m_logger->Log(Error) << "avformat_seek_file() FAILED for seeking to 'currTask->startPts'(" << currTask->seekPts.first << ")! fferr = " << fferr << "!" << endl;
                                break;
                            }
                        }
                        demuxEof = false;
                        int64_t ptsAfterSeek = INT64_MIN;
                        if (!ReadNextStreamPacket(stmidx, &avpkt, &avpktLoaded, &ptsAfterSeek))
                            break;
                        if (ptsAfterSeek == INT64_MAX)
                            demuxEof = true;
                        else if ((m_isVideoReader && ptsAfterSeek == m_vidAvStm->start_time) ||
                                 (!m_isVideoReader && ptsAfterSeek == m_audAvStm->start_time))
                            currTask->mediaBegin = true;
                        else if (ptsAfterSeek != currTask->seekPts.first)
                        {
                            // m_logger->Log(WARN) << "WARNING! 'ptsAfterSeek'(" << ptsAfterSeek << ") != 'ssTask->startPts'(" << currTask->seekPts.first << ")!" << endl;
                            // currTask->seekPts.first = ptsAfterSeek;
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
                            currTask->mediaEnd = true;
                            currTask->demuxEof = true;
                            if (taskChanged)
                            {
                                m_logger->Log(WARN) << "First AVPacket is EOF for this task! This task is INVALID." << endl;
                                currTask->cancel = true;
                            }
                            demuxEof = true;
                            // cancel all the following tasks if there is any
                            {
                                lock_guard<mutex> lk(m_bldtskByPriLock);
                                for (auto& task : m_bldtskPriOrder)
                                {
                                    if (task->seekPts.first > currTask->seekPts.first)
                                    {
                                        m_logger->Log(DEBUG) << "CANCEL invalid task after demux EOF, seekPts.first=" << task->seekPts.first << "." << endl;
                                        task->cancel = true;
                                    }
                                }
                            }
                        }
                        else
                        {
                            m_errMsg = FFapiFailureMessage("av_read_frame", fferr);
                            m_logger->Log(Error) << "Demuxer ERROR! 'av_read_frame' returns " << fferr << "." << endl;
                        }
                    }
                }

                if (avpktLoaded)
                {
                    if (avpkt.stream_index == stmidx)
                    {
                        if (avpkt.pts >= currTask->seekPts.second)
                            currTask->demuxEof = true;

                        if (!currTask->demuxEof)
                        {
                            AVPacket* enqpkt = av_packet_clone(&avpkt);
                            if (!enqpkt)
                            {
                                m_logger->Log(Error) << "FAILED to invoke 'av_packet_clone(DemuxThreadProc)'!" << endl;
                                break;
                            }
                            {
                                lock_guard<mutex> lk(currTask->avpktQLock);
                                // m_logger->Log(DEBUG) << "-> Queuing AVPacket of stream#" << stmidx << ", pts=" << enqpkt->pts << "." << endl;
                                currTask->avpktQ.push_back(enqpkt);
                                if (lastPktPts != enqpkt->pts)
                                {
                                    currTask->frmPtsAry.push_back(enqpkt->pts);
                                    lastPktPts = enqpkt->pts;
                                }
                                if (enqpkt->pts < currTask->seekPts.first)
                                {
                                    m_logger->Log(DEBUG) << "-=-> Change current task 'seekPts.first' from " << currTask->seekPts.first << " to " << enqpkt->pts << endl;
                                    currTask->seekPts.first = enqpkt->pts;
                                }
                                else if (enqpkt->pts > currTask->seekPts.second)
                                {
                                    m_logger->Log(DEBUG) << "-=-> Change current task 'seekPts.second' from " << currTask->seekPts.second << " to " << enqpkt->pts << endl;
                                    currTask->seekPts.second = enqpkt->pts;
                                }
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

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }

        if (currTask && !currTask->demuxEof)
            currTask->demuxEof = true;
        if (avpktLoaded)
            av_packet_unref(&avpkt);
        m_logger->Log(DEBUG) << "Leave DemuxThreadProc()." << endl;
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
        } while (fferr >= 0 && !m_quitThread);
        if (m_quitThread)
            return false;
        return true;
    }

    GopDecodeTaskHolder FindNextDecoderTask()
    {
        lock_guard<mutex> lk(m_bldtskByPriLock);
        GopDecodeTaskHolder nxttsk = nullptr;
        for (auto& tsk : m_bldtskPriOrder)
            if (!tsk->cancel && tsk->demuxing && !tsk->decoding)
            {
                nxttsk = tsk;
                break;
            }
        return nxttsk;
    }

    bool EnqueueSnapshotAVFrame(AVFrame* frm)
    {
        double ts = (double)CvtPtsToMts(frm->pts)/1000;
        lock_guard<mutex> lk(m_bldtskByPriLock);
        auto iter = find_if(m_bldtskPriOrder.begin(), m_bldtskPriOrder.end(), [frm](const GopDecodeTaskHolder& task) {
            return frm->pts >= task->seekPts.first && frm->pts < task->seekPts.second;
        });
        if (iter != m_bldtskPriOrder.end())
        {
            VideoFrame vf;
            vf.ts = ts;
            vf.decfrm = CloneSelfFreeAVFramePtr(frm);
            if (!vf.decfrm)
            {
                m_logger->Log(Error) << "FAILED to invoke 'CloneSelfFreeAVFramePtr()' to allocate new AVFrame for VF!" << endl;
                return false;
            }
            // m_logger->Log(DEBUG) << "Adding VF#" << ts << "." << endl;
            auto& task = *iter;
            if (task->vfAry.empty())
            {
                task->vfAry.push_back(vf);
                task->frmCnt++;
                m_pendingVidfrmCnt++;
            }
            else
            {
                auto vfRvsIter = find_if(task->vfAry.rbegin(), task->vfAry.rend(), [ts](const VideoFrame& vf) {
                    return vf.ts <= ts;
                });
                if (vfRvsIter != task->vfAry.rend() && vfRvsIter->ts == ts)
                {
                    m_logger->Log(DEBUG) << "Found duplicated VF#" << ts << ", dropping this VF. pts=" << frm->pts
                        << ", t=" << MillisecToString(CvtPtsToMts(frm->pts)) << "." << endl;
                }
                else
                {
                    auto vfFwdIter = vfRvsIter.base();
                    task->vfAry.insert(vfFwdIter, vf);
                    task->frmCnt++;
                    m_pendingVidfrmCnt++;
                }
            }
            if (task->vfAry.size() >= task->frmPtsAry.size() || frm->pts >= task->frmPtsAry.back())
            {
                // m_logger->Log(DEBUG) << "Task [" << task->seekPts.first << "(" << MillisecToString(CvtPtsToMts(task->seekPts.first)) << "), "
                // << task->seekPts.second << "(" << MillisecToString(CvtPtsToMts(task->seekPts.second)) << ")) finishes ALL FRAME decoding." << endl;
                task->decodeEof = true;
            }
            return true;
        }
        else
        {
            m_logger->Log(DEBUG) << "Dropping VF#" << ts << " due to no matching task is found." << endl;
        }
        return false;
    }

    void VideoDecodeThreadProc()
    {
        m_logger->Log(DEBUG) << "Enter VideoDecodeThreadProc()..." << endl;

        while (!m_prepared && !m_quitThread)
            this_thread::sleep_for(chrono::milliseconds(5));

        GopDecodeTaskHolder currTask;
        AVFrame avfrm = {0};
        bool avfrmLoaded = false;
        bool needResetDecoder = false;
        bool sentNullPacket = false;
        while (!m_quitThread)
        {
            bool idleLoop = true;
            bool quitLoop = false;

            if (!currTask || currTask->cancel || currTask->decInputEof)
            {
                GopDecodeTaskHolder oldTask = currTask;
                currTask = FindNextDecoderTask();
                if (currTask)
                {
                    currTask->decoding = true;
                    m_logger->Log(DEBUG) << "==> Change decoding task, startPts="
                        << currTask->seekPts.first << "(" << MillisecToString(CvtPtsToMts(currTask->seekPts.first)) << ")"
                        << ", endPts=" << currTask->seekPts.second << "(" << MillisecToString(CvtPtsToMts(currTask->seekPts.second)) << ")" << endl;
                }
                if (oldTask)
                {
                    if (oldTask->cancel && avfrmLoaded)
                    {
                        m_logger->Log(DEBUG) << "~~~~ Old video task canceled, startPts="
                            << oldTask->seekPts.first << "(" << MillisecToString(CvtPtsToMts(oldTask->seekPts.first)) << ")"
                            << ", endPts=" << oldTask->seekPts.second << "(" << MillisecToString(CvtPtsToMts(oldTask->seekPts.second)) << ")" << endl;
                        av_frame_unref(&avfrm);
                        avfrmLoaded = false;
                    }
                    if (oldTask->cancel || !currTask || oldTask->seekPts.second != currTask->seekPts.first)
                    {
                        // m_logger->Log(DEBUG) << ">>>--->>> Sending NULL ptr to video decoder <<<---<<<" << endl;
                        avcodec_send_packet(m_viddecCtx, nullptr);
                        sentNullPacket = true;
                    }
                }
            }

            if (needResetDecoder)
            {
                avcodec_flush_buffers(m_viddecCtx);
                needResetDecoder = false;
                sentNullPacket = false;
            }

            // retrieve output frame
            bool hasOutput;
            do{
                if (!avfrmLoaded)
                {
                    int fferr = avcodec_receive_frame(m_viddecCtx, &avfrm);
                    if (fferr == 0)
                    {
                        // m_logger->Log(DEBUG) << "<<< Get video frame pts=" << avfrm.pts << "(" << MillisecToString(CvtPtsToMts(avfrm.pts)) << ")." << endl;
                        avfrmLoaded = true;
                        idleLoop = false;
                    }
                    else if (fferr != AVERROR(EAGAIN))
                    {
                        if (fferr != AVERROR_EOF)
                        {
                            m_errMsg = FFapiFailureMessage("avcodec_receive_frame", fferr);
                            m_logger->Log(Error) << "FAILED to invoke 'avcodec_receive_frame'(VideoDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
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
                    // else
                    // {
                    //     m_logger->Log(DEBUG) << "<<<<<<--- avcodec_receive_frame() returns EAGAIN." << endl;
                    // }
                }

                hasOutput = avfrmLoaded;
                if (avfrmLoaded)
                {
                    if (m_pendingVidfrmCnt < m_maxPendingVidfrmCnt)
                    {
                        EnqueueSnapshotAVFrame(&avfrm);
                        av_frame_unref(&avfrm);
                        avfrmLoaded = false;
                        idleLoop = false;
                    }
                    else
                    {
                        this_thread::sleep_for(chrono::milliseconds(5));
                    }
                }
            } while (hasOutput && !m_quitThread && (!currTask || !currTask->cancel));
            if (quitLoop)
                break;
            if (currTask && currTask->cancel)
                continue;

            if (currTask && !sentNullPacket)
            {
                // input packet to decoder
                if (!currTask->avpktQ.empty())
                {
                    AVPacket* avpkt = currTask->avpktQ.front();
                    int fferr = avcodec_send_packet(m_viddecCtx, avpkt);
                    if (fferr == 0)
                    {
                        // m_logger->Log(DEBUG) << ">>> Send video packet pts=" << avpkt->pts << "(" << MillisecToString(CvtPtsToMts(avpkt->pts)) << ")." << endl;
                        {
                            lock_guard<mutex> lk(currTask->avpktQLock);
                            currTask->avpktQ.pop_front();
                        }
                        av_packet_free(&avpkt);
                        idleLoop = false;
                    }
                    else if (fferr == AVERROR_INVALIDDATA)
                    {
                        m_logger->Log(DEBUG) << "(VIDEO)avcodec_send_packet() return AVERROR_INVALIDDATA when decoding AVPacket with pts=" << avpkt->pts
                            << " from file '" << m_hParser->GetUrl() << "'. DISCARD this PACKET." << endl;
                        {
                            lock_guard<mutex> lk(currTask->avpktQLock);
                            currTask->avpktQ.pop_front();
                        }
                        av_packet_free(&avpkt);
                        idleLoop = false;
                    }
                    else if (fferr != AVERROR(EAGAIN))
                    {
                        m_errMsg = FFapiFailureMessage("avcodec_send_packet", fferr);
                        m_logger->Log(Error) << "FAILED to invoke 'avcodec_send_packet'(VideoDecodeThreadProc)! return code is "
                            << fferr << "." << endl;
                        break;
                    }
                    // else
                    // {
                    //     m_logger->Log(DEBUG) << "--->>>>>> avcodec_send_packet() returns EAGAIN." << endl;
                    // }
                }
                else if (currTask->demuxEof)
                {
                    currTask->decInputEof = true;
                    idleLoop = false;
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (currTask && !currTask->decInputEof)
            currTask->decInputEof = true;
        if (avfrmLoaded)
            av_frame_unref(&avfrm);
        m_logger->Log(DEBUG) << "Leave VideoDecodeThreadProc()." << endl;
    }

    GopDecodeTaskHolder FindNextCfUpdateTask()
    {
        lock_guard<mutex> lk(m_bldtskByPriLock);
        GopDecodeTaskHolder nxttsk = nullptr;
        for (auto& tsk : m_bldtskPriOrder)
            if (!tsk->cancel && tsk->frmCnt > 0)
            {
                nxttsk = tsk;
                break;
            }
        return nxttsk;
    }

    void GenerateVideoFrameThreadProc()
    {
        m_logger->Log(DEBUG) << "Enter GenerateVideoFrameThreadProc()..." << endl;

        while (!m_prepared && !m_quitThread)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_quitThread)
            return;

        GopDecodeTaskHolder currTask;
        while (!m_quitThread)
        {
            bool idleLoop = true;

            if (!currTask || currTask->cancel || currTask->frmCnt <= 0)
            {
                currTask = FindNextCfUpdateTask();
            }

            if (currTask)
            {
                for (VideoFrame& vf : currTask->vfAry)
                {
                    if (vf.decfrm)
                    {
                        AVFrame* frm = vf.decfrm.get();
                        if (IsHwFrame(frm))
                        {
                            SelfFreeAVFramePtr swfrm = AllocSelfFreeAVFramePtr();
                            if (swfrm)
                            {
                                if (HwFrameToSwFrame(swfrm.get(), frm))
                                    vf.ownfrm = swfrm;
                                else
                                    m_logger->Log(Error) << "FAILED to convert HW frame to SW frame!" << endl;
                            }
                            else
                                m_logger->Log(Error) << "FAILED to allocate new SelfFreeAVFramePtr!" << endl;
                        }
                        else
                        {
                            vf.ownfrm = vf.decfrm;
                        }
                        vf.decfrm = nullptr;
                        currTask->frmCnt--;
                        if (currTask->frmCnt < 0)
                            m_logger->Log(Error) << "!! ABNORMAL !! Task [" << currTask->seekPts.first << ", " << currTask->seekPts.second << "] has negative 'frmCnt'("
                                << currTask->frmCnt << ")!" << endl;
                        m_pendingVidfrmCnt--;
                        if (m_pendingVidfrmCnt < 0)
                            m_logger->Log(Error) << "Pending video AVFrame ptr count is NEGATIVE! " << m_pendingVidfrmCnt << endl;

                        idleLoop = false;
                    }
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_logger->Log(DEBUG) << "Leave GenerateVideoFrameThreadProc()." << endl;
    }

    bool EnqueueAudioAVFrame(AVFrame* frm)
    {
        double ts = (double)CvtPtsToMts(frm->pts)/1000;
        lock_guard<mutex> lk(m_bldtskByPriLock);
        auto iter = find_if(m_bldtskPriOrder.begin(), m_bldtskPriOrder.end(), [frm](const GopDecodeTaskHolder& task) {
            return frm->pts >= task->seekPts.first && frm->pts < task->seekPts.second;
        });
        if (iter != m_bldtskPriOrder.end())
        {
            AudioFrame af;
            af.ts = ts;
            af.pts = frm->pts;
            af.decfrm = CloneSelfFreeAVFramePtr(frm);
            if (!af.decfrm)
            {
                m_logger->Log(Error) << "FAILED to invoke 'CloneSelfFreeAVFramePtr()' to allocate new AVFrame for AF!" << endl;
                return false;
            }
            int64_t nextPts;
            if (frm->pkt_duration > 0)
                nextPts = frm->pts+frm->pkt_duration;
            else
            {
                int64_t pktDur = CvtMtsToPts((int64_t)((double)frm->nb_samples*1000/frm->sample_rate));
                nextPts = frm->pts+pktDur;
            }
            af.endOfGop = nextPts >= iter->get()->seekPts.second || nextPts >= m_audDurPts;
            // m_logger->Log(DEBUG) << "Adding AF#" << ts << "." << endl;
            auto& task = *iter;
            if (task->afAry.empty())
            {
                task->afAry.push_back(af);
                task->frmCnt++;
            }
            else
            {
                auto afRvsIter = find_if(task->afAry.rbegin(), task->afAry.rend(), [ts](const AudioFrame& af) {
                    return af.ts <= ts;
                });
                if (afRvsIter != task->afAry.rend() && afRvsIter->ts == ts)
                {
                    m_logger->Log(DEBUG) << "Found duplicated AF#" << ts << ", dropping this AF. pts=" << frm->pts
                        << ", t=" << MillisecToString(CvtPtsToMts(frm->pts)) << "." << endl;
                }
                else
                {
                    auto afFwdIter = afRvsIter.base();
                    task->afAry.insert(afFwdIter, af);
                    task->frmCnt++;
                }
            }
            if (task->afAry.size() >= task->frmPtsAry.size())
            {
                // m_logger->Log(DEBUG) << "Task [" << task->seekPts.first << "(" << MillisecToString(CvtPtsToMts(task->seekPts.first)) << "), "
                // << task->seekPts.second << "(" << MillisecToString(CvtPtsToMts(task->seekPts.second)) << ")) finishes ALL FRAME decoding." << endl;
                task->decodeEof = true;
            }
            return true;
        }
        else
        {
            m_logger->Log(DEBUG) << "Dropping AF#" << ts << " due to no matching task is found." << endl;
        }
        return false;
    }

    void AudioDecodeThreadProc()
    {
        m_logger->Log(DEBUG) << "Enter AudioDecodeThreadProc()..." << endl;

        while (!m_prepared && !m_quitThread)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_quitThread)
            return;

        GopDecodeTaskHolder currTask;
        AVFrame avfrm = {0};
        bool avfrmLoaded = false;
        while (!m_quitThread)
        {
            bool idleLoop = true;
            bool quitLoop = false;

            if (currTask && currTask->cancel)
            {
                m_logger->Log(DEBUG) << "~~~~ Current audio task canceled" << endl;
                if (avfrmLoaded)
                {
                    av_frame_unref(&avfrm);
                    avfrmLoaded = false;
                }
                currTask = nullptr;
            }

            if (!currTask || currTask->decInputEof)
            {
                // GopDecodeTaskHolder oldTask = currTask;
                currTask = FindNextDecoderTask();
                if (currTask)
                {
                    currTask->decoding = true;
                    // m_logger->Log(DEBUG) << "==> Change decoding task to build index (" << currTask->ssIdxPair.first << " ~ " << currTask->ssIdxPair.second << ")." << endl;
                }
                // else if (oldTask)
                // {
                //     avcodec_send_packet(m_viddecCtx, nullptr);
                //     sentNullPacket = true;
                // }
            }

            if (currTask)
            {
                // retrieve output frame
                bool hasOutput;
                do{
                    if (!avfrmLoaded)
                    {
                        int fferr = avcodec_receive_frame(m_auddecCtx, &avfrm);
                        if (fferr == 0)
                        {
                            // m_logger->Log(DEBUG) << "<<< Get audio frame pts=" << avfrm.pts << "(" << MillisecToString(CvtPtsToMts(avfrm.pts)) << ")." << endl;
                            avfrmLoaded = true;
                            idleLoop = false;
                        }
                        else if (fferr != AVERROR(EAGAIN))
                        {
                            if (fferr != AVERROR_EOF)
                            {
                                m_errMsg = FFapiFailureMessage("avcodec_receive_frame", fferr);
                                m_logger->Log(Error) << "FAILED to invoke 'avcodec_receive_frame'(AudioDecodeThreadProc)! return code is "
                                    << fferr << "." << endl;
                                quitLoop = true;
                                break;
                            }
                            else
                            {
                                idleLoop = false;
                                // needResetDecoder = true;
                                // m_logger->Log(DEBUG) << "Audio decoder current task reaches EOF!" << endl;
                            }
                        }
                    }

                    hasOutput = avfrmLoaded;
                    if (avfrmLoaded)
                    {
                        EnqueueAudioAVFrame(&avfrm);
                        av_frame_unref(&avfrm);
                        avfrmLoaded = false;
                        idleLoop = false;
                    }
                } while (hasOutput && !m_quitThread);
                if (quitLoop)
                    break;

                // input packet to decoder
                if (!currTask->avpktQ.empty())
                {
                    AVPacket* avpkt = currTask->avpktQ.front();
                    int fferr = avcodec_send_packet(m_auddecCtx, avpkt);
                    if (fferr == 0)
                    {
                        // m_logger->Log(DEBUG) << ">>> Send audio packet pts=" << avpkt->pts << "(" << MillisecToString(CvtPtsToMts(avpkt->pts)) << ")." << endl;
                        {
                            lock_guard<mutex> lk(currTask->avpktQLock);
                            currTask->avpktQ.pop_front();
                        }
                        av_packet_free(&avpkt);
                        idleLoop = false;
                    }
                    else if (fferr != AVERROR(EAGAIN) && fferr != AVERROR_INVALIDDATA)
                    {
                        m_errMsg = FFapiFailureMessage("avcodec_send_packet", fferr);
                        m_logger->Log(Error) << "FAILED to invoke 'avcodec_send_packet'(AudioDecodeThreadProc)! return code is "
                            << fferr << "." << endl;
                        break;
                    }
                }
                else if (currTask->demuxEof)
                {
                    currTask->decInputEof = true;
                    idleLoop = false;
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (avfrmLoaded)
            av_frame_unref(&avfrm);
        m_logger->Log(DEBUG) << "Leave AudioDecodeThreadProc()." << endl;
    }

    void GenerateAudioSamplesThreadProc()
    {
        m_logger->Log(DEBUG) << "Enter GenerateAudioSamplesThreadProc()..." << endl;

        while (!m_prepared && !m_quitThread)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_quitThread)
            return;

        GopDecodeTaskHolder currTask;
        AVRational audTimebase = m_audAvStm->time_base;
        while (!m_quitThread)
        {
            bool idleLoop = true;

            if (!currTask || currTask->cancel || currTask->frmCnt <= 0)
            {
                currTask = FindNextCfUpdateTask();
            }

            if (currTask)
            {
                for (AudioFrame& af : currTask->afAry)
                {
                    int fferr;
                    SelfFreeAVFramePtr fwdfrm;
                    SelfFreeAVFramePtr bwdfrm;
                    if (af.decfrm)
                    {
                        if (m_swrPassThrough)
                        {
                            fwdfrm = af.decfrm;
                        }
                        else
                        {
                            fwdfrm = AllocSelfFreeAVFramePtr();
                            if (!fwdfrm)
                            {
                                m_logger->Log(Error) << "FAILED to allocate new AVFrame for 'swr_convert()'!" << endl;
                                break;
                            }
                            AVFrame* srcfrm = af.decfrm.get();
                            AVFrame* dstfrm = fwdfrm.get();
                            av_frame_copy_props(dstfrm, srcfrm);
                            dstfrm->format = (int)m_swrOutSmpfmt;
                            dstfrm->sample_rate = m_swrOutSampleRate;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
                            dstfrm->channels = m_swrOutChannels;
                            dstfrm->channel_layout = m_swrOutChnLyt;
#else
                            dstfrm->ch_layout = m_swrOutChlyt;
#endif
                            dstfrm->nb_samples = swr_get_out_samples(m_swrCtx, srcfrm->nb_samples);
                            fferr = av_frame_get_buffer(dstfrm, 0);
                            if (fferr < 0)
                            {
                                m_logger->Log(Error) << "av_frame_get_buffer(UpdatePcmThreadProc1) FAILED with return code " << fferr << endl;
                                break;
                            }
                            int64_t outpts = swr_next_pts(m_swrCtx, av_rescale(srcfrm->pts, audTimebase.num*(int64_t)dstfrm->sample_rate*srcfrm->sample_rate, audTimebase.den));
                            dstfrm->pts = ROUNDED_DIV(outpts, srcfrm->sample_rate);
                            fferr = swr_convert(m_swrCtx, dstfrm->data, dstfrm->nb_samples, (const uint8_t **)srcfrm->data, srcfrm->nb_samples);
                            if (fferr < 0)
                            {
                                m_logger->Log(Error) << "swr_convert(GenerateAudioSamplesThreadProc) FAILED with return code " << fferr << endl;
                                break;
                            }
                            if (fferr < dstfrm->nb_samples)
                            {
                                dstfrm->nb_samples = fferr;
                            }
                            af.pts = dstfrm->pts;
                        }
                        if (m_fpPcmFile)
                        {
                            int frameSize = m_swrPassThrough ? m_audFrmSize : m_swrFrmSize;
                            if (IsPlanar())
                            {
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
                                int frmChannels = fwdfrm->channels;
#else
                                int frmChannels = fwdfrm->ch_layout.nb_channels;
#endif
                                int bytesPerSample = frameSize/frmChannels;
                                int offset = 0;
                                for (int i = 0; i < fwdfrm->nb_samples; i++)
                                {
                                    for (int j = 0; j < frmChannels; j++)
                                        fwrite(fwdfrm->data[j]+offset, 1, bytesPerSample, m_fpPcmFile);
                                    offset += bytesPerSample;
                                }
                            }
                            else
                            {
                                const int writeSize = fwdfrm->nb_samples*frameSize;
                                fwrite(fwdfrm->data[0], 1, writeSize, m_fpPcmFile);
                            }
                        }

                        bwdfrm = GenerateBackwardAudioFrame(fwdfrm);
                        if (!bwdfrm)
                        {
                            m_logger->Log(Error) << "FAILED to GENERATE backward audio frame!" << endl;
                            break;
                        }

                        af.decfrm = nullptr;
                        af.fwdfrm = fwdfrm;
                        af.bwdfrm = bwdfrm;
                        currTask->frmCnt--;
                        if (currTask->frmCnt < 0)
                            m_logger->Log(Error) << "!! ABNORMAL !! Task [" << currTask->seekPts.first << ", " << currTask->seekPts.second << "] has negative 'frmCnt'("
                                << currTask->frmCnt << ")!" << endl;

                        idleLoop = false;
                    }
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_logger->Log(DEBUG) << "Leave GenerateAudioSamplesThreadProc()." << endl;
    }

    SelfFreeAVFramePtr GenerateBackwardAudioFrame(SelfFreeAVFramePtr fwdfrm)
    {
        SelfFreeAVFramePtr bwdfrm = AllocSelfFreeAVFramePtr();
        if (!bwdfrm)
        {
            m_logger->Log(Error) << "FAILED to allocate new AVFrame for backward pcm frame!" << endl;
            return nullptr;
        }
        bwdfrm->format = fwdfrm->format;
        bwdfrm->sample_rate = fwdfrm->sample_rate;
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        int frmChannels = fwdfrm->channels;
        bwdfrm->channels = frmChannels;
        bwdfrm->channel_layout = fwdfrm->channel_layout;
#else
        int frmChannels = fwdfrm->ch_layout.nb_channels;
        bwdfrm->ch_layout = fwdfrm->ch_layout;
#endif
        bwdfrm->nb_samples = fwdfrm->nb_samples;
        int fferr;
        fferr = av_frame_get_buffer(bwdfrm.get(), 0);
        if (fferr < 0)
        {
            m_logger->Log(Error) << "av_frame_get_buffer(GenerateBackwardAudioFrame) FAILED with return code " << fferr << endl;
            return nullptr;
        }
        av_frame_copy_props(bwdfrm.get(), fwdfrm.get());
        uint32_t audFrmSize = m_swrPassThrough ? m_audFrmSize : m_swrFrmSize;
        if (IsPlanar())
        {
            audFrmSize /= frmChannels;
            if (audFrmSize == 1)
            {
                for (int j = 0; j < frmChannels; j++)
                {
                    uint8_t* srcptr = fwdfrm->data[j]+fwdfrm->nb_samples-1;
                    uint8_t* dstptr = bwdfrm->data[j];
                    for (int i = 0; i < fwdfrm->nb_samples; i++)
                        *dstptr++ = *srcptr--;
                }
            }
            else if (audFrmSize == 2)
            {
                for (int j = 0; j < frmChannels; j++)
                {
                    uint16_t* srcptr = (uint16_t*)(fwdfrm->data[j])+fwdfrm->nb_samples-1;
                    uint16_t* dstptr = (uint16_t*)(bwdfrm->data[j]);
                    for (int i = 0; i < fwdfrm->nb_samples; i++)
                        *dstptr++ = *srcptr--;
                }
            }
            else if (audFrmSize == 4)
            {
                for (int j = 0; j < frmChannels; j++)
                {
                    uint32_t* srcptr = (uint32_t*)(fwdfrm->data[j])+fwdfrm->nb_samples-1;
                    uint32_t* dstptr = (uint32_t*)(bwdfrm->data[j]);
                    for (int i = 0; i < fwdfrm->nb_samples; i++)
                        *dstptr++ = *srcptr--;
                }
            }
            else if (audFrmSize == 8)
            {
                for (int j = 0; j < frmChannels; j++)
                {
                    uint64_t* srcptr = (uint64_t*)(fwdfrm->data[j])+fwdfrm->nb_samples-1;
                    uint64_t* dstptr = (uint64_t*)(bwdfrm->data[j]);
                    for (int i = 0; i < fwdfrm->nb_samples; i++)
                        *dstptr++ = *srcptr--;
                }
            }
            else
            {
                for (int j = 0; j < frmChannels; j++)
                {
                    uint8_t* srcptr = fwdfrm->data[j]+(fwdfrm->nb_samples-1)*audFrmSize;
                    uint8_t* dstptr = bwdfrm->data[j];
                    for (int i = 0; i < fwdfrm->nb_samples; i++)
                    {
                        memcpy(dstptr, srcptr, audFrmSize);
                        srcptr -= audFrmSize;
                        dstptr += audFrmSize;
                    }
                }
            }
        }
        else
        {
            if (audFrmSize == 1)
            {
                uint8_t* srcptr = fwdfrm->data[0]+fwdfrm->nb_samples-1;
                uint8_t* dstptr = bwdfrm->data[0];
                for (int i = 0; i < fwdfrm->nb_samples; i++)
                    *dstptr++ = *srcptr--;
            }
            else if (audFrmSize == 2)
            {
                uint16_t* srcptr = (uint16_t*)(fwdfrm->data[0])+fwdfrm->nb_samples-1;
                uint16_t* dstptr = (uint16_t*)(bwdfrm->data[0]);
                for (int i = 0; i < fwdfrm->nb_samples; i++)
                    *dstptr++ = *srcptr--;
            }
            else if (audFrmSize == 4)
            {
                uint32_t* srcptr = (uint32_t*)(fwdfrm->data[0])+fwdfrm->nb_samples-1;
                uint32_t* dstptr = (uint32_t*)(bwdfrm->data[0]);
                for (int i = 0; i < fwdfrm->nb_samples; i++)
                    *dstptr++ = *srcptr--;
            }
            else if (audFrmSize == 8)
            {
                uint64_t* srcptr = (uint64_t*)(fwdfrm->data[0])+fwdfrm->nb_samples-1;
                uint64_t* dstptr = (uint64_t*)(bwdfrm->data[0]);
                for (int i = 0; i < fwdfrm->nb_samples; i++)
                    *dstptr++ = *srcptr--;
            }
            else
            {
                uint8_t* srcptr = fwdfrm->data[0]+(fwdfrm->nb_samples-1)*audFrmSize;
                uint8_t* dstptr = bwdfrm->data[0];
                for (int i = 0; i < fwdfrm->nb_samples; i++)
                {
                    memcpy(dstptr, srcptr, audFrmSize);
                    srcptr -= audFrmSize;
                    dstptr += audFrmSize;
                }
            }
        }
        bwdfrm->pts = fwdfrm->pts+fwdfrm->nb_samples;
        return bwdfrm;
    }

    pair<int64_t, int64_t> GetSeekPosByTs(double ts)
    {
        int64_t targetPts = CvtMtsToPts((int64_t)(ts*1000));
        auto iter = find_if(m_hSeekPoints->begin(), m_hSeekPoints->end(),
            [targetPts](int64_t keyPts) { return keyPts > targetPts; });
        if (iter != m_hSeekPoints->begin())
            iter--;
        int64_t first = *iter++;
        int64_t second = iter == m_hSeekPoints->end() ? INT64_MAX : *iter;
        return { first, second };
    }

    void UpdateCacheWindow(double readPos, bool forceUpdate = false)
    {
        if (readPos == m_cacheWnd.readPos && !forceUpdate)
            return;

        const double beforeCacheDur = m_readForward ? m_backwardCacheDur : m_forwardCacheDur;
        const double afterCacheDur = m_readForward ? m_forwardCacheDur : m_backwardCacheDur;
        double cacheBeginTs, cacheEndTs;
        int64_t seekPosRead, seekPos00, seekPos10;
        if (m_isVideoReader)
        {
            cacheBeginTs = readPos > beforeCacheDur ? readPos-beforeCacheDur : 0;
            cacheEndTs = readPos+afterCacheDur < m_vidDurTs ? readPos+afterCacheDur : m_vidDurTs;
            seekPosRead = GetSeekPosByTs(readPos).first;
            seekPos00 = GetSeekPosByTs(cacheBeginTs).first;
            seekPos10 = GetSeekPosByTs(cacheEndTs).first;
        }
        else
        {
            cacheBeginTs = floor(readPos-beforeCacheDur);
            if (cacheBeginTs < 0) cacheBeginTs = 0;
            cacheEndTs = ceil(readPos+afterCacheDur);
            if (cacheEndTs > m_audDurTs) cacheEndTs = m_audDurTs;
            seekPosRead = CvtMtsToPts(floor(readPos)*1000);
            seekPos00 = CvtMtsToPts(cacheBeginTs*1000);
            seekPos10 = CvtMtsToPts((cacheEndTs-1)*1000);
        }
        CacheWindow cacheWnd = m_cacheWnd;
        if (seekPosRead != cacheWnd.seekPosShow || seekPos00 != cacheWnd.seekPos00 || seekPos10 != cacheWnd.seekPos10 || forceUpdate)
        {
            m_cacheWnd = { readPos, cacheBeginTs, cacheEndTs, seekPosRead, seekPos00, seekPos10 };
            m_needUpdateBldtsk = true;
        }
        m_cacheWnd.readPos = readPos;
        m_logger->Log(VERBOSE) << "Cache window updated: { readPos=" << readPos << ", cacheBeginTs=" << m_cacheWnd.cacheBeginTs << ", cacheEndTs=" << m_cacheWnd.cacheEndTs
                << ", seekPosShow=" << m_cacheWnd.seekPosShow << ", seekPos00=" << m_cacheWnd.seekPos00 << ", seekPos10=" << m_cacheWnd.seekPos10 << " }" << endl;
    }

    void ResetBuildTask()
    {
        if (m_isVideoReader)
            ResetSnapshotBuildTask();
        else
            ResetAudioSampleBuildTask();
    }

    void UpdateBuildTask()
    {
        if (m_isVideoReader)
            UpdateSnapshotBuildTask();
        else
            UpdateAudioSampleBuildTask();
    }

    void ResetSnapshotBuildTask()
    {
        CacheWindow currwnd = m_cacheWnd;
        lock_guard<mutex> lk(m_bldtskByTimeLock);
        if (!m_bldtskTimeOrder.empty())
        {
            for (auto& tsk : m_bldtskTimeOrder)
                tsk->cancel = true;
            m_bldtskTimeOrder.clear();
        }

        int64_t searchPts = CvtMtsToPts((int64_t)(currwnd.cacheBeginTs*1000));
        auto iter = find_if(m_hSeekPoints->begin(), m_hSeekPoints->end(),
            [searchPts](int64_t keyPts) { return keyPts > searchPts; });
        if (iter != m_hSeekPoints->begin())
            iter--;
        do
        {
            int64_t first = *iter++;
            int64_t second = iter == m_hSeekPoints->end() ? INT64_MAX : *iter;
            GopDecodeTaskHolder task = make_shared<GopDecodeTask>(*this);
            task->seekPts = { first, second };
            m_bldtskTimeOrder.push_back(task);
            searchPts = second;
        } while (searchPts < INT64_MAX && (double)CvtPtsToMts(searchPts)/1000 <= currwnd.cacheEndTs);
        m_bldtskSnapWnd = currwnd;
        m_logger->Log(DEBUG) << "^^^ Initialized build task, pos = " << TimestampToString(m_bldtskSnapWnd.readPos) << ", window = ["
            << TimestampToString(m_bldtskSnapWnd.cacheBeginTs) << " ~ " << TimestampToString(m_bldtskSnapWnd.cacheEndTs) << "]." << endl;

        UpdateBuildTaskByPriority();
    }

    void UpdateSnapshotBuildTask()
    {
        CacheWindow currwnd = m_cacheWnd;
        if (currwnd.cacheBeginTs != m_bldtskSnapWnd.cacheBeginTs || currwnd.cacheEndTs != m_bldtskSnapWnd.cacheEndTs)
        {
            m_logger->Log(DEBUG) << "^^^ Updating build task, index changed from ("
                << TimestampToString(m_bldtskSnapWnd.cacheBeginTs) << " ~ " << TimestampToString(m_bldtskSnapWnd.cacheEndTs) << ") to ("
                << TimestampToString(currwnd.cacheBeginTs) << " ~ " << TimestampToString(currwnd.cacheEndTs) << ")." << endl;
            lock_guard<mutex> lk(m_bldtskByTimeLock);
            if (currwnd.cacheBeginTs > m_bldtskSnapWnd.cacheBeginTs ||
                (currwnd.cacheBeginTs == m_bldtskSnapWnd.cacheBeginTs && currwnd.cacheEndTs > m_bldtskSnapWnd.cacheEndTs))
            {
                int64_t beginPts = CvtMtsToPts((int64_t)(currwnd.cacheBeginTs*1000));
                int64_t endPts = CvtMtsToPts((int64_t)(currwnd.cacheEndTs*1000))+1;
                if (currwnd.seekPos00 <= m_bldtskSnapWnd.seekPos10)
                {
                    auto iter = m_bldtskTimeOrder.begin();
                    while (iter != m_bldtskTimeOrder.end())
                    {
                        auto& tsk = *iter;
                        if (tsk->seekPts.second < currwnd.seekPos00)
                        {
                            tsk->cancel = true;
                            m_logger->Log(DEBUG) << "^^> Remove task 'seekPts.first'=" << (*iter)->seekPts.first << "(" << MillisecToString(CvtPtsToMts((*iter)->seekPts.first)) << ")"
                                << ", 'seekPts.second'=" << (*iter)->seekPts.second << "(" << MillisecToString(CvtPtsToMts((*iter)->seekPts.second)) << ")" << endl;
                            iter = m_bldtskTimeOrder.erase(iter);
                        }
                        else
                            break;
                    }
                    beginPts = m_bldtskTimeOrder.back()->seekPts.second;
                }
                else
                {
                    m_logger->Log(DEBUG) << "^^> CLEAR ALL TASKS." << endl;
                    for (auto& tsk : m_bldtskTimeOrder)
                        tsk->cancel = true;
                    m_bldtskTimeOrder.clear();
                }

                if (beginPts < endPts)
                {
                    auto iter = find_if(m_hSeekPoints->begin(), m_hSeekPoints->end(),
                        [beginPts](int64_t keyPts) { return keyPts > beginPts; });
                    if (iter != m_hSeekPoints->begin())
                        iter--;
                    if (*iter < endPts)
                    {
                        do
                        {
                            int64_t first = *iter++;
                            int64_t second = iter == m_hSeekPoints->end() ? INT64_MAX : *iter;
                            GopDecodeTaskHolder task = make_shared<GopDecodeTask>(*this);
                            task->seekPts = { first, second };
                            m_bldtskTimeOrder.push_back(task);
                            beginPts = second;
                        } while (beginPts < endPts);
                    }
                }
            }
            else //(currwnd.cacheBeginTs < m_bldtskSnapWnd.cacheBeginTs)
            {
                int64_t beginPts = CvtMtsToPts((int64_t)(currwnd.cacheBeginTs*1000));
                int64_t endPts = CvtMtsToPts((int64_t)(currwnd.cacheEndTs*1000))+1;
                if (currwnd.seekPos10 >= m_bldtskSnapWnd.seekPos00)
                {
                    // buildIndex1 = m_bldtskSnapWnd.cacheIdx0-1;
                    auto iter = m_bldtskTimeOrder.end();
                    iter--;
                    while (iter != m_bldtskTimeOrder.begin())
                    {
                        auto& tsk = *iter;
                        if (tsk->seekPts.first > currwnd.seekPos10)
                        {
                            tsk->cancel = true;
                            m_logger->Log(DEBUG) << "^^> Remove task 'seekPts.first'=" << (*iter)->seekPts.first << "(" << MillisecToString(CvtPtsToMts((*iter)->seekPts.first)) << ")"
                                << ", 'seekPts.second'=" << (*iter)->seekPts.second << "(" << MillisecToString(CvtPtsToMts((*iter)->seekPts.second)) << ")" << endl;
                            iter = m_bldtskTimeOrder.erase(iter);
                            iter--;
                        }
                        else
                            break;
                    }
                    endPts = m_bldtskTimeOrder.front()->seekPts.first;
                }
                else
                {
                    m_logger->Log(DEBUG) << "^^> CLEAR ALL TASKS." << endl;
                    for (auto& tsk : m_bldtskTimeOrder)
                        tsk->cancel = true;
                    m_bldtskTimeOrder.clear();
                }

                if (beginPts < endPts)
                {
                    auto iter = find_if(m_hSeekPoints->begin(), m_hSeekPoints->end(),
                        [endPts](int64_t keyPts) { return keyPts >= endPts; });
                    if (iter != m_hSeekPoints->begin())
                    {
                        iter--;
                        do
                        {
                            auto iter2 = iter; iter2++;
                            int64_t first = *iter;
                            int64_t second = iter2 == m_hSeekPoints->end() ? INT64_MAX : *iter2;
                            GopDecodeTaskHolder task = make_shared<GopDecodeTask>(*this);
                            task->seekPts = { first, second };
                            m_bldtskTimeOrder.push_front(task);
                            if (iter != m_hSeekPoints->begin())
                                iter--;
                            else
                                break;
                            endPts = first;
                        } while (beginPts < endPts);
                    }
                }
            }
        }
        bool windowPosChanged = currwnd.seekPosShow != m_bldtskSnapWnd.seekPosShow;
        m_bldtskSnapWnd = currwnd;

        if (windowPosChanged)
            UpdateBuildTaskByPriority();
    }

    void UpdateBuildTaskByPriority()
    {
        lock_guard<mutex> lk(m_bldtskByPriLock);
        if (m_isVideoReader)
        {
            CacheWindow cwnd = m_bldtskSnapWnd;
            m_bldtskPriOrder = m_bldtskTimeOrder;
            m_bldtskPriOrder.sort([this, cwnd](const GopDecodeTaskHolder& a, const GopDecodeTaskHolder& b) {
                bool aIsShowGop = a->seekPts.first == cwnd.seekPosShow;
                if (aIsShowGop)
                    return true;
                bool bIsShowGop = b->seekPts.first == cwnd.seekPosShow;
                if (bIsShowGop)
                    return false;
                bool aIsForwardGop = a->seekPts.first > cwnd.seekPosShow;
                if (!m_readForward)
                    aIsForwardGop = !aIsForwardGop;
                bool bIsForwardGop = b->seekPts.first > cwnd.seekPosShow;
                if (!m_readForward)
                    bIsForwardGop = !bIsForwardGop;
                if (aIsForwardGop)
                {
                    if (!bIsForwardGop)
                        return true;
                    else
                        return (m_readForward^(a->seekPts.first < b->seekPts.first)) == 0;
                }
                else if (bIsForwardGop)
                    return false;
                else
                    return (m_readForward^(a->seekPts.first > b->seekPts.first)) == 0;
            });
        }
        else
        {
            m_bldtskPriOrder = m_bldtskTimeOrder;
            if (!m_bldtskPriOrder.empty())
            {
                auto iter = find(m_bldtskPriOrder.begin(), m_bldtskPriOrder.end(), m_audReadTask);
                if (iter == m_bldtskPriOrder.end())
                {
                    m_audReadTask = nullptr;
                    m_audReadOffset = -1;
                }
            }
            else
            {
                m_audReadTask = nullptr;
                m_audReadOffset = -1;
            }
        }
        m_logger->Log(DEBUG) << "Build task priority updated." << endl;
    }

    void ResetAudioSampleBuildTask()
    {
        CacheWindow currwnd = m_cacheWnd;
        lock_guard<mutex> lk(m_bldtskByTimeLock);
        if (!m_bldtskTimeOrder.empty())
        {
            for (auto& tsk : m_bldtskTimeOrder)
                tsk->cancel = true;
            m_bldtskTimeOrder.clear();
        }

        int64_t beginPts = CvtMtsToPts((int64_t)(currwnd.cacheBeginTs*1000));
        int64_t endPts = CvtMtsToPts((int64_t)(currwnd.cacheEndTs*1000));
        int64_t pts0, pts1;
        pts0 = beginPts-beginPts%m_audioTaskPtsGap;
        while (pts0 < endPts)
        {
            pts1 = pts0+m_audioTaskPtsGap;
            GopDecodeTaskHolder task = make_shared<GopDecodeTask>(*this);
            task->seekPts = { pts0, pts1 };
            if (pts0 <= 0) task->mediaBegin = true;
            m_bldtskTimeOrder.push_back(task);
            pts0 = pts1;
        }
        m_bldtskSnapWnd = currwnd;
        m_audReadTask = nullptr;
        m_audReadOffset = -1;
        m_logger->Log(DEBUG) << "^^^ Initialized build task, pos = " << TimestampToString(m_bldtskSnapWnd.readPos) << ", window = ["
            << TimestampToString(m_bldtskSnapWnd.cacheBeginTs) << " ~ " << TimestampToString(m_bldtskSnapWnd.cacheEndTs) << "]." << endl;

        UpdateBuildTaskByPriority();
    }

    void UpdateAudioSampleBuildTask()
    {
        CacheWindow currwnd = m_cacheWnd;
        bool windowAreaChanged = false;
        if (currwnd.seekPos00 != m_bldtskSnapWnd.seekPos00 || currwnd.seekPos10 != m_bldtskSnapWnd.seekPos10)
        {
            m_logger->Log(DEBUG) << "^^^ Updating build task, index changed from ("
                << TimestampToString(m_bldtskSnapWnd.cacheBeginTs) << " ~ " << TimestampToString(m_bldtskSnapWnd.cacheEndTs) << ") to ("
                << TimestampToString(currwnd.cacheBeginTs) << " ~ " << TimestampToString(currwnd.cacheEndTs) << ")." << endl;
            lock_guard<mutex> lk(m_bldtskByTimeLock);
            if (currwnd.seekPos00 > m_bldtskSnapWnd.seekPos00 ||
                (currwnd.seekPos00 == m_bldtskSnapWnd.seekPos00 && currwnd.seekPos10 > m_bldtskSnapWnd.seekPos10))
            {
                int64_t beginPts = CvtMtsToPts((int64_t)(currwnd.cacheBeginTs*1000));
                beginPts = beginPts-beginPts%m_audioTaskPtsGap;
                int64_t endPts = CvtMtsToPts((int64_t)(currwnd.cacheEndTs*1000))+1;
                if (currwnd.seekPos00 <= m_bldtskSnapWnd.seekPos00)
                {
                    beginPts = m_bldtskTimeOrder.back()->seekPts.second;
                }
                else if (currwnd.seekPos00 <= m_bldtskSnapWnd.seekPos10)
                {
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
                            break;
                    }
                    beginPts = m_bldtskTimeOrder.back()->seekPts.second;
                }
                else
                {
                    for (auto& tsk : m_bldtskTimeOrder)
                        tsk->cancel = true;
                    m_bldtskTimeOrder.clear();
                }

                int64_t pts0, pts1;
                pts0 = beginPts;
                while (pts0 < endPts)
                {
                    pts1 = pts0+m_audioTaskPtsGap;
                    GopDecodeTaskHolder task = make_shared<GopDecodeTask>(*this);
                    task->seekPts = { pts0, pts1 };
                    if (pts0 <= 0) task->mediaBegin = true;
                    m_bldtskTimeOrder.push_back(task);
                    pts0 = pts1;
                }
            }
            else //(currwnd.cacheBeginTs < m_bldtskSnapWnd.cacheBeginTs)
            {
                int64_t beginPts = CvtMtsToPts((int64_t)(currwnd.cacheBeginTs*1000));
                int64_t endPts = CvtMtsToPts((int64_t)(currwnd.cacheEndTs*1000))+1;
                if (endPts%m_audioTaskPtsGap > 0)
                    endPts = endPts-endPts%m_audioTaskPtsGap+m_audioTaskPtsGap;
                if (currwnd.seekPos10 >= m_bldtskSnapWnd.seekPos10)
                {
                    endPts = m_bldtskTimeOrder.front()->seekPts.first;
                }
                else if (currwnd.seekPos10 >= m_bldtskSnapWnd.seekPos00)
                {
                    // buildIndex1 = m_bldtskSnapWnd.cacheIdx0-1;
                    auto iter = m_bldtskTimeOrder.end();
                    iter--;
                    while (iter != m_bldtskTimeOrder.begin())
                    {
                        auto& tsk = *iter;
                        if (tsk->seekPts.first > currwnd.seekPos10+m_audioTaskPtsGap)
                        {
                            tsk->cancel = true;
                            iter = m_bldtskTimeOrder.erase(iter);
                            iter--;
                        }
                        else
                            break;
                    }
                    endPts = m_bldtskTimeOrder.front()->seekPts.first;
                }
                else
                {
                    for (auto& tsk : m_bldtskTimeOrder)
                        tsk->cancel = true;
                    m_bldtskTimeOrder.clear();
                }

                int64_t pts0, pts1;
                pts1 = endPts;
                while (beginPts < pts1)
                {
                    pts0 = pts1-m_audioTaskPtsGap;
                    GopDecodeTaskHolder task = make_shared<GopDecodeTask>(*this);
                    task->seekPts = { pts0, pts1 };
                    if (pts0 <= 0) task->mediaBegin = true;
                    m_bldtskTimeOrder.push_front(task);
                    pts1 = pts0;
                }
            }
            windowAreaChanged = true;
            if (!m_bldtskTimeOrder.empty())
            {
                auto& first = m_bldtskTimeOrder.front();
                auto& last = m_bldtskTimeOrder.back();
                m_logger->Log(DEBUG) << "^^^ Build task updated, first task seekPts=(" << first->seekPts.first << "," << first->seekPts.second
                        << "), last task seekPts=(" << last->seekPts.first << "," << last->seekPts.second << ")." << endl;
            }
        }
        m_bldtskSnapWnd = currwnd;

        if (windowAreaChanged)
            UpdateBuildTaskByPriority();
    }

    GopDecodeTaskHolder FindNextAudioReadTask()
    {
        lock_guard<mutex> lk(m_bldtskByPriLock);
        if (m_audReadEof)
            return nullptr;
        if (m_bldtskPriOrder.empty())
        {
            m_audReadTask = nullptr;
            m_audReadOffset = -1;
            m_logger->Log(WARN) << "'m_bldtskPriOrder' is EMPTY! CANNOT find next read audio task." << endl;
            return nullptr;
        }

        auto iter = find(m_bldtskPriOrder.begin(), m_bldtskPriOrder.end(), m_audReadTask);
        if (iter == m_bldtskPriOrder.end())
        {
            if (m_audReadNextTaskSeekPts0 != INT64_MIN)
            {
                auto iter2 = find_if(m_bldtskPriOrder.begin(), m_bldtskPriOrder.end(), [this](const GopDecodeTaskHolder& task) {
                    return task->seekPts.first == m_audReadNextTaskSeekPts0;
                });
                if (iter2 != m_bldtskPriOrder.end())
                {
                    m_audReadTask = *iter2;
                    m_audReadOffset = 0;
                    m_audReadNextTaskSeekPts0 = m_audReadTask->seekPts.second;
                }
                else
                {
                    m_audReadTask = nullptr;
                    m_audReadOffset = -1;
                }
            }
            else
            {
                CacheWindow currwnd = m_cacheWnd;
                const int64_t readPosPts = CvtAudMtsToPts((int64_t)(currwnd.readPos*1000));
                auto iter3 = find_if(m_bldtskPriOrder.begin(), m_bldtskPriOrder.end(), [this, readPosPts](const GopDecodeTaskHolder& task) {
                    return readPosPts >= task->seekPts.first && readPosPts < task->seekPts.second;
                });
                if (iter3 != m_bldtskPriOrder.end())
                {
                    m_audReadTask = *iter3;
                    if (m_readForward)
                        m_audReadOffset = (int)((currwnd.readPos-(double)CvtAudPtsToMts(m_audReadTask->seekPts.first)/1000)*m_swrOutSampleRate)*m_swrFrmSize;
                    else
                        m_audReadOffset = (int)(((double)CvtAudPtsToMts(m_audReadTask->seekPts.second)/1000-currwnd.readPos)*m_swrOutSampleRate)*m_swrFrmSize;
                    m_audReadNextTaskSeekPts0 = m_audReadTask->seekPts.second;
                }
                else
                {
                    // m_logger->Log(WARN) << "'m_audReadTask' CANNOT be found in 'm_bldtskPriOrder'!" << endl;
                    m_audReadTask = nullptr;
                    m_audReadOffset = -1;
                }
            }
        }
        else
        {
            bool reachEnd;
            if (m_readForward)
            {
                iter++;
                reachEnd = iter == m_bldtskPriOrder.end();
            }
            else
            {
                reachEnd = iter == m_bldtskPriOrder.begin();
                if (!reachEnd)
                    iter--;
            }
            if (reachEnd)
            {
                m_audReadTask = nullptr;
                m_audReadOffset = -1;
                if ((m_readForward && m_bldtskPriOrder.back()->mediaEnd) ||
                    (!m_readForward && m_bldtskPriOrder.front()->mediaBegin))
                {
                    m_audReadEof = true;
                    m_audReadNextTaskSeekPts0 = INT64_MIN;
                }
                else
                    m_audReadNextTaskSeekPts0 = m_readForward ? m_bldtskPriOrder.back()->seekPts.second
                        : m_bldtskPriOrder.front()->seekPts.first-m_audioTaskPtsGap;
            }
            else
            {
                m_audReadTask = *iter;
                m_audReadOffset = 0;
                m_audReadNextTaskSeekPts0 = m_audReadTask->seekPts.second;
            }
        }

        if (!m_audReadTask)
            m_logger->Log(DEBUG) << "CAN NOT find next AUDIO read task!" << endl;
        return m_audReadTask;
    }

    void ReleaseResourceProc()
    {
        if (!m_isImage)
        {
            m_logger->Log(VERBOSE) << "Quit 'ReleaseResourceProc()', this only works for IMAGE source." << endl;
            return;
        }
        while (!m_quitThread)
        {
            bool imgEof = true;
            {
                lock_guard<mutex> lk(m_bldtskByPriLock);
                GopDecodeTaskHolder nxttsk = nullptr;
                for (auto& tsk : m_bldtskPriOrder)
                {
                    if (!tsk->decodeEof)
                    {
                        imgEof = false;
                        break;
                    }
                    for (auto& vf : tsk->vfAry)
                    {
                        if (!vf.ownfrm)
                        {
                            imgEof = false;
                            break;
                        }
                    }
                    if (!imgEof)
                        break;
                }
            }
            if (!m_prepared || !imgEof)
                this_thread::sleep_for(chrono::milliseconds(100));
            else
                break;
        }
        if (!m_quitThread)
        {
            bool lockAquired = false;
            while (!(lockAquired = m_apiLock.try_lock()) && !m_quitThread)
                this_thread::sleep_for(chrono::milliseconds(5));
            if (m_quitThread)
            {
                if (lockAquired) m_apiLock.unlock();
                return;
            }
            lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
            m_logger->Log(DEBUG) << "AUTO RELEASE decoding resources." << endl;
            ReleaseResources(true);
        }
    }

    void ReleaseResources(bool callFromReleaseProc = false)
    {
        WaitAllThreadsQuit(callFromReleaseProc);
        // DO NOT flush task queues here! Because ReadVideoFrame still need to find the target image frame in the queue.
        // This is only for IMAGE instance.

        if (m_swrCtx)
        {
            swr_free(&m_swrCtx);
            m_swrCtx = nullptr;
        }
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
        m_swrOutChannels = 0;
        m_swrOutChnLyt = 0;
#else
        m_swrOutChlyt = {AV_CHANNEL_ORDER_UNSPEC, 0};
#endif
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
        m_vidAvStm = nullptr;
        m_audAvStm = nullptr;
        m_viddec = nullptr;
        m_auddec = nullptr;

        m_prepared = false;
    }

private:
    static atomic_uint32_t s_idCounter;
    uint32_t m_id;
    ALogger* m_logger;
    string m_errMsg;

    MediaParserHolder m_hParser;
    MediaInfo::InfoHolder m_hMediaInfo;
    MediaParser::SeekPointsHolder m_hSeekPoints;
    bool m_opened{false};
    bool m_configured{false};
    bool m_streamInfoFound{false};
    bool m_isVideoReader;
    bool m_isImage{false};
    bool m_started{false};
    bool m_prepared{false};
    recursive_mutex m_apiLock;
    bool m_close{false}, m_quitThread{false};

    AVFormatContext* m_avfmtCtx{nullptr};
    int m_vidStmIdx{-1};
    int m_audStmIdx{-1};
    AVStream* m_vidAvStm{nullptr};
    AVStream* m_audAvStm{nullptr};
    AVCodecPtr m_viddec{nullptr};
    AVCodecPtr m_auddec{nullptr};
    AVCodecContext* m_viddecCtx{nullptr};
    bool m_vidPreferUseHw{true};
    AVHWDeviceType m_vidUseHwType{AV_HWDEVICE_TYPE_NONE};
    AVPixelFormat m_vidHwPixFmt{AV_PIX_FMT_NONE};
    AVHWDeviceType m_viddecDevType{AV_HWDEVICE_TYPE_NONE};
    AVBufferRef* m_viddecHwDevCtx{nullptr};
    int64_t m_vidStartTime{0};
    AVRational m_vidTimeBase;
    AVCodecContext* m_auddecCtx{nullptr};
    bool m_swrPassThrough{false};
    SwrContext* m_swrCtx{nullptr};
    AVSampleFormat m_swrOutSmpfmt{AV_SAMPLE_FMT_FLT};
    int m_swrOutSampleRate{0};
#if !defined(FF_API_OLD_CHANNEL_LAYOUT) && (LIBAVUTIL_VERSION_MAJOR < 58)
    int m_swrOutChannels{0};
    int64_t m_swrOutChnLyt;
#else
    AVChannelLayout m_swrOutChlyt{AV_CHANNEL_ORDER_UNSPEC, 0};
#endif
    AVRational m_swrOutTimebase;
    int64_t m_swrOutStartTime;
    uint32_t m_swrFrmSize{0};
    bool m_isOutFmtPlanar{false};

    // demuxing thread
    thread m_demuxThread;
    // video decoding thread
    thread m_viddecThread;
    // update snapshots thread
    thread m_updateCfThread;
    // audio decoding thread
    thread m_auddecThread;
    // swr thread
    thread m_swrThread;
    // release resource thread
    thread m_releaseThread;

    double m_prevReadPos{0};
    ImGui::ImMat m_prevReadImg;
    bool m_readForward{true};
    bool m_seekPosUpdated{false};
    double m_seekPosTs{0};
    mutex m_seekPosLock;
    double m_vidfrmIntvMts{0};
    list<GopDecodeTaskHolder> m_bldtskTimeOrder;
    mutex m_bldtskByTimeLock;
    list<GopDecodeTaskHolder> m_bldtskPriOrder;
    mutex m_bldtskByPriLock;
    atomic_int32_t m_pendingVidfrmCnt{0};
    int32_t m_maxPendingVidfrmCnt{4};
    double m_forwardCacheDur{5};
    double m_backwardCacheDur{1};
    CacheWindow m_cacheWnd;
    CacheWindow m_bldtskSnapWnd;
    bool m_needUpdateBldtsk{false};
    double m_vidDurTs{0};
    double m_audDurTs{0};
    int64_t m_audDurPts{0};
    uint32_t m_audFrmSize{0};
    GopDecodeTaskHolder m_audReadTask;
    int32_t m_audReadOffset{-1};
    bool m_audReadEof{false};
    int64_t m_audReadNextTaskSeekPts0{INT64_MIN};
    int64_t m_audioTaskPtsGap{0};

    float m_ssWFacotr{1.f}, m_ssHFacotr{1.f};
    AVFrameToImMatConverter m_frmCvt;

    bool m_dumpPcm{false};
    FILE* m_fpPcmFile{NULL};
};

ALogger* MediaReader_Impl::s_logger;
atomic_uint32_t MediaReader_Impl::s_idCounter{1};

ALogger* GetMediaReaderLogger()
{
    if (!MediaReader_Impl::s_logger)
        MediaReader_Impl::s_logger = GetLogger("MReader");
    return MediaReader_Impl::s_logger;
}

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{
    MediaReader_Impl* mrd = reinterpret_cast<MediaReader_Impl*>(ctx->opaque);
    const AVPixelFormat *p;
    AVPixelFormat candidateSwfmt = AV_PIX_FMT_NONE;
    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*p);
        if (!(desc->flags & AV_PIX_FMT_FLAG_HWACCEL) && candidateSwfmt == AV_PIX_FMT_NONE)
        {
            // save this software format as candidate
            candidateSwfmt = *p;
        }
        if (mrd->CheckHwPixFmt(*p))
            return *p;
    }
    return candidateSwfmt;
}

MediaReader* CreateMediaReader(const string& loggerName)
{
    return new MediaReader_Impl(loggerName);
}

void ReleaseMediaReader(MediaReader** msrc)
{
    if (msrc == nullptr || *msrc == nullptr)
        return;
    MediaReader_Impl* ms = dynamic_cast<MediaReader_Impl*>(*msrc);
    ms->Close();
    delete ms;
    *msrc = nullptr;
}
