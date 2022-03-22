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

    MediaReader_Impl()
    {
        m_id = s_idCounter++;
        m_logger = GetMediaReaderLogger();
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

        m_ssWFacotr = outWidthFactor;
        m_ssHFacotr = outHeightFactor;

        auto vidStream = GetVideoStream();
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

        m_isVideoReader = true;
        m_configured = true;
        return true;
    }

    bool ConfigAudioReader(uint32_t outChannels, uint32_t outSampleRate) override
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
        lock_guard<recursive_mutex> lk(m_apiLock);

        m_swrOutChannels = outChannels;
        m_swrOutSampleRate = outSampleRate;

        m_isVideoReader = false;
        m_configured = true;
        return true;
    }

    bool Start(bool suspend) override
    {
        if (!m_configured)
        {
            m_errMsg = "Can NOT start a 'MediaReader' until it's been configured!";
            return false;
        }
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_started)
            return true;

        if (!suspend || !m_isVideoReader)
            StartAllThreads();
        else
            ReleaseVideoResource();
        m_started = true;
        return true;
    }

    void Close() override
    {
        m_quit = true;
        lock_guard<recursive_mutex> lk(m_apiLock);
        WaitAllThreadsQuit();
        FlushAllQueues();

        if (m_swrCtx)
        {
            swr_free(&m_swrCtx);
            m_swrCtx = nullptr;
        }
        m_swrOutChannels = 0;
        m_swrOutChnLyt = 0;
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
        m_opened = false;

        m_errMsg = "";
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
        if (m_readForward != forward)
        {
            m_readForward = forward;
            if (m_prepared)
                UpdateCacheWindow(m_cacheWnd.readPos, true);
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
        if (m_quit || !m_isVideoReader)
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
        if (!m_quit || !m_isVideoReader)
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
        return m_started && m_quit;
    }

    bool IsDirectionForward() const override
    {
        return m_readForward;
    }

    bool ReadVideoFrame(double pos, ImGui::ImMat& m, bool& eof, bool wait) override
    {
        if (!m_started)
        {
            m_errMsg = "Invalid state! Can NOT read video frame from a 'MediaReader' until it's started!";
            return false;
        }
        if (pos < 0 || pos > m_vidDurTs)
        {
            m_errMsg = "Invalid argument! 'pos' can NOT be negative or larger than video's duration.";
            eof = true;
            return false;
        }
        eof = false;
        if ((pos == m_prevReadPos || m_quit) && !m_prevReadImg.empty())
        {
            m = m_prevReadImg;
            return true;
        }
        while (!m_prepared && !m_quit)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_quit)
            return false;

        lock_guard<recursive_mutex> lk(m_apiLock);
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
            m_errMsg = "Invalid state! Can NOT read video frame from a 'MediaReader' until it's started!";
            return false;
        }
        while (!m_prepared && !m_quit)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_quit)
            return false;
        eof = false;

        lock_guard<recursive_mutex> lk(m_apiLock);
        bool success = ReadAudioSamples_Internal(buf, size, pos, wait);
        double readDur = m_swrPassThrough ?
                (double)size/m_audFrmSize/m_swrOutSampleRate :
                (double)size/m_swrFrmSize/m_swrOutSampleRate;
        UpdateCacheWindow(pos+readDur);
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

    uint32_t GetAudioOutChannels() const override
    {
        return m_swrOutChannels;
    }

    uint32_t GetAudioOutSampleRate() const override
    {
        return m_swrOutSampleRate;
    }

    uint32_t GetAudioOutFrameSize() const override
    {
        return m_swrPassThrough ? m_audFrmSize : m_swrFrmSize;
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
        return av_rescale_q(pts-m_vidAvStm->start_time, m_vidAvStm->time_base, MILLISEC_TIMEBASE);
    }

    int64_t CvtVidMtsToPts(int64_t mts)
    {
        return av_rescale_q(mts, MILLISEC_TIMEBASE, m_vidAvStm->time_base)+m_vidAvStm->start_time;
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

        if (m_vidStmIdx >= 0)
        {
            MediaInfo::VideoStream* vidStream = dynamic_cast<MediaInfo::VideoStream*>(m_hMediaInfo->streams[m_vidStmIdx].get());
            m_vidDurTs = vidStream->duration;
            AVRational avgFrmRate = { vidStream->avgFrameRate.num, vidStream->avgFrameRate.den };
            AVRational timebase = { vidStream->timebase.num, vidStream->timebase.den };
            m_vidfrmIntvMts = av_q2d(av_inv_q(avgFrmRate))*1000.;
        }

        if (m_audStmIdx >= 0)
        {
            MediaInfo::AudioStream* audStream = dynamic_cast<MediaInfo::AudioStream*>(m_hMediaInfo->streams[m_audStmIdx].get());
            m_audDurTs = audStream->duration;
            m_audFrmSize = (audStream->bitDepth>>3)*audStream->channels;
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
        } while (!locked && !m_quit);
        if (m_quit)
            return false;

        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        int fferr;
        fferr = avformat_find_stream_info(m_avfmtCtx, nullptr);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avformat_open_input", fferr);
            return false;
        }

        if (m_isVideoReader)
        {
            m_hParser->EnableParseInfo(MediaParser::VIDEO_SEEK_POINTS);
            m_hSeekPoints = m_hParser->GetVideoSeekPoints();
            if (!m_hSeekPoints)
            {
                m_errMsg = "FAILED to retrieve video seek points!";
                m_logger->Log(Error) << m_errMsg << endl;
                return false;
            }

            m_vidAvStm = m_avfmtCtx->streams[m_vidStmIdx];

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
        fferr = avcodec_parameters_to_context(m_viddecCtx, m_vidAvStm->codecpar);
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
        int inChannels = m_audAvStm->codecpar->channels;
        uint64_t inChnLyt = m_audAvStm->codecpar->channel_layout;
        int inSampleRate = m_audAvStm->codecpar->sample_rate;
        AVSampleFormat inSmpfmt = (AVSampleFormat)m_audAvStm->codecpar->format;
        m_swrOutChnLyt = av_get_default_channel_layout(m_swrOutChannels);
        if (inChnLyt <= 0)
            inChnLyt = av_get_default_channel_layout(inChannels);
        if (m_swrOutChnLyt != inChnLyt || m_swrOutSmpfmt != inSmpfmt || m_swrOutSampleRate != inSampleRate)
        {
            m_swrCtx = swr_alloc_set_opts(NULL, m_swrOutChnLyt, m_swrOutSmpfmt, m_swrOutSampleRate, inChnLyt, inSmpfmt, inSampleRate, 0, nullptr);
            if (!m_swrCtx)
            {
                m_errMsg = "FAILED to invoke 'swr_alloc_set_opts()' to create 'SwrContext'!";
                return false;
            }
            int fferr = swr_init(m_swrCtx);
            if (fferr < 0)
            {
                m_errMsg = FFapiFailureMessage("swr_init", fferr);
                return false;
            }
            m_swrOutTimebase = { 1, m_swrOutSampleRate };
            m_swrOutStartTime = av_rescale_q(m_audAvStm->start_time, m_audAvStm->time_base, m_swrOutTimebase);
            m_swrFrmSize = av_get_bytes_per_sample(m_swrOutSmpfmt)*m_swrOutChannels;
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
        m_quit = false;
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
        if (!m_opened)
            return false;
        if (!m_prepared)
        {
            m.time_stamp = ts;
            return true;
        }

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
        } while (!m_quit);
        if (!targetTask)
        {
            m.time_stamp = ts;
            return true;
        }
        if (targetTask->demuxEof && targetTask->frmPtsAry.empty())
        {
            m_logger->Log(WARN) << "Current task [" << targetTask->seekPts.first << "(" << MillisecToString(CvtPtsToMts(targetTask->seekPts.first)) << "), "
                << targetTask->seekPts.second << "(" << MillisecToString(CvtPtsToMts(targetTask->seekPts.second)) << ")) has NO FRM PTS!" << endl;
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
        } while (!m_quit);

        if (foundBestFrame)
        {
            if (wait)
                while(!m_quit && !bestfrmIter->ownfrm)
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
        if (m_audReadEof)
        {
            size = 0;
            return true;
        }

        uint8_t* dstptr = buf;
        uint32_t readSize = 0, toReadSize = size, skipSize;
        skipSize = m_audReadOffset > 0 ? m_audReadOffset : 0;
        list<AudioFrame>::iterator fwditer;
        list<AudioFrame>::reverse_iterator bwditer;
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
                if (!readTask)
                {
                    if ((m_readForward && m_bldtskPriOrder.back()->mediaEof) ||
                        (!m_readForward && m_bldtskPriOrder.front()->mediaEof))
                    {
                        m_audReadEof = true;
                        size = readSize;
                        return true;
                    }
                }
            }

            if (readTask)
            {
                if (!readTask->afAry.empty() && (m_readForward || readTask->afAry.back().endOfGop))
                {
                    auto& afAry = readTask->afAry;
                    if (!isIterSet)
                    {
                        if (m_readForward)
                            fwditer = afAry.begin();
                        else
                            bwditer = afAry.rbegin();
                        isIterSet = true;
                    }

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
                        if (skipSize >= readfrm->linesize[0])
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
                                skipSize -= readfrm->linesize[0];
                                idleLoop = false;
                            }
                            else
                            {
                                m_audReadEof = true;
                                size = readSize;
                                return true;
                            }
                        }
                        else
                        {
                            uint32_t copySize = readfrm->linesize[0]-skipSize;
                            if (copySize > toReadSize)
                                copySize = toReadSize;
                            memcpy(dstptr+readSize, readfrm->data[0]+skipSize, copySize);
                            toReadSize -= copySize;
                            readSize += copySize;
                            skipSize = 0;
                            m_audReadOffset += copySize;

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
                                isIterSet = false;
                            }
                            idleLoop = false;
                        }
                    }
                }
            }

            needLoop = ((readTask && !readTask->cancel) || (!readTask && wait) || !idleLoop) && toReadSize > 0 && !m_quit;
            if (needLoop && idleLoop)
                this_thread::sleep_for(chrono::milliseconds(2));
        } while (needLoop);
        size = readSize;
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
        bool mediaEof{false};
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
        while (!m_quit)
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
                        m_logger->Log(DEBUG) << "~~~~ Current demux task canceled" << endl;
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
                        int fferr = avformat_seek_file(m_avfmtCtx, stmidx, INT64_MIN, currTask->seekPts.first, currTask->seekPts.first, 0);
                        if (fferr < 0)
                        {
                            m_logger->Log(Error) << "avformat_seek_file() FAILED for seeking to 'currTask->startPts'(" << currTask->seekPts.first << ")! fferr = " << fferr << "!" << endl;
                            break;
                        }
                        demuxEof = false;
                        int64_t ptsAfterSeek = INT64_MIN;
                        if (!ReadNextStreamPacket(stmidx, &avpkt, &avpktLoaded, &ptsAfterSeek))
                            break;
                        if (ptsAfterSeek == INT64_MAX)
                            demuxEof = true;
                        else if (ptsAfterSeek != currTask->seekPts.first)
                        {
                            m_logger->Log(WARN) << "WARNING! 'ptsAfterSeek'(" << ptsAfterSeek << ") != 'ssTask->startPts'(" << currTask->seekPts.first << ")!" << endl;
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
                            currTask->mediaEof = true;
                            currTask->demuxEof = true;
                            demuxEof = true;
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
                                currTask->avpktQ.push_back(enqpkt);
                                if (lastPktPts != enqpkt->pts)
                                {
                                    currTask->frmPtsAry.push_back(enqpkt->pts);
                                    lastPktPts = enqpkt->pts;
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
        } while (fferr >= 0 && !m_quit);
        if (m_quit)
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
            if (task->vfAry.size() >= task->frmPtsAry.size())
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

        while (!m_prepared && !m_quit)
            this_thread::sleep_for(chrono::milliseconds(5));

        GopDecodeTaskHolder currTask;
        AVFrame avfrm = {0};
        bool avfrmLoaded = false;
        bool needResetDecoder = false;
        bool sentNullPacket = false;
        while (!m_quit)
        {
            bool idleLoop = true;
            bool quitLoop = false;

            if (currTask && currTask->cancel)
            {
                m_logger->Log(DEBUG) << "~~~~ Current video task canceled" << endl;
                if (avfrmLoaded)
                {
                    av_frame_unref(&avfrm);
                    avfrmLoaded = false;
                }
                currTask = nullptr;
            }

            if (!currTask || currTask->decInputEof)
            {
                GopDecodeTaskHolder oldTask = currTask;
                currTask = FindNextDecoderTask();
                if (currTask)
                {
                    currTask->decoding = true;
                    // m_logger->Log(DEBUG) << "==> Change decoding task to build index (" << currTask->ssIdxPair.first << " ~ " << currTask->ssIdxPair.second << ")." << endl;
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
                                // m_logger->Log(DEBUG) << "Video decoder current task reaches EOF!" << endl;
                            }
                        }
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
                    }
                } while (hasOutput && !m_quit && (!currTask || !currTask->cancel));
                if (quitLoop)
                    break;
                if (currTask && currTask->cancel)
                    continue;

                // input packet to decoder
                if (!sentNullPacket)
                {
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
                        else if (fferr != AVERROR(EAGAIN) && fferr != AVERROR_INVALIDDATA)
                        {
                            m_errMsg = FFapiFailureMessage("avcodec_send_packet", fferr);
                            m_logger->Log(Error) << "FAILED to invoke 'avcodec_send_packet'(VideoDecodeThreadProc)! return code is "
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

        while (!m_prepared && !m_quit)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_quit)
            return;

        GopDecodeTaskHolder currTask;
        while (!m_quit)
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
            af.endOfGop = nextPts >= iter->get()->seekPts.second;
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

        while (!m_prepared && !m_quit)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_quit)
            return;

        GopDecodeTaskHolder currTask;
        AVFrame avfrm = {0};
        bool avfrmLoaded = false;
        while (!m_quit)
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
                } while (hasOutput && !m_quit);
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
                this_thread::sleep_for(chrono::milliseconds(1));
        }
        if (avfrmLoaded)
            av_frame_unref(&avfrm);
        m_logger->Log(DEBUG) << "Leave AudioDecodeThreadProc()." << endl;
    }

    void GenerateAudioSamplesThreadProc()
    {
        m_logger->Log(DEBUG) << "Enter GenerateAudioSamplesThreadProc()..." << endl;

        while (!m_prepared && !m_quit)
            this_thread::sleep_for(chrono::milliseconds(5));
        if (m_quit)
            return;

        uint32_t audFrmSize = m_swrFrmSize;
        GopDecodeTaskHolder currTask;
        AVRational audTimebase = m_audAvStm->time_base;
        while (!m_quit)
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
                            dstfrm->channels = m_swrOutChannels;
                            dstfrm->channel_layout = m_swrOutChnLyt;
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
                                dstfrm->linesize[0] = fferr*m_swrFrmSize;
                            }
                            af.pts = dstfrm->pts;
                        }

                        bwdfrm = AllocSelfFreeAVFramePtr();
                        if (!bwdfrm)
                        {
                            m_logger->Log(Error) << "FAILED to allocate new AVFrame for backward pcm frame!" << endl;
                            break;
                        }
                        bwdfrm->format = fwdfrm->format;
                        bwdfrm->sample_rate = fwdfrm->sample_rate;
                        bwdfrm->channels = fwdfrm->channels;
                        bwdfrm->channel_layout = fwdfrm->channel_layout;
                        bwdfrm->nb_samples = fwdfrm->nb_samples;
                        fferr = av_frame_get_buffer(bwdfrm.get(), 0);
                        if (fferr < 0)
                        {
                            m_logger->Log(Error) << "av_frame_get_buffer(UpdatePcmThreadProc2) FAILED with return code " << fferr << endl;
                            break;
                        }
                        av_frame_copy_props(bwdfrm.get(), fwdfrm.get());
                        uint8_t* srcptr = fwdfrm->data[0]+(fwdfrm->nb_samples-1)*audFrmSize;
                        uint8_t* dstptr = bwdfrm->data[0];
                        for (int i = 0; i < fwdfrm->nb_samples; i++)
                        {
                            memcpy(dstptr, srcptr, audFrmSize);
                            srcptr -= audFrmSize;
                            dstptr += audFrmSize;
                        }
                        bwdfrm->linesize[0] = fwdfrm->linesize[0];
                        bwdfrm->pts = fwdfrm->pts+fwdfrm->nb_samples;

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
                this_thread::sleep_for(chrono::milliseconds(1));
        }
        m_logger->Log(DEBUG) << "Leave GenerateAudioSamplesThreadProc()." << endl;
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
        int64_t pts0, pts1, mts0, mts1;
        pts0 = beginPts;
        mts0 = CvtPtsToMts(pts0);
        while (pts0 < endPts)
        {
            mts1 = mts0+1000;
            if ((double)mts1/1000 >= currwnd.cacheEndTs-0.5)
                pts1 = endPts;
            else
                pts1 = CvtMtsToPts(mts1);
            GopDecodeTaskHolder task = make_shared<GopDecodeTask>(*this);
            task->seekPts = { pts0, pts1 };
            m_bldtskTimeOrder.push_back(task);
            pts0 = pts1;
            mts0 = mts1;
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
                int64_t endPts = CvtMtsToPts((int64_t)(currwnd.cacheEndTs*1000));
                if (currwnd.seekPos00 <= m_bldtskSnapWnd.seekPos10)
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

                int64_t pts0, pts1, mts0, mts1;
                pts0 = beginPts;
                mts0 = CvtPtsToMts(pts0);
                while (pts0 < endPts)
                {
                    mts1 = mts0+1000;
                    if ((double)mts1/1000 >= currwnd.cacheEndTs-0.5)
                        pts1 = endPts;
                    else
                        pts1 = CvtMtsToPts(mts1);
                    GopDecodeTaskHolder task = make_shared<GopDecodeTask>(*this);
                    task->seekPts = { pts0, pts1 };
                    m_bldtskTimeOrder.push_back(task);
                    pts0 = pts1;
                    mts0 = mts1;
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

                int64_t pts0, pts1, mts0, mts1;
                pts1 = endPts;
                mts1 = CvtPtsToMts(pts1);
                while (beginPts < pts1)
                {
                    mts0 = mts1-1000;
                    if ((double)mts0/1000 <= currwnd.cacheBeginTs-0.5)
                        pts0 = beginPts;
                    else
                        pts0 = CvtMtsToPts(mts0);
                    GopDecodeTaskHolder task = make_shared<GopDecodeTask>(*this);
                    task->seekPts = { pts0, pts1 };
                    m_bldtskTimeOrder.push_front(task);
                    pts1 = pts0;
                    mts1 = mts0;
                }
            }
            windowAreaChanged = true;
        }
        m_bldtskSnapWnd = currwnd;

        if (windowAreaChanged)
            UpdateBuildTaskByPriority();
    }

    GopDecodeTaskHolder FindNextAudioReadTask()
    {
        lock_guard<mutex> lk(m_bldtskByPriLock);
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
                auto iter3 = find_if(m_bldtskPriOrder.begin(), m_bldtskPriOrder.end(), [this, currwnd](const GopDecodeTaskHolder& task) {
                    return currwnd.readPos*1000 >= CvtAudPtsToMts(task->seekPts.first) && currwnd.readPos*1000 < CvtAudPtsToMts(task->seekPts.second);
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
                    m_logger->Log(WARN) << "'m_audReadTask' CANNOT be found in 'm_bldtskPriOrder'!" << endl;
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
            }
            else
            {
                m_audReadTask = *iter;
                m_audReadOffset = 0;
                m_audReadNextTaskSeekPts0 = m_audReadTask->seekPts.second;
            }
        }
        return m_audReadTask;
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
    bool m_isVideoReader;
    bool m_started{false};
    bool m_prepared{false};
    recursive_mutex m_apiLock;
    bool m_quit{false};

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
    AVCodecContext* m_auddecCtx{nullptr};
    bool m_swrPassThrough{false};
    SwrContext* m_swrCtx{nullptr};
    AVSampleFormat m_swrOutSmpfmt{AV_SAMPLE_FMT_FLT};
    int m_swrOutSampleRate;
    int m_swrOutChannels;
    int64_t m_swrOutChnLyt;
    AVRational m_swrOutTimebase;
    int64_t m_swrOutStartTime;
    uint32_t m_swrFrmSize{0};

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
    uint32_t m_audFrmSize{0};
    GopDecodeTaskHolder m_audReadTask;
    int32_t m_audReadOffset{-1};
    bool m_audReadEof{false};
    int64_t m_audReadNextTaskSeekPts0{INT64_MIN};

    float m_ssWFacotr{1.f}, m_ssHFacotr{1.f};
    AVFrameToImMatConverter m_frmCvt;
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
    for (p = pix_fmts; *p != -1; p++) {
        if (mrd->CheckHwPixFmt(*p))
            return *p;
    }
    return AV_PIX_FMT_NONE;
}

MediaReader* CreateMediaReader()
{
    return new MediaReader_Impl();
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
