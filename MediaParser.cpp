#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <list>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include "MediaParser.h"
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
using std::placeholders::_1;

class MediaParser_Impl : public MediaParser
{
public:
    static ALogger* s_logger;

    MediaParser_Impl()
    {
        m_logger = GetMediaParserLogger();
        m_taskThread = thread(&MediaParser_Impl::TaskThreadProc, this);
    }

    MediaParser_Impl(const MediaParser_Impl&) = delete;
    MediaParser_Impl(MediaParser_Impl&&) = delete;
    MediaParser_Impl& operator=(const MediaParser_Impl&) = delete;

    virtual ~MediaParser_Impl()
    {
        m_quitTaskThread = true;
        {
            lock_guard<mutex> lk(m_pendingTaskQLock);
            if (m_currTask)
                m_currTask->cancel = true;
        }
        if (m_taskThread.joinable())
            m_taskThread.join();
        Close();
    }

    bool Open(const string& url) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);

        if (m_url == url)
            return true;

        if (IsOpened())
            Close();

        int fferr = avformat_open_input(&m_avfmtCtx, url.c_str(), nullptr, nullptr);
        if (fferr < 0)
        {
            m_avfmtCtx = nullptr;
            m_errMsg = FFapiFailureMessage("avformat_open_input", fferr);
            return false;
        }
        m_url = url;
        // avformat_find_stream_info(m_avfmtCtx, nullptr);

        TaskHolder hTask(new ParseTask());
        hTask->taskProc = bind(&MediaParser_Impl::ParseGeneralMediaInfo, this, _1);
        {
            lock_guard<mutex> lk(m_taskTableLock);
            m_taskTable[MEDIA_INFO] = hTask;
        }
        {
            lock_guard<mutex> lk(m_pendingTaskQLock);
            m_pendingTaskQ.push_back(hTask);
        }

        m_opened = true;
        return true;
    }

    void Close() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);

        {
            lock_guard<mutex> lk(m_pendingTaskQLock);
            m_pendingTaskQ.clear();
            if (m_currTask)
                m_currTask->cancel = true;
        }
        {
            lock_guard<mutex> lk(m_taskTableLock);
            m_taskTable.clear();
        }
        if (m_avfmtCtx)
        {
            avformat_close_input(&m_avfmtCtx);
            m_avfmtCtx = nullptr;
        }

        m_hMediaInfo = nullptr;
        m_hVidSeekPoints = nullptr;

        m_url = "";
        m_errMsg = "";
        m_opened = false;
    }

    bool EnableParseInfo(InfoType infoType) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);

        TaskHolder hTask;
        {
            lock_guard<mutex> lk(m_taskTableLock);
            auto iter = m_taskTable.find(infoType);
            if (iter == m_taskTable.end())
            {
                hTask = TaskHolder(new ParseTask());
                switch (infoType)
                {
                    case MEDIA_INFO:
                        hTask->taskProc = bind(&MediaParser_Impl::ParseGeneralMediaInfo, this, _1);
                        break;
                    case VIDEO_SEEK_POINTS:
                        hTask->taskProc = bind(&MediaParser_Impl::ParseVideoSeekPoints, this, _1);
                        break;
                    default:
                        m_errMsg = string("Invalid argument value! There is no method to parse 'infoType'(")
                            +to_string((int)infoType)+").";
                        return false;
                }
                m_taskTable[infoType] = hTask;
            }
        }
        if (hTask)
        {
            lock_guard<mutex> lk(m_pendingTaskQLock);
            m_pendingTaskQ.push_back(hTask);
        }
        return true;
    }

    bool CheckInfoReady(InfoType infoType) override
    {
        bool ready = false;
        {
            lock_guard<mutex> lk(m_taskTableLock);
            auto iter = m_taskTable.find(infoType);
            if (iter != m_taskTable.end())
            {
                auto& task = iter->second;
                ready = task->success;
            }
        }
        return ready;
    }

    string GetUrl() const override
    {
        return m_url;
    }

    MediaInfo::InfoHolder GetMediaInfo(bool wait) override
    {
        if (wait)
            WaitTaskDone(MEDIA_INFO);
        return m_hMediaInfo;
    }

    bool HasVideo() override
    {
        WaitTaskDone(MEDIA_INFO);
        return m_bestVidStmIdx >= 0;
    }

    bool HasAudio() override
    {
        WaitTaskDone(MEDIA_INFO);
        return m_bestAudStmIdx >= 0;
    }

    int GetBestVideoStreamIndex() override
    {
        WaitTaskDone(MEDIA_INFO);
        return m_bestVidStmIdx;
    }

    int GetBestAudioStreamIndex() override
    {
        WaitTaskDone(MEDIA_INFO);
        return m_bestAudStmIdx;
    }

    MediaInfo::VideoStream* GetBestVideoStream() override
    {
        WaitTaskDone(MEDIA_INFO);
        if (m_bestVidStmIdx < 0)
            return nullptr;
        return dynamic_cast<MediaInfo::VideoStream*>(m_hMediaInfo->streams[m_bestVidStmIdx].get());
    }

    MediaInfo::AudioStream* GetBestAudioStream() override
    {
        WaitTaskDone(MEDIA_INFO);
        if (m_bestAudStmIdx < 0)
            return nullptr;
        return dynamic_cast<MediaInfo::AudioStream*>(m_hMediaInfo->streams[m_bestAudStmIdx].get());
    }

    SeekPointsHolder GetVideoSeekPoints(bool wait) override
    {
        if (wait)
            WaitTaskDone(VIDEO_SEEK_POINTS);
        return m_hVidSeekPoints;
    }

    bool IsOpened() const override
    {
        return m_opened;
    }

    string GetError() const override
    {
        return m_errMsg;
    }

private:
    struct ParseTask;
    using TaskHolder = shared_ptr<ParseTask>;

    struct ParseTask
    {
        function<bool(TaskHolder)> taskProc;
        bool cancel{false};
        bool failed{false};
        bool success{false};
        string errMsg;

        bool isDone() const
        { return cancel || failed || success; }
    };

    string FFapiFailureMessage(const string& apiName, int fferr)
    {
        ostringstream oss;
        oss << "FF api '" << apiName << "' returns error! fferr=" << fferr << ".";
        return oss.str();
    }

    void TaskThreadProc()
    {
        while (!m_quitTaskThread)
        {
            bool idleLoop = true;
            if (!m_pendingTaskQ.empty())
            {
                lock_guard<mutex> lk(m_pendingTaskQLock);
                if (!m_pendingTaskQ.empty())
                {
                    m_currTask = m_pendingTaskQ.front();
                    m_pendingTaskQ.pop_front();
                }
            }

            if (m_currTask)
            {
                if (!m_currTask->taskProc(m_currTask))
                    m_currTask->failed = true;
                else if (!m_currTask->cancel)
                    m_currTask->success = true;
                else
                    m_logger->Log(DEBUG) << "Task cancelled." << endl;
                idleLoop = false;

                {
                    lock_guard<mutex> lk(m_pendingTaskQLock);
                    m_currTask = nullptr;
                }
                m_taskDoneCv.notify_all();
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
    }

    bool ParseGeneralMediaInfo(TaskHolder hTask)
    {
        int fferr;
        fferr = avformat_find_stream_info(m_avfmtCtx, nullptr);
        if (fferr < 0)
        {
            hTask->errMsg = FFapiFailureMessage("avformat_find_stream_info", fferr);
            return false;
        }

        m_hMediaInfo = GenerateMediaInfoByAVFormatContext(m_avfmtCtx);
        m_bestVidStmIdx = av_find_best_stream(m_avfmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        m_bestAudStmIdx = av_find_best_stream(m_avfmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        m_logger->Log(DEBUG) << "Parse general media info done." << endl;
        return true;
    }

    bool ParseVideoSeekPoints(TaskHolder hTask)
    {
        if (m_bestVidStmIdx < 0)
        {
            hTask->errMsg = "No video stream found!";
            return false;
        }

        // find the 1st key frame pts
        int vidstmidx = m_bestVidStmIdx;
        AVStream* vidStream = m_avfmtCtx->streams[vidstmidx];
        int fferr;
        fferr = avformat_seek_file(m_avfmtCtx, vidstmidx, INT64_MIN, vidStream->start_time, vidStream->start_time, 0);
        if (fferr < 0)
        {
            hTask->errMsg = FFapiFailureMessage("avformat_seek_file", fferr);
            return false;
        }

        list<int64_t> vidSeekPoints;
        int64_t lastKeyPts;
        int64_t searchStart;
        int64_t searchEnd = vidStream->start_time+vidStream->duration;
        int64_t ptsStep = av_rescale_q((int64_t)(m_minSpIntervalSec*1000000), MICROSEC_TIMEBASE, vidStream->time_base);
        AVPacket avpkt = {0};
        do {
            fferr = av_read_frame(m_avfmtCtx, &avpkt);
            if (fferr == 0)
            {
                if (avpkt.stream_index == vidstmidx)
                {
                    lastKeyPts = avpkt.pts;
                    vidSeekPoints.push_back(lastKeyPts);
                    searchStart = lastKeyPts+ptsStep;
                    av_packet_unref(&avpkt);
                    break;
                }
                av_packet_unref(&avpkt);
            }
        } while (fferr >= 0 && !hTask->cancel);
        if (vidSeekPoints.empty())
        {
            hTask->errMsg = "No key-frame is found!";
            return false;
        }

        // find the following key frames
        if (searchStart < vidStream->start_time) searchStart = vidStream->start_time;
        while (!hTask->cancel)
        {
            fferr = avformat_seek_file(m_avfmtCtx, vidstmidx, searchStart, searchStart, INT64_MAX, 0);
            if (fferr < 0)
            {
                if (fferr != AVERROR(EPERM))
                {
                    hTask->errMsg = FFapiFailureMessage("avformat_seek_file", fferr);
                    return false;
                }
                break;
            }
            AVPacket avpkt = {0};
            do {
                fferr = av_read_frame(m_avfmtCtx, &avpkt);
                if (fferr == 0)
                {
                    if (avpkt.stream_index == vidstmidx)
                    {
                        if (avpkt.pts >= searchStart)
                        {
                            lastKeyPts = avpkt.pts;
                            searchStart = lastKeyPts+ptsStep;
                        }
                        else
                        {
                            m_logger->Log(WARN) << "'avformat_seek_file' does NOT function NORMAL! Return packet pts(" << avpkt.pts
                                << ") is smaller than 'searchStart' pts(" << searchStart << ")." << endl;
                            searchStart += ptsStep;
                            fferr = AVERROR(EAGAIN);
                        }

                        av_packet_unref(&avpkt);
                        break;
                    }
                    av_packet_unref(&avpkt);
                }
            } while (fferr >= 0 && !hTask->cancel);
            if (fferr == 0)
            {
                vidSeekPoints.push_back(lastKeyPts);
            }
            else if (fferr != AVERROR(EAGAIN))
            {
                if (fferr != AVERROR_EOF)
                {
                    hTask->errMsg = FFapiFailureMessage("av_read_frame", fferr);
                    return false;
                }
                break;
            }
            else if (searchStart > searchEnd)
            {
                m_logger->Log(WARN) << "[SeekPointsParsing] searchStart(" << searchStart << ") > searchEnd(" << searchEnd << ")! Quit parsing loop." << endl;
                break;
            }
        }

        SeekPointsHolder hSeekPoints(new vector<int64_t>());
        hSeekPoints->reserve(vidSeekPoints.size());
        for (int64_t pts : vidSeekPoints)
            hSeekPoints->push_back(pts);
        m_hVidSeekPoints = hSeekPoints;
        m_logger->Log(DEBUG) << "Parse video seek points done. " << vidSeekPoints.size() << " seek points are found." << endl;
        return true;
    }

    bool ResetAVFormatContext(TaskHolder hTask)
    {
        int fferr = avformat_seek_file(m_avfmtCtx, -1, INT64_MIN, m_avfmtCtx->start_time, m_avfmtCtx->start_time, 0);
        if (fferr < 0)
        {
            if (hTask)
                hTask->errMsg = FFapiFailureMessage("avformat_seek_file", fferr);
            return false;
        }
        return true;
    }

    void WaitTaskDone(InfoType type)
    {
        TaskHolder hTask;
        {
            lock_guard<mutex> lk(m_taskTableLock);
            auto iter = m_taskTable.find(type);
            hTask = iter->second;
        }
        if (!hTask)
            return;
        {
            unique_lock<mutex> lk(m_pendingTaskQLock);
            m_taskDoneCv.wait(lk, [hTask]() { return hTask->isDone(); });
        }
    }

private:
    ALogger* m_logger;
    thread m_taskThread;
    TaskHolder m_currTask;
    bool m_quitTaskThread{false};
    list<TaskHolder> m_pendingTaskQ;
    mutex m_pendingTaskQLock;
    condition_variable m_taskDoneCv;
    unordered_map<InfoType, TaskHolder> m_taskTable;
    mutex m_taskTableLock;

    recursive_mutex m_apiLock;
    bool m_opened{false};

    MediaInfo::InfoHolder m_hMediaInfo;
    int m_bestVidStmIdx{-1};
    int m_bestAudStmIdx{-1};

    SeekPointsHolder m_hVidSeekPoints;
    double m_minSpIntervalSec{2};

    string m_url;
    AVFormatContext* m_avfmtCtx{nullptr};

    string m_errMsg;
};

ALogger* MediaParser_Impl::s_logger;

MediaParserHolder CreateMediaParser()
{
    MediaParserHolder hParser(new MediaParser_Impl());
    return hParser;
}

ALogger* GetMediaParserLogger()
{
    if (!MediaParser_Impl::s_logger)
        MediaParser_Impl::s_logger = GetLogger("MParser");
    return MediaParser_Impl::s_logger;
}