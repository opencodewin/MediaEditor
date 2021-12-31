#include <thread>
#include <mutex>
#include <sstream>
#include <atomic>
#include "MediaReader.h"
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

class MediaReader_Impl : public MediaReader
{
public:
    MediaReader_Impl() = default;
    MediaReader_Impl(const MediaReader_Impl&) = delete;
    MediaReader_Impl(MediaReader_Impl&&) = delete;
    MediaReader_Impl& operator=(const MediaReader_Impl&) = delete;

    virtual ~MediaReader_Impl() {}

    bool Open(const std::string& url)
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

    bool Open(MediaParserHolder hParser)
    {

    }

    MediaParserHolder GetMediaParser() const
    {

    }

    void Close()
    {

    }

    void SeekTo(double ts)
    {

    }

    void SetDirection(bool forward)
    {

    }

    bool ReadFrame(double ts, ImGui::ImMat& m)
    {

    }

    bool IsOpened() const
    {

    }

    bool HasVideo() const
    {

    }

    bool HasAudio() const
    {

    }

    bool SetSnapshotSize(uint32_t width, uint32_t height)
    {

    }

    bool SetSnapshotResizeFactor(float widthFactor, float heightFactor)
    {

    }

    bool SetOutColorFormat(ImColorFormat clrfmt)
    {

    }

    bool SetResizeInterpolateMode(ImInterpolateMode interp)
    {

    }

    MediaInfo::InfoHolder GetMediaInfo() const
    {

    }

    const MediaInfo::VideoStream* GetVideoStream() const
    {

    }

    const MediaInfo::AudioStream* GetAudioStream() const
    {

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
        return av_rescale_q(pts-m_vidStream->start_time, m_vidStream->time_base, MILLISEC_TIMEBASE);
    }

    int64_t CvtVidMtsToPts(int64_t mts)
    {
        return av_rescale_q(mts, MILLISEC_TIMEBASE, m_vidStream->time_base)+m_vidStream->start_time;
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

        MediaInfo::VideoStream* vidStream = dynamic_cast<MediaInfo::VideoStream*>(m_hMediaInfo->streams[m_vidStmIdx].get());
        // m_vidStartMts = (int64_t)(vidStream->startTime*1000);
        m_vidDurTs = vidStream->duration;
        // m_vidFrmCnt = vidStream->frameNum;
        AVRational avgFrmRate = { vidStream->avgFrameRate.num, vidStream->avgFrameRate.den };
        AVRational timebase = { vidStream->timebase.num, vidStream->timebase.den };
        m_vidfrmIntvMts = av_q2d(av_inv_q(avgFrmRate))*1000.;
        // m_vidfrmIntvMtsHalf = ceil(m_vidfrmIntvMts)/2;
        // if (avgFrmRate.num*avgFrmRate.num > 0)
        //     m_vidfrmIntvPts = (avgFrmRate.den*timebase.den)/(avgFrmRate.num*timebase.num);
        // else
        //     m_vidfrmIntvPts = 0;

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
        }

        if (HasAudio())
        {
            m_audStream = m_avfmtCtx->streams[m_audStmIdx];

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

    struct VideoFrame
    {
        AVFrame* avfrm{nullptr};
        ImGui::ImMat img;
        double ts;
    };

    struct CacheWindow
    {
        double currPos;
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
            {
                if (vf.avfrm)
                {
                    av_frame_free(&vf.avfrm);
                    outterObj.m_pendingVidfrmCnt--;
                }
            }
        }

        MediaReader_Impl& outterObj;
        pair<int64_t, int64_t> seekPts;
        list<VideoFrame> vfAry;
        atomic_int32_t frmCnt{0};
        list<AVPacket*> avpktQ;
        mutex avpktQLock;
        bool demuxing{false};
        bool demuxerEof{false};
        bool decoding{false};
        bool decoderEof{false};
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
        Log(DEBUG) << "Enter DemuxThreadProc()..." << endl;

        if (!m_prepared && !Prepare())
        {
            Log(ERROR) << "Prepare() FAILED! Error is '" << m_errMsg << "'." << endl;
            return;
        }

        AVPacket avpkt = {0};
        bool avpktLoaded = false;
        GopDecodeTaskHolder currTask = nullptr;
        // int64_t lastPktPts;
        // int32_t currPktSsIdx;
        // uint32_t gopSmallestIdx;
        // bool currPktChecked;
        bool demuxEof = false;
        while (!m_quit)
        {
            bool idleLoop = true;

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
                        Log(DEBUG) << "--> Change demux task, startPts=" 
                            << currTask->seekPts.first << "(" << MillisecToString(CvtVidPtsToMts(currTask->seekPts.first)) << ")"
                            << ", endPts=" << currTask->seekPts.second << "(" << MillisecToString(CvtVidPtsToMts(currTask->seekPts.second)) << ")" << endl;
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
                                break;
                            }
                            demuxEof = false;
                            int64_t ptsAfterSeek = INT64_MIN;
                            if (!ReadNextStreamPacket(m_vidStmIdx, &avpkt, &avpktLoaded, &ptsAfterSeek))
                                break;
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
                            // currPktChecked = false;
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
                            {
                                m_errMsg = FFapiFailureMessage("av_read_frame", fferr);
                                Log(ERROR) << "Demuxer ERROR! 'av_read_frame' returns " << fferr << "." << endl;
                            }
                        }
                    }

                    if (avpktLoaded)
                    {
                        if (avpkt.stream_index == m_vidStmIdx)
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
        double ts = (double)CvtVidPtsToMts(frm->pts)/1000;
        lock_guard<mutex> lk(m_bldtskByPriLock);
        auto iter = find_if(m_bldtskPriOrder.begin(), m_bldtskPriOrder.end(), [frm](const GopDecodeTaskHolder& task) {
            return frm->pts >= task->seekPts.first && frm->pts < task->seekPts.second;
        });
        if (iter != m_bldtskPriOrder.end())
        {
            VideoFrame vf;
            vf.ts = ts;
            vf.avfrm = av_frame_clone(frm);
            if (!vf.avfrm)
            {
                Log(ERROR) << "FAILED to invoke 'av_frame_clone()' to allocate new AVFrame for VF!" << endl;
                return false;
            }
            // Log(DEBUG) << "Adding VF#" << ts << "." << endl;
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
                    Log(DEBUG) << "Found duplicated VF#" << ts << ", dropping this VF. pts=" << frm->pts
                        << ", t=" << MillisecToString(CvtVidPtsToMts(frm->pts)) << "." << endl;
                    if (vf.avfrm)
                        av_frame_free(&vf.avfrm);
                }
                else
                {
                    auto vfFwdIter = vfRvsIter.base();
                    task->vfAry.insert(vfFwdIter, vf);
                    task->frmCnt++;
                    m_pendingVidfrmCnt++;
                }
            }
            return true;
        }
        else
        {
            Log(DEBUG) << "Dropping VF#" << ts << " due to no matching task is found." << endl;
        }
        return false;
    }

    void VideoDecodeThreadProc()
    {
        Log(DEBUG) << "Enter VideoDecodeThreadProc()..." << endl;

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
                GopDecodeTaskHolder oldTask = currTask;
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
                                m_errMsg = FFapiFailureMessage("avcodec_receive_frame", fferr);
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
                        if (m_pendingVidfrmCnt < m_maxPendingVidfrmCnt)
                        {
                            EnqueueSnapshotAVFrame(&avfrm);
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
                            m_errMsg = FFapiFailureMessage("avcodec_send_packet", fferr);
                            Log(ERROR) << "FAILED to invoke 'avcodec_send_packet'(VideoDecodeThreadProc)! return code is "
                                << fferr << "." << endl;
                            break;
                        }
                    }
                    else if (currTask->demuxerEof)
                    {
                        currTask->decoderEof = true;
                        idleLoop = false;
                    }
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

    GopDecodeTaskHolder FindNextSsUpdateTask()
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

    void UpdateSnapshotThreadProc()
    {
        Log(DEBUG) << "Enter UpdateSnapshotThreadProc()." << endl;
        GopDecodeTaskHolder currTask;
        while (!m_quit)
        {
            bool idleLoop = true;

            if (!currTask || currTask->cancel || currTask->frmCnt <= 0)
            {
                currTask = FindNextSsUpdateTask();
            }

            if (currTask)
            {
                AVFrame* ssfrm = nullptr;
                for (VideoFrame& vf : currTask->vfAry)
                {
                    if (vf.avfrm)
                    {
                        double ts = (double)CvtVidPtsToMts(vf.avfrm->pts)/1000.;
                        if (!m_frmCvt.ConvertImage(vf.avfrm, vf.img, ts))
                            Log(ERROR) << "FAILED to convert AVFrame to ImGui::ImMat! Message is '" << m_frmCvt.GetError() << "'." << endl;
                        av_frame_free(&vf.avfrm);
                        vf.avfrm = nullptr;
                        currTask->frmCnt--;
                        if (currTask->frmCnt < 0)
                            Log(ERROR) << "!! ABNORMAL !! Task [" << currTask->seekPts.first << ", " << currTask->seekPts.second << "] has negative 'frmCnt'("
                                << currTask->frmCnt << ")!" << endl;
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

    pair<int64_t, int64_t> GetSeekPosByTs(double ts)
    {
        int64_t targetPts = CvtVidMtsToPts((int64_t)(ts*1000));
        auto iter = find_if(m_hSeekPoints->begin(), m_hSeekPoints->end(),
            [targetPts](int64_t keyPts) { return keyPts > targetPts; });
        if (iter != m_hSeekPoints->begin())
            iter--;
        int64_t first = *iter++;
        int64_t second = iter == m_hSeekPoints->end() ? INT64_MAX : *iter;
        return { first, second };
    }

    void UpdateCacheWindow(double readPos)
    {
        if (readPos == m_cacheWnd.currPos)
            return;
        const double beforeCacheDur = m_readForward ? m_backwardCacheDur : m_forwardCacheDur;
        const double afterCacheDur = m_readForward ? m_forwardCacheDur : m_backwardCacheDur;
        const double cacheBeginTs = readPos > beforeCacheDur ? readPos-beforeCacheDur : 0;
        const double cacheEndTs = readPos+afterCacheDur < m_vidDurTs ? readPos+afterCacheDur : m_vidDurTs;
        const int64_t seekPosRead = GetSeekPosByTs(readPos).first;
        const int64_t seekPos00 = GetSeekPosByTs(cacheBeginTs).first;
        const int64_t seekPos10 = GetSeekPosByTs(cacheEndTs).first;
        CacheWindow cacheWnd = m_cacheWnd;
        if (seekPosRead != cacheWnd.seekPosShow || seekPos00 != cacheWnd.seekPos00 || seekPos10 != cacheWnd.seekPos10)
        {
            m_cacheWnd = { readPos, cacheBeginTs, cacheEndTs, seekPosRead, seekPos00, seekPos10 };
            m_needUpdateBldtsk = true;
        }
        m_cacheWnd.currPos = readPos;
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

        int64_t searchPts = CvtVidMtsToPts((int64_t)(currwnd.cacheBeginTs*1000));
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
        } while (searchPts < INT64_MAX && (double)CvtVidPtsToMts(searchPts)/1000 <= currwnd.cacheEndTs);
        m_bldtskSnapWnd = currwnd;
        Log(DEBUG) << "^^^ Initialized build task, pos = " << TimestampToString(m_bldtskSnapWnd.currPos) << ", window = ["
            << TimestampToString(m_bldtskSnapWnd.cacheBeginTs) << " ~ " << TimestampToString(m_bldtskSnapWnd.cacheEndTs) << "]." << endl;

        UpdateBuildTaskByPriority();
    }

    void UpdateSnapshotBuildTask()
    {
        CacheWindow currwnd = m_cacheWnd;
        if (currwnd.cacheBeginTs != m_bldtskSnapWnd.cacheBeginTs)
        {
            Log(DEBUG) << "^^^ Updating build task, index changed from ("
                << TimestampToString(m_bldtskSnapWnd.cacheBeginTs) << " ~ " << TimestampToString(m_bldtskSnapWnd.cacheEndTs) << ") to ("
                << TimestampToString(currwnd.cacheBeginTs) << " ~ " << TimestampToString(currwnd.cacheEndTs) << ")." << endl;
            lock_guard<mutex> lk(m_bldtskByTimeLock);
            if (currwnd.cacheBeginTs > m_bldtskSnapWnd.cacheBeginTs)
            {
                int64_t beginPts = CvtVidMtsToPts((int64_t)(currwnd.cacheBeginTs*1000));
                int64_t endPts = CvtVidMtsToPts((int64_t)(currwnd.cacheEndTs*1000))+1;
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
                int64_t beginPts = CvtVidMtsToPts((int64_t)(currwnd.cacheBeginTs*1000));
                int64_t endPts = CvtVidMtsToPts((int64_t)(currwnd.cacheEndTs*1000))+1;
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
                    return (m_readForward^(a->seekPts.first < b->seekPts.first)) != 0;
            }
            else if (bIsForwardGop)
                return false;
            else
                return (m_readForward^(a->seekPts.first > b->seekPts.first)) != 0;
        });
    }

private:
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

    bool m_readForward{true};
    double m_readPosTs{0};
    double m_vidfrmIntvMts{0};
    list<GopDecodeTaskHolder> m_bldtskTimeOrder;
    mutex m_bldtskByTimeLock;
    list<GopDecodeTaskHolder> m_bldtskPriOrder;
    mutex m_bldtskByPriLock;
    atomic_int32_t m_pendingVidfrmCnt{0};
    int32_t m_maxPendingVidfrmCnt{4};
    double m_forwardCacheDur;
    double m_backwardCacheDur;
    CacheWindow m_cacheWnd;
    CacheWindow m_bldtskSnapWnd;
    bool m_needUpdateBldtsk{false};
    double m_vidDurTs{0};

    bool m_ssSizeChanged{false};
    float m_ssWFacotr{1.f}, m_ssHFacotr{1.f};
    AVFrameToImMatConverter m_frmCvt;
};