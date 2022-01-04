#include <sstream>
#include <thread>
#include <algorithm>
#include "MediaOverview.h"
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

class MediaOverview_Impl : public MediaOverview
{
public:
    MediaOverview_Impl() = default;
    MediaOverview_Impl(const MediaOverview_Impl&) = delete;
    MediaOverview_Impl(MediaOverview_Impl&&) = delete;
    MediaOverview_Impl& operator=(const MediaOverview_Impl&) = delete;

    virtual ~MediaOverview_Impl() {}

    bool Open(const string& url, uint32_t snapshotCount) override
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
        m_ssCount = snapshotCount;
        m_ssIntvMts = (double)m_vidDurMts/m_ssCount;

        BuildSnapshots();
        m_opened = true;
        return true;
    }

    bool Open(MediaParserHolder hParser, uint32_t snapshotCount) override
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
        m_ssCount = snapshotCount;
        m_ssIntvMts = (double)m_vidDurMts/m_ssCount;

        BuildSnapshots();
        m_opened = true;
        return true;
    }

    void Close() override
    {
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
        m_vidAvStm = nullptr;
        m_audAvStm = nullptr;
        m_viddec = nullptr;
        m_auddec = nullptr;
        m_hParser = nullptr;
        m_hMediaInfo = nullptr;

        m_demuxEof = false;
        m_viddecEof = false;
        m_genSsEof = false;
        m_opened = false;
        m_prepared = false;

        m_errMsg = "";
    }

    bool GetSnapshots(std::vector<ImGui::ImMat>& snapshots) override
    {
        if (!IsOpened())
            return false;

        snapshots.clear();
        for (auto& ss : m_snapshots)
        {
            if (ss.sameFrame)
                snapshots.push_back(m_snapshots[ss.sameAsIndex].img);
            else
                snapshots.push_back(ss.img);
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

    bool IsDone() const override
    {
        return m_genSsEof;
    }

    bool HasVideo() const override
    {
        return m_vidStmIdx >= 0;
    }

    bool HasAudio() const override
    {
        return m_audStmIdx >= 0;
    }

    uint32_t GetSnapshotCount() const override
    {
        if (!IsOpened())
            return 0;
        return m_ssCount;
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
        RebuildSnapshots();
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

            uint32_t outWidth = (uint32_t)ceil(m_vidAvStm->codecpar->width*widthFactor);
            if ((outWidth&0x1) == 1)
                outWidth++;
            uint32_t outHeight = (uint32_t)ceil(m_vidAvStm->codecpar->height*heightFactor);
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
        RebuildSnapshots();
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
        RebuildSnapshots();
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
        if (m_vidAvStm)
        {
            return m_vidAvStm->codecpar->width;
        }
        return 0;
    }

    uint32_t GetVideoHeight() const override
    {
        if (m_vidAvStm)
        {
            return m_vidAvStm->codecpar->height;
        }
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

    uint32_t GetAudioChannel() const override
    {
        if (!HasAudio())
            return 0;
        return m_audAvStm->codecpar->channels;
    }

    uint32_t GetAudioSampleRate() const override
    {
        if (!HasAudio())
            return 0;
        return m_audAvStm->codecpar->sample_rate;
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
    struct Snapshot
    {
        uint32_t index{0};
        bool sameFrame{false};
        uint32_t sameAsIndex{0};
        int64_t ssFrmPts{INT64_MIN};
        ImGui::ImMat img;
    };

    string FFapiFailureMessage(const string& apiName, int fferr)
    {
        ostringstream oss;
        oss << "FF api '" << apiName << "' returns error! fferr=" << fferr << ".";
        return oss.str();
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
        int fferr;
        fferr = avformat_find_stream_info(m_avfmtCtx, nullptr);
        if (fferr < 0)
        {
            m_errMsg = FFapiFailureMessage("avformat_open_input", fferr);
            return false;
        }

        if (HasVideo())
        {
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

        if (HasAudio())
        {
            m_audAvStm = m_avfmtCtx->streams[m_audStmIdx];

            // wyvern: disable opening audio decoder because we don't use it now
            // if (!OpenAudioDecoder())
            //     return false;
        }

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
        Log(DEBUG) << "Video decoder(HW) '" << m_viddecCtx->codec->name << "' opened." << endl;
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
        m_auddecCtx->opaque = this;

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
        Log(DEBUG) << "Audio decoder '" << m_auddec->name << "' opened." << endl;

        // setup sw resampler
        int inChannels = m_audAvStm->codecpar->channels;
        uint64_t inChnLyt = m_audAvStm->codecpar->channel_layout;
        int inSampleRate = m_audAvStm->codecpar->sample_rate;
        AVSampleFormat inSmpfmt = (AVSampleFormat)m_audAvStm->codecpar->format;
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
                m_errMsg = "FAILED to invoke 'swr_alloc_set_opts()' to create 'SwrContext'!";
                return false;
            }
            int fferr = swr_init(m_swrCtx);
            if (fferr < 0)
            {
                m_errMsg = FFapiFailureMessage("swr_init", fferr);
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

    void BuildSnapshots()
    {
        m_snapshots.clear();
        for (uint32_t i = 0; i < m_ssCount; i++)
        {
            Snapshot ss;
            ss.index = i;
            ss.img.time_stamp = (m_ssIntvMts*i+m_vidStartMts)/1000.;
            m_snapshots.push_back(ss);
        }
        StartAllThreads();
    }

    void StartAllThreads()
    {
        m_quitScan = false;
        m_demuxThread = thread(&MediaOverview_Impl::DemuxThreadProc, this);
        if (HasVideo())
            m_viddecThread = thread(&MediaOverview_Impl::VideoDecodeThreadProc, this);
        m_GenSsThread = thread(&MediaOverview_Impl::GenerateSsThreadProc, this);
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
        if (m_GenSsThread.joinable())
        {
            m_GenSsThread.join();
            m_GenSsThread = thread();
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

    void RebuildSnapshots()
    {
        if (!IsOpened())
            return;

        WaitAllThreadsQuit();
        FlushAllQueues();
        if (m_viddecCtx)
            avcodec_flush_buffers(m_viddecCtx);
        if (m_auddecCtx)
            avcodec_flush_buffers(m_auddecCtx);
        BuildSnapshots();
    }

    void DemuxThreadProc()
    {
        Log(DEBUG) << "Enter DemuxThreadProc()..." << endl;

        if (!m_prepared && !Prepare())
        {
            Log(ERROR) << "Prepare() FAILED! Error is '" << m_errMsg << "'." << endl;
            return;
        }

        AVPacket avpkt = {0};
        bool avpktLoaded = false;
        while (!m_quitScan)
        {
            bool idleLoop = true;

            if (HasVideo())
            {
                auto iter = find_if(m_snapshots.begin(), m_snapshots.end(), [](const Snapshot& ss) {
                    return ss.ssFrmPts == INT64_MIN;
                });
                if (iter == m_snapshots.end())
                    break;

                Snapshot& ss = *iter;
                int fferr;
                int64_t seekTargetPts = ss.ssFrmPts != INT64_MIN ? ss.ssFrmPts :
                    av_rescale_q((int64_t)(m_ssIntvMts*ss.index+m_vidStartMts), MILLISEC_TIMEBASE, m_vidAvStm->time_base);
                fferr = avformat_seek_file(m_avfmtCtx, m_vidStmIdx, INT64_MIN, seekTargetPts, seekTargetPts, 0);
                if (fferr < 0)
                {
                    Log(ERROR) << "avformat_seek_file() FAILED for seeking to pts(" << seekTargetPts << ")! fferr = " << fferr << "!" << endl;
                    break;
                }

                bool enqDone = false;
                while (!m_quitScan && !enqDone)
                {
                    if (!avpktLoaded)
                    {
                        int fferr = av_read_frame(m_avfmtCtx, &avpkt);
                        if (fferr == 0)
                        {
                            avpktLoaded = true;
                            idleLoop = false;
                            ss.ssFrmPts = avpkt.pts;
                            auto iter2 = iter;
                            if (avpkt.stream_index == m_vidStmIdx && iter2 != m_snapshots.begin())
                            {
                                iter2--;
                                if (iter2->ssFrmPts == ss.ssFrmPts)
                                {
                                    ss.sameFrame = true;
                                    ss.sameAsIndex = iter2->sameFrame ? iter2->sameAsIndex : iter2->index;
                                    av_packet_unref(&avpkt);
                                    avpktLoaded = false;
                                    enqDone = true;
                                }
                            }
                        }
                        else
                        {
                            if (fferr != AVERROR_EOF)
                                Log(ERROR) << "Demuxer ERROR! 'av_read_frame()' returns " << fferr << "." << endl;
                            break;
                        }
                    }

                    if (avpktLoaded)
                    {
                        if (avpkt.stream_index == m_vidStmIdx)
                        {
                            if (m_vidpktQ.size() < m_vidpktQMaxSize)
                            {
                                AVPacket* enqpkt = av_packet_clone(&avpkt);
                                if (!enqpkt)
                                {
                                    Log(ERROR) << "FAILED to invoke 'av_packet_clone(DemuxThreadProc)'!" << endl;
                                    break;
                                }
                                {
                                    lock_guard<mutex> lk(m_vidpktQLock);
                                    m_vidpktQ.push_back(enqpkt);
                                }
                                av_packet_unref(&avpkt);
                                avpktLoaded = false;
                                idleLoop = false;
                                enqDone = true;
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
        if (avpktLoaded)
            av_packet_unref(&avpkt);
        m_demuxEof = true;
        Log(DEBUG) << "Leave DemuxThreadProc()." << endl;
    }

    void VideoDecodeThreadProc()
    {
        Log(DEBUG) << "Enter VideoDecodeThreadProc()..." << endl;

        while (!m_prepared && !m_quitScan)
            this_thread::sleep_for(chrono::milliseconds(5));

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
                        // Log(DEBUG) << "<<< Get video frame pts=" << avfrm.pts << "(" << MillisecToString(av_rescale_q(avfrm.pts, m_vidAvStm->time_base, MILLISEC_TIMEBASE)) << ")." << endl;
                        avfrmLoaded = true;
                        idleLoop = false;
                    }
                    else if (fferr != AVERROR(EAGAIN))
                    {
                        if (fferr != AVERROR_EOF)
                        {
                            Log(ERROR) << "FAILED to invoke 'avcodec_receive_frame'(VideoDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
                        }
                        quitLoop = true;
                    }
                }

                hasOutput = avfrmLoaded;
                if (avfrmLoaded && m_vidfrmQ.size() < m_vidfrmQMaxSize)
                {
                    AVFrame* enqfrm = av_frame_clone(&avfrm);
                    {
                        lock_guard<mutex> lk(m_vidfrmQLock);
                        m_vidfrmQ.push_back(enqfrm);
                    }
                    av_frame_unref(&avfrm);
                    avfrmLoaded = false;
                    idleLoop = false;
                }
            } while (hasOutput && !m_quitScan);
            if (quitLoop)
                break;

            // input packet to decoder
            if (!inputEof)
            {
                if (!m_vidpktQ.empty())
                {
                    AVPacket* avpkt = m_vidpktQ.front();
                    int fferr = avcodec_send_packet(m_viddecCtx, avpkt);
                    if (fferr == 0)
                    {
                        // Log(DEBUG) << ">>> Send video packet pts=" << avpkt->pts << "(" << MillisecToString(av_rescale_q(avpkt->pts, m_vidAvStm->time_base, MILLISEC_TIMEBASE))
                        //     << "), size=" << avpkt->size << "." << endl;
                        {
                            lock_guard<mutex> lk(m_vidpktQLock);
                            m_vidpktQ.pop_front();
                        }
                        av_packet_free(&avpkt);
                        idleLoop = false;
                    }
                    else if (fferr != AVERROR(EAGAIN))
                    {
                        Log(ERROR) << "FAILED to invoke 'avcodec_send_packet'(VideoDecodeThreadProc)! return code is "
                            << fferr << "." << endl;
                        break;
                    }
                }
                else if (m_demuxEof)
                {
                    avcodec_send_packet(m_viddecCtx, nullptr);
                    inputEof = true;
                }
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        m_viddecEof = true;
        Log(DEBUG) << "Leave VideoDecodeThreadProc()." << endl;
    }

    void GenerateSsThreadProc()
    {
        Log(DEBUG) << "Enter GenerateSsThreadProc()." << endl;
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

                double ts = (double)av_rescale_q(frm->pts, m_vidAvStm->time_base, MILLISEC_TIMEBASE)/1000.;
                auto iter = find_if(m_snapshots.begin(), m_snapshots.end(), [frm](const Snapshot& ss){
                    return ss.ssFrmPts == frm->pts;
                });
                if (iter != m_snapshots.end())
                {
                    if (!m_frmCvt.ConvertImage(frm, iter->img, ts))
                        Log(ERROR) << "FAILED to convert AVFrame to ImGui::ImMat! Message is '" << m_frmCvt.GetError() << "'." << endl;
                    // else
                    //     Log(DEBUG) << "Add SS#" << iter->index << "." << endl;
                }
                else
                {
                    Log(WARN) << "Discard AVFrame with pts=" << frm->pts << "(ts=" << ts << ")!";
                }

                av_frame_free(&frm);
                idleLoop = false;
            }
            else if (m_viddecEof)
                break;

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }

        auto iter = find_if(m_snapshots.begin(), m_snapshots.end(), [](const Snapshot& ss) {
            return ss.ssFrmPts == INT64_MIN;
        });
        if (iter != m_snapshots.begin())
        {
            auto iter2 = iter;
            iter2--;
            while (iter != m_snapshots.end())
            {
                iter->sameFrame = true;
                iter->sameAsIndex = iter2->sameFrame ? iter2->sameAsIndex : iter2->index;
                iter++;
                iter2++;
            }
        }
        else
        {
            iter++;
            while (iter != m_snapshots.end())
            {
                iter->sameFrame = true;
                iter->sameAsIndex = 0;
                iter++;
            }
        }
        m_genSsEof = true;
        Log(DEBUG) << "Leave GenerateSsThreadProc()." << endl;
    }

private:
    bool m_opened{false};
    string m_errMsg;
    bool m_vidPreferUseHw{true};
    AVHWDeviceType m_vidUseHwType{AV_HWDEVICE_TYPE_NONE};

    MediaParserHolder m_hParser;
    MediaInfo::InfoHolder m_hMediaInfo;

    AVFormatContext* m_avfmtCtx{nullptr};
    bool m_prepared{false};
    int m_vidStmIdx{-1};
    int m_audStmIdx{-1};
    AVStream* m_vidAvStm{nullptr};
    AVStream* m_audAvStm{nullptr};
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
    list<AVPacket*> m_vidpktQ;
    int m_vidpktQMaxSize{8};
    mutex m_vidpktQLock;
    list<AVPacket*> m_audpktQ;
    int m_audpktQMaxSize{64};
    mutex m_audpktQLock;
    bool m_demuxEof{false};
    // video decoding thread
    thread m_viddecThread;
    list<AVFrame*> m_vidfrmQ;
    int m_vidfrmQMaxSize{4};
    mutex m_vidfrmQLock;
    bool m_viddecEof{false};
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
    thread m_GenSsThread;
    bool m_genSsEof;

    recursive_mutex m_apiLock;
    bool m_quitScan{false};

    vector<Snapshot> m_snapshots;
    uint32_t m_ssCount;

    int64_t m_vidStartMts{0};
    int64_t m_vidDurMts{0};
    int64_t m_vidFrmCnt{0};
    double m_ssIntvMts;

    bool m_useRszFactor{false};
    bool m_ssSizeChanged{false};
    float m_ssWFacotr{1.f}, m_ssHFacotr{1.f};
    AVFrameToImMatConverter m_frmCvt;
};

static AVPixelFormat get_hw_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{
    MediaOverview_Impl* ms = reinterpret_cast<MediaOverview_Impl*>(ctx->opaque);
    const AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (ms->CheckHwPixFmt(*p))
            return *p;
    }
    return AV_PIX_FMT_NONE;
}

MediaOverview* CreateMediaOverview()
{
    return new MediaOverview_Impl();
}

void ReleaseMediaOverview(MediaOverview** msrc)
{
    if (msrc == nullptr || *msrc == nullptr)
        return;
    MediaOverview_Impl* movr = dynamic_cast<MediaOverview_Impl*>(*msrc);
    movr->Close();
    delete movr;
    *msrc = nullptr;
}
