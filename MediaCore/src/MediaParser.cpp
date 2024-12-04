/*
    Copyright (c) 2023-2024 CodeWin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <list>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include "ThreadUtils.h"
#include "MediaParser.h"
#include "FFUtils.h"
extern "C"
{
    #include "libavutil/avutil.h"
    #include "libavutil/avstring.h"
    #include "libavutil/pixdesc.h"
    #include "libavutil/display.h"
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

namespace MediaCore
{
class MediaParser_Impl : public MediaParser
{
public:
    MediaParser_Impl()
    {
        m_logger = MediaParser::GetLogger();
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

    bool OpenImageSequence(const Ratio& frameRate,
            const std::string& dirPath, const std::string& regexPattern, bool caseSensitive, bool includeSubDir) override
    {
        if (!Ratio::IsValid(frameRate))
        {
            ostringstream oss; oss << "INVALID argument 'frameRate'! Ratio " << frameRate << " is not valid.";
            m_errMsg = oss.str();
            return false;
        }
        lock_guard<recursive_mutex> lk(m_apiLock);
        m_hFileIter = SysUtils::FileIterator::CreateInstance(dirPath);
        if (!m_hFileIter)
        {
            ostringstream oss; oss << "INVALID argument 'dirPath'! '" << dirPath << "' is NOT a DIRECTORY.";
            m_errMsg = oss.str();
            return false;
        }
        m_hFileIter->SetCaseSensitive(caseSensitive);
        m_hFileIter->SetFilterPattern(regexPattern, true);
        m_hFileIter->SetRecursive(includeSubDir);
        m_hFileIter->StartParsing();
        m_url = dirPath;

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

        m_imgsqFrameRate = frameRate;
        m_isImageSequence = true;
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
        m_isImageSequence = false;
        m_hFileIter = nullptr;
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
                ostringstream oss;
                switch (infoType)
                {
                    case MEDIA_INFO:
                        hTask->taskProc = bind(&MediaParser_Impl::ParseGeneralMediaInfo, this, _1);
                        break;
                    case VIDEO_SEEK_POINTS:
                        if (!m_isImageSequence)
                            hTask->taskProc = bind(&MediaParser_Impl::ParseVideoSeekPoints, this, _1);
                        else
                        {
                            m_errMsg = "Image sequence do NOT support parsing seek-points!";
                            return false;
                        }
                        break;
                    default:
                        oss << "Invalid argument value! There is no method to parse 'infoType'(" << to_string((int)infoType) << ").";
                        m_errMsg = oss.str();
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

    MediaInfo::Holder GetMediaInfo(bool wait) override
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

    VideoStream* GetBestVideoStream() override
    {
        WaitTaskDone(MEDIA_INFO);
        if (m_bestVidStmIdx < 0)
            return nullptr;
        return dynamic_cast<VideoStream*>(m_hMediaInfo->streams[m_bestVidStmIdx].get());
    }

    AudioStream* GetBestAudioStream() override
    {
        WaitTaskDone(MEDIA_INFO);
        if (m_bestAudStmIdx < 0)
            return nullptr;
        return dynamic_cast<AudioStream*>(m_hMediaInfo->streams[m_bestAudStmIdx].get());
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

    bool IsImageSequence() const override
    {
        return m_isImageSequence;
    }

    SysUtils::FileIterator::Holder GetImageSequenceIterator() const override
    {
        return m_hFileIter;
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
                this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_TIME));
        }
    }

    bool ParseGeneralMediaInfo(TaskHolder hTask)
    {
        if (m_isImageSequence)
            return ParseMediaInfoFromImageSequence(hTask);
        else
            return ParseMediaInfoFromFile(hTask);
    }

    bool ParseMediaInfoFromFile(TaskHolder hTask)
    {
        string fileName = SysUtils::ExtractFileName(m_url);
        ostringstream thnOss;
        thnOss << "PsrTsk-" << fileName;
        SysUtils::SetThreadName(m_taskThread, thnOss.str());

        int fferr = 0;
        fferr = av_opt_set_int(m_avfmtCtx, "probesize", 5000, 0);
        if (fferr < 0)
            m_logger->Log(Error) << "FAILED to set option 'probesize' to m_avfmtCtx! fferr=" << fferr << "." << endl;
        fferr = avformat_find_stream_info(m_avfmtCtx, nullptr);
        if (fferr < 0)
        {
            hTask->errMsg = FFapiFailureMessage("avformat_find_stream_info", fferr);
            return false;
        }

        m_hMediaInfo = GenerateMediaInfoByAVFormatContext(m_avfmtCtx);
        if (!m_hMediaInfo->isComplete)
        {
            m_logger->Log(INFO) << "MediaInfo is NOT COMPLETE. Try to parse the media again with LARGER probe size." << endl;
            AVFormatContext* avfmtCtx = nullptr;
            fferr = avformat_open_input(&avfmtCtx, m_url.c_str(), nullptr, nullptr);
            if (fferr < 0)
            {
                m_logger->Log(WARN) << "FAILED to open media '" << m_url << "' again!" << endl;
            }
            else
            {
                m_logger->Log(INFO) << "Increase 'probesize' to 5000000." << endl;
                av_opt_set_int(avfmtCtx, "probesize", 5000000, 0);
                fferr = avformat_find_stream_info(avfmtCtx, nullptr);
                if (fferr < 0)
                {
                    avformat_close_input(&avfmtCtx);
                    hTask->errMsg = FFapiFailureMessage("avformat_find_stream_info", fferr);
                    return false;
                }
                m_hMediaInfo = GenerateMediaInfoByAVFormatContext(avfmtCtx);

                lock_guard<recursive_mutex> lk(m_apiLock);
                avformat_close_input(&m_avfmtCtx);
                m_avfmtCtx = avfmtCtx;
            }
        }
        m_bestVidStmIdx = av_find_best_stream(m_avfmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        m_bestAudStmIdx = av_find_best_stream(m_avfmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

        // pre-decode mjpeg and parse exif metadata
        if (m_bestVidStmIdx >= 0 && m_avfmtCtx->streams[m_bestVidStmIdx]->codecpar->codec_id == AV_CODEC_ID_MJPEG)
        {
            FFUtils::OpenVideoDecoderOptions tVidDecOpenOpts;
            FFUtils::OpenVideoDecoderResult tVidDecOpenRes;
            if (FFUtils::OpenVideoDecoder(m_avfmtCtx, m_bestVidStmIdx, &tVidDecOpenOpts, &tVidDecOpenRes, true))
            {
                auto probeFrame = tVidDecOpenRes.probeFrame;
                if (probeFrame)
                {
                    auto pSideData = av_frame_get_side_data(probeFrame.get(), AV_FRAME_DATA_DISPLAYMATRIX);
                    if (pSideData && pSideData->data && pSideData->size >= 9*4)
                    {
                        auto pVidstm = dynamic_cast<VideoStream*>(m_hMediaInfo->streams[m_bestVidStmIdx].get());
                        pVidstm->displayRotation = av_display_rotation_get((const int32_t*)pSideData->data);
                        if (pVidstm->displayRotation != 0)
                        {
                            const double dTimesTo90 = pVidstm->displayRotation/90.0;
                            double integ_;
                            const double frac = modf(dTimesTo90, &integ_);
                            const int integ = (int)integ_;
                            if (frac == 0.0 && ((integ&0x1) == 1))
                            {
                                pVidstm->width = pVidstm->rawHeight;
                                pVidstm->height = pVidstm->rawWidth;
                            }
                        }
                    }
                }
            }
        }
        m_logger->Log(INFO) << "Parse general media info of media '" << m_url << "' done." << endl;
        return true;
    }

    bool ParseMediaInfoFromImageSequence(TaskHolder hTask)
    {
        string dirPath = m_hFileIter->GetBaseDirPath();
        ostringstream thnOss;
        thnOss << "PsrTskIs-" << dirPath;
        SysUtils::SetThreadName(m_taskThread, thnOss.str());

        m_hMediaInfo = MediaInfo::Holder(new MediaInfo());
        m_hMediaInfo->url = m_url;
        auto filePath = m_hFileIter->GetQuickSample();
        if (!filePath.empty())
        {
            auto fullPath = m_hFileIter->JoinBaseDirPath(filePath);
            int fferr = avformat_open_input(&m_avfmtCtx, fullPath.c_str(), nullptr, nullptr);
            if (fferr < 0)
            {
                m_avfmtCtx = nullptr;
                m_errMsg = FFapiFailureMessage("avformat_open_input", fferr);
                return false;
            }
            fferr = avformat_find_stream_info(m_avfmtCtx, nullptr);
            if (fferr < 0)
            {
                hTask->errMsg = FFapiFailureMessage("avformat_find_stream_info", fferr);
                return false;
            }
            auto hMediaInfo = GenerateMediaInfoByAVFormatContext(m_avfmtCtx);
            m_bestVidStmIdx = av_find_best_stream(m_avfmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
            if (m_bestVidStmIdx >= 0)
            {
                m_hMediaInfo->streams.push_back(hMediaInfo->streams[m_bestVidStmIdx]);
                m_bestVidStmIdx = 0;
                auto vidstm = dynamic_cast<VideoStream*>(m_hMediaInfo->streams[0].get());
                vidstm->avgFrameRate = vidstm->realFrameRate = m_imgsqFrameRate;
                vidstm->isImage = false;
                vidstm->frameNum = m_hFileIter->GetValidFileCount();
                vidstm->duration = (double)vidstm->frameNum*(double)m_imgsqFrameRate.den/m_imgsqFrameRate.num;
                m_hMediaInfo->duration = vidstm->duration;
            }
            else
            {
                m_hMediaInfo->duration = 0;
            }
            m_hMediaInfo->startTime = 0;
            m_hMediaInfo->isComplete = true;
            m_logger->Log(INFO) << "Parse general media info of media '" << fullPath << "' done." << endl;
        }

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
                            m_logger->Log(DEBUG) << "Parsing file '" << m_url << "', returned packet pts("
                                    << avpkt.pts << ") is smaller than 'searchStart' pts(" << searchStart << ")." << endl;
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
        m_logger->Log(INFO) << "Parse video seek points of media '" << m_url << "' done. " << vidSeekPoints.size() << " seek points are found." << endl;
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
            if (iter != m_taskTable.end())
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

    MediaInfo::Holder m_hMediaInfo;
    int m_bestVidStmIdx{-1};
    int m_bestAudStmIdx{-1};

    SeekPointsHolder m_hVidSeekPoints;
    double m_minSpIntervalSec{2};

    SysUtils::FileIterator::Holder m_hFileIter;
    bool m_isImageSequence{false};
    Ratio m_imgsqFrameRate;

    string m_url;
    AVFormatContext* m_avfmtCtx{nullptr};

    string m_errMsg;
};

static const auto MEDIA_PARSER_HOLDER_DELETER = [] (MediaParser* p) {
    MediaParser_Impl* ptr = dynamic_cast<MediaParser_Impl*>(p);
    delete ptr;
};

MediaParser::Holder MediaParser::CreateInstance()
{
    return MediaParser::Holder(new MediaParser_Impl(), MEDIA_PARSER_HOLDER_DELETER);
}

ALogger* MediaParser::GetLogger()
{
    return Logger::GetLogger("MParser");
}
}