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

#include <sstream>
#include <algorithm>
#include <atomic>
#include <thread>
#include <cmath>
#include <cassert>
#include "VideoTrack.h"
#include "MediaCore.h"
#include "ThreadUtils.h"
#include "DebugHelper.h"
#include "Logger.h"

using namespace std;
using namespace Logger;

namespace MediaCore
{
class ReadFrameTask_Impl : public ReadFrameTask
{
public:
    ReadFrameTask_Impl(int64_t frameIndex, int64_t readPos, bool canDrop, bool needSeek, bool bypassBgNode)
        : m_frameIndex(frameIndex)
        , m_readPos(readPos)
        , m_canDrop(canDrop)
        , m_needSeek(needSeek)
        , m_bypassBgNode(bypassBgNode)
    {}

    int64_t FrameIndex() const override
    {
        return m_frameIndex;
    }

    int64_t ReadPos() const
    {
        return m_readPos;
    }

    bool CanDrop() const
    {
        return m_discarded || (m_canDrop && (!m_pCb || m_pCb->TriggerDrop()));
    }

    bool NeedSeek() const
    {
        return m_needSeek;
    }

    bool HasSeeked() const
    {
        return m_seeked;
    }

    void SetSeeked()
    {
        m_seeked = true;
    }

    bool IsSourceFrameReady() const override
    {
        return m_src1Ready && (!m_hasOvlp || m_src2Ready);
    }

    void StartProcessing() override
    {
        m_needProcess = true;
    }

    void Reprocess() override
    {
        m_outputReady = false;
    }

    VideoFrame::Holder GetVideoFrame() override
    {
        return m_hOutVfrm;
    }

    void SetDiscarded() override
    {
        m_discarded = true;
    }

    bool IsDiscarded() const override
    {
        return m_discarded;
    }

    bool IsOutputFrameReady() const override
    {
        return m_outputReady;
    }

    bool IsStarted() const override
    {
        return m_started;
    }

    bool Start()
    {
        if (m_discarded)
            return false;
        m_started = !m_pCb || m_pCb->TriggerStart();
        return m_started;
    }

    bool IsVisible() const override
    {
        return m_visible;
    }

    void SetVisible(bool visible) override
    {
        m_visible = visible;
    }

    void UpdateHostFrames() override
    {
        if (!m_pCb)
            return;
        unique_lock<mutex> lk(m_mtxOutFrames);
        const auto outFrames(m_outFrames);
        lk.unlock();
        m_pCb->UpdateOutputFrames(outFrames);
    }

    bool IsInited() const
    {
        return m_inited;
    }

    void Initialize(VideoClip::Holder hClip1, VideoClip::Holder hClip2, VideoOverlap::Holder hOvlp)
    {
        m_hClip1 = hClip1;
        m_hClip2 = hClip2;
        m_hasOvlp = hOvlp != nullptr;
        m_hOvlp = hOvlp;
        m_inited = true;
    }

    bool NeedProcess() const
    {
        return m_needProcess && !m_outputReady;
    }

    void SetOutputReady()
    {
        m_outputReady = true;
    }

    void DoReadSourceFrame()
    {
        if (m_hClip1)
        {
            if (!m_src1Ready)
            {
                auto clipPos = m_readPos-m_hClip1->Start();
                m_srcVf1 = m_hClip1->ReadSourceFrame(clipPos, m_eof1, false);
                if (m_srcVf1 || m_eof1)
                {
                    if (m_srcVf1)
                    {
                        lock_guard<mutex> lg(m_mtxOutFrames);
                        m_outFrames.push_back(CorrelativeVideoFrame::Holder(new CorrelativeVideoFrame(CorrelativeFrame::PHASE_SOURCE_FRAME, m_hClip1->Id(), m_hClip1->TrackId(), m_srcVf1)));
                    }
                    m_src1Ready = true;
                }
            }
        }
        else
            m_src1Ready = true;
        if (m_hClip2)
        {
            if (!m_src2Ready)
            {
                auto clipPos = m_readPos-m_hClip2->Start();
                m_srcVf2 = m_hClip2->ReadSourceFrame(clipPos, m_eof2, false);
                if (m_srcVf2 || m_eof2)
                {
                    if (m_srcVf2)
                    {
                        lock_guard<mutex> lg(m_mtxOutFrames);
                        m_outFrames.push_back(CorrelativeVideoFrame::Holder(new CorrelativeVideoFrame(CorrelativeFrame::PHASE_SOURCE_FRAME, m_hClip2->Id(), m_hClip2->TrackId(), m_srcVf2)));
                    }
                    m_src2Ready = true;
                }
            }
        }
        else
            m_src2Ready = true;
    }

    void ProcessFrame()
    {
        if (!m_visible)
        {
            m_outputReady = true;
            return;
        }
        if (!IsSourceFrameReady())
            return;
        if (!m_hClip1)
        {
            m_outputReady = true;
            return;
        }

        unordered_map<string, string> extraArgs;
        if (m_bypassBgNode)
            extraArgs["bypass_bg_node"] = "true";
        vector<CorrelativeVideoFrame::Holder> outFrames;
        VideoFrame::Holder hOutVfrm;
        if (m_hasOvlp)
        {
            m_outFrames.clear();
            hOutVfrm = m_hOvlp->ProcessSourceFrame(m_readPos-m_hOvlp->Start(), outFrames, m_srcVf1, m_srcVf2, &extraArgs);
        }
        else if (m_hClip1)
        {
            m_outFrames.clear();
            hOutVfrm = m_hClip1->ProcessSourceFrame(m_readPos-m_hClip1->Start(), outFrames, m_srcVf1, &extraArgs);
        }
        {
            lock_guard<mutex> lg(m_mtxOutFrames);
            m_outFrames.insert(m_outFrames.end(), outFrames.begin(), outFrames.end());
        }
        if (!hOutVfrm)
            return;
        ImGui::ImMat tOutMat;
        if (!hOutVfrm->GetMat(tOutMat))
            return;
        tOutMat.time_stamp = (double)m_readPos/1000;
        m_hOutVfrm = VideoFrame::CreateMatInstance(tOutMat);
        m_hOutVfrm->SetOpacity(hOutVfrm->Opacity());
        m_outputReady = true;
    }

    void SetCallback(Callback* pCallback) override
    {
        m_pCb = pCallback;
    }

private:
    int64_t m_frameIndex;
    int64_t m_readPos;
    bool m_canDrop;
    bool m_needSeek;
    bool m_bypassBgNode;
    bool m_seeked{false};
    bool m_started{false};
    bool m_inited{false};
    bool m_needProcess{false};
    bool m_visible{true};
    VideoFrame::Holder m_srcVf1;
    bool m_eof1{false};
    VideoClip::Holder m_hClip1;
    bool m_src1Ready{false};
    bool m_hasOvlp{false};
    VideoFrame::Holder m_srcVf2;
    bool m_eof2{false};
    VideoClip::Holder m_hClip2;
    bool m_src2Ready{false};
    VideoOverlap::Holder m_hOvlp;
    vector<CorrelativeVideoFrame::Holder> m_outFrames;
    mutex m_mtxOutFrames;
    VideoFrame::Holder m_hOutVfrm;
    bool m_outputReady{false};
    bool m_discarded{false};
    Callback* m_pCb{nullptr};
};

static const auto READ_FRAME_TASK_HOLDER_DELETER = [] (ReadFrameTask* p) {
    ReadFrameTask_Impl* ptr = dynamic_cast<ReadFrameTask_Impl*>(p);
    delete ptr;
};

static const auto CLIP_SORT_CMP = [] (const VideoClip::Holder& a, const VideoClip::Holder& b){
    return a->Start() < b->Start();
};

static const auto OVERLAP_SORT_CMP = [] (const VideoOverlap::Holder& a, const VideoOverlap::Holder& b) {
    return a->Start() < b->Start();
};

class VideoTrack_Impl : public VideoTrack
{
public:
    VideoTrack_Impl(int64_t id, SharedSettings::Holder hSettings)
        : m_id(id), m_hSettings(hSettings)
    {
        ostringstream loggerNameOss;
        loggerNameOss << id;
        string idstr = loggerNameOss.str();
        if (idstr.size() > 4)
            idstr = idstr.substr(idstr.size()-4);
        loggerNameOss.str(""); loggerNameOss << "VTrk#" << idstr;
        string tag = loggerNameOss.str();
        m_logger = GetLogger(tag);

        m_readClipIter = m_clips.begin();
        m_readThread = thread(&VideoTrack_Impl::ReadFrameProc, this);
        SysUtils::SetThreadName(m_readThread, tag);
    }

    ~VideoTrack_Impl()
    {
        m_quitThread = true;
        if (m_readThread.joinable())
            m_readThread.join();
        for (auto& rft : m_readFrameTasks)
            rft->SetDiscarded();
        m_readFrameTasks.clear();
    }

    Holder Clone(SharedSettings::Holder hSettings) override;

    VideoClip::Holder AddVideoClip(int64_t clipId, MediaParser::Holder hParser, int64_t start, int64_t end, int64_t startOffset, int64_t endOffset, int64_t readPos) override
    {
        VideoClip::Holder hClip;
        auto vidstream = hParser->GetBestVideoStream();
        assert(!vidstream->isImage);
        hClip = VideoClip::CreateVideoInstance(clipId, hParser, m_hSettings, start, end, startOffset, endOffset, readPos-start, m_readForward);
        InsertClip(hClip);
        return hClip;
    }

    VideoClip::Holder AddImageClip(int64_t clipId, MediaParser::Holder hParser, int64_t start, int64_t length) override
    {
        VideoClip::Holder hClip;
        auto vidstream = hParser->GetBestVideoStream();
        assert(vidstream->isImage);
        hClip = VideoClip::CreateImageInstance(clipId, hParser, m_hSettings, start, length);
        InsertClip(hClip);
        return hClip;
    }

    void InsertClip(VideoClip::Holder hClip) override
    {
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
        if (!CheckClipRangeValid(hClip->Id(), hClip->Start(), hClip->End()))
            throw invalid_argument("Invalid argument for inserting clip!");

        // hClip->SetLogLevel(DEBUG);
        // add this clip into clip list 2
        hClip->SetDirection(m_readForward);
        hClip->SetTrackId(m_id);
        m_clips2.push_back(hClip);
        if (hClip->End() > m_duration2)
            m_duration2 = hClip->End();
        UpdateClipOverlap();
        m_clipChanged = true;
    }

    void MoveClip(int64_t id, int64_t start) override
    {
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
        VideoClip::Holder hClip = GetClipById2(id);
        if (!hClip)
            throw invalid_argument("Invalid value for argument 'id'!");
        if (hClip->Start() == start)
            return;

        bool isTailClip = hClip->End() == m_duration2;
        hClip->SetStart(start);
        if (!CheckClipRangeValid(id, hClip->Start(), hClip->End()))
            throw invalid_argument("Invalid argument for moving clip!");

        if (hClip->End() >= m_duration2)
            m_duration2 = hClip->End();
        else if (isTailClip)
        {
            int64_t newDuration = 0;
            for (auto& clip : m_clips2)
            {
                if (clip->End() > newDuration)
                    newDuration = clip->End();
            }
            m_duration2 = newDuration;
        }
        UpdateClipOverlap();
        m_clipChanged = true;
    }

    void ChangeClipRange(int64_t id, int64_t startOffset, int64_t endOffset) override
    {
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
        VideoClip::Holder hClip = GetClipById2(id);
        if (!hClip)
            throw invalid_argument("Invalid value for argument 'id'!");

        bool isTailClip = hClip->End() == m_duration2;
        bool rangeChanged = false;
        if (hClip->IsImage())
        {
            int64_t start = startOffset>endOffset ? endOffset : startOffset;
            int64_t end = startOffset>endOffset ? startOffset : endOffset;
            if (start != hClip->Start())
            {
                hClip->SetStart(start);
                rangeChanged = true;
            }
            int64_t duration = end-start;
            if (duration != hClip->Duration())
            {
                hClip->SetDuration(duration);
                rangeChanged = true;
            }
        }
        else
        {
            if (startOffset != hClip->StartOffset())
            {
                int64_t bias = startOffset-hClip->StartOffset();
                hClip->ChangeStartOffset(startOffset);
                hClip->SetStart(hClip->Start()+bias);
                rangeChanged = true;
            }
            if (endOffset != hClip->EndOffset())
            {
                hClip->ChangeEndOffset(endOffset);
                rangeChanged = true;
            }
        }
        if (!rangeChanged)
            return;
        if (!CheckClipRangeValid(id, hClip->Start(), hClip->End()))
            throw invalid_argument("Invalid argument for changing clip range!");

        if (hClip->End() >= m_duration2)
            m_duration2 = hClip->End();
        else if (isTailClip)
        {
            int64_t newDuration = 0;
            for (auto& clip : m_clips2)
            {
                if (clip->End() > newDuration)
                    newDuration = clip->End();
            }
            m_duration2 = newDuration;
        }
        UpdateClipOverlap();
        m_clipChanged = true;
    }

    VideoClip::Holder RemoveClipById(int64_t clipId) override
    {
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
        auto iter = find_if(m_clips2.begin(), m_clips2.end(), [clipId](const VideoClip::Holder& clip) {
            return clip->Id() == clipId;
        });
        if (iter == m_clips2.end())
            return nullptr;

        auto hClip = *iter;
        bool isTailClip = hClip->End() == m_duration2;
        m_clips2.erase(iter);
        hClip->SetTrackId(-1);

        if (isTailClip)
        {
            int64_t newDuration = 0;
            for (auto& clip : m_clips2)
            {
                if (clip->End() > newDuration)
                    newDuration = clip->End();
            }
            m_duration2 = newDuration;
        }
        UpdateClipOverlap();
        m_clipChanged = true;
        return hClip;
    }

    VideoClip::Holder RemoveClipByIndex(uint32_t index) override
    {
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
        if (index >= m_clips2.size())
            throw invalid_argument("Argument 'index' exceeds the count of clips!");

        auto iter = m_clips2.begin();
        while (index > 0)
        {
            iter++; index--;
        }

        auto hClip = *iter;
        bool isTailClip = hClip->End() == m_duration2;
        m_clips2.erase(iter);
        hClip->SetTrackId(-1);

        if (isTailClip)
        {
            int64_t newDuration = 0;
            for (auto& clip : m_clips2)
            {
                if (clip->End() > newDuration)
                    newDuration = clip->End();
            }
            m_duration2 = newDuration;
        }
        UpdateClipOverlap();
        m_clipChanged = true;
        return hClip;
    }

    list<VideoClip::Holder> GetClipList() override
    {
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
        return list<VideoClip::Holder>(m_clips);
    }

    list<VideoOverlap::Holder> GetOverlapList() override
    {
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
        return list<VideoOverlap::Holder>(m_overlaps);
    }

    int64_t Id() const override
    {
        return m_id;
    }

    uint32_t OutWidth() const override
    {
        return m_hSettings->VideoOutWidth();
    }

    uint32_t OutHeight() const override
    {
        return m_hSettings->VideoOutHeight();
    }

    Ratio FrameRate() const override
    {
        return m_hSettings->VideoOutFrameRate();
    }

    int64_t Duration() const override
    {
        if (m_clipChanged)
            return m_duration2;
        return m_duration;
    }

    int64_t ReadPos(int64_t frameIndex) const
    {
        const auto frameRate = m_hSettings->VideoOutFrameRate();
        return round((double)frameIndex*1000*frameRate.den/frameRate.num);
    }

    bool Direction() const override
    {
        return m_readForward;
    }

    ReadFrameTask::Holder CreateReadFrameTask(int64_t frameIndex, bool canDrop, bool needSeek, bool bypassBgNode, ReadFrameTask::Callback* pCb) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (frameIndex < 0)
            return nullptr;
        const int64_t readPos = ReadPos(frameIndex);
        ReadFrameTask_Impl* pTask = new ReadFrameTask_Impl(frameIndex, readPos, canDrop, needSeek, bypassBgNode);
        ReadFrameTask::Holder hTask(pTask, READ_FRAME_TASK_HOLDER_DELETER);
        if (pCb) pTask->SetCallback(pCb);
        {
            lock_guard<mutex> lk2(m_readFrameTasksLock);
            if (!m_readFrameTasks.empty())
            {
                ReadFrameTask_Impl* pTailTask = dynamic_cast<ReadFrameTask_Impl*>(m_readFrameTasks.back().get());
                if (!pTailTask->IsStarted() && pTailTask->CanDrop())
                {
                    auto& rft = m_readFrameTasks.back();
                    rft->SetDiscarded();
                    m_readFrameTasks.pop_back();
                }
            }
            m_readFrameTasks.push_back(hTask);
        }
        return hTask;
    }

    void SetDirection(bool forward) override
    {
        if (m_readForward == forward)
            return;
        m_readForward = forward;
        for (auto& clip : m_clips)
            clip->SetDirection(forward);
    }

    void SetVisible(bool visible) override
    {
        m_visible = visible;
    }

    bool IsVisible() const override
    {
        return m_visible;
    }

    VideoClip::Holder GetClipByIndex(uint32_t index) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (index >= m_clips.size())
            return nullptr;
        auto iter = m_clips.begin();
        while (index > 0)
        {
            iter++; index--;
        }
        return *iter;
    }

    VideoClip::Holder GetClipById(int64_t id) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        auto iter = find_if(m_clips.begin(), m_clips.end(), [id] (const VideoClip::Holder& clip) {
            return clip->Id() == id;
        });
        if (iter != m_clips.end())
            return *iter;
        return nullptr;
    }

    VideoOverlap::Holder GetOverlapById(int64_t id) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        auto iter = find_if(m_overlaps.begin(), m_overlaps.end(), [id] (const VideoOverlap::Holder& ovlp) {
            return ovlp->Id() == id;
        });
        if (iter != m_overlaps.end())
            return *iter;
        return nullptr;
    }

    void UpdateClipState() override
    {
        if (!m_clipChanged)
            return;
        {
            lock_guard<recursive_mutex> lk2(m_clipChangeLock);
            if (!m_clipChanged)
                return;
            m_clips = m_clips2;
            m_overlaps = m_overlaps2;
            m_clipChanged = false;
        }
        // udpate duration
        if (m_clips.empty())
        {
            m_duration = 0;
        }
        else
        {
            m_clips.sort(CLIP_SORT_CMP);
            auto& tail = m_clips.back();
            m_duration = tail->End();
        }
        m_needUpdateReadIter = true;
    }

    void UpdateSettings(SharedSettings::Holder hSettings) override
    {
        lock_guard<recursive_mutex> lk(m_clipChangeLock);
        for (auto& hClip : m_clips2)
            hClip->UpdateSettings(hSettings);
    }

    void SetPreReadMaxNum(int iMaxNum) override
    {
        m_iPreReadMaxNum = iMaxNum > 4 ? iMaxNum : 4;
    }

    void SetLogLevel(Logger::Level l) override
    {
        m_logger->SetShowLevels(l);
    }

    friend ostream& operator<<(ostream& os, VideoTrack_Impl& track);

private:
    void ReadFrameProc()
    {
        while (!m_quitThread)
        {
            bool idleLoop = true;

            ReadFrameTask::Holder hTask;
            ReadFrameTask_Impl* pTask = nullptr;
            // check if there is a task need to be processed
            {
                lock_guard<mutex> lk(m_readFrameTasksLock);
                // 1st, try to find a task that needs to be processed
                auto iter = m_readFrameTasks.begin();
                while (iter != m_readFrameTasks.end())
                {
                    // remove this task if it's discarded
                    if ((*iter)->IsDiscarded())
                    {
                        iter = m_readFrameTasks.erase(iter);
                        continue;
                    }

                    ReadFrameTask_Impl* pt = dynamic_cast<ReadFrameTask_Impl*>(iter->get());
                    if (pt->NeedProcess())
                    {
                        hTask = *iter;
                        pTask = pt;
                        break;
                    }
                    iter++;
                }
                if (!hTask)
                {
                    // 2nd, if no frame needs to be processed, then try to find a frame that needs to read the source mat
                    int index = 0;
                    iter = m_readFrameTasks.begin();
                    while (iter != m_readFrameTasks.end() && index < m_iPreReadMaxNum)
                    {
                        // remove this task if it's discarded
                        if ((*iter)->IsDiscarded())
                        {
                            iter = m_readFrameTasks.erase(iter);
                            continue;
                        }

                        ReadFrameTask_Impl* pt = dynamic_cast<ReadFrameTask_Impl*>(iter->get());
                        if (!pt->IsSourceFrameReady())
                        {
                            if (!pt->IsStarted())
                            {
                                if (!pt->Start())
                                {
                                    iter = m_readFrameTasks.erase(iter);
                                    pt->SetDiscarded();
                                    continue;
                                }
                            }
                            if (pt->IsStarted())
                            {
                                hTask = *iter;
                                pTask = pt;
                                break;
                            }
                        }
                        iter++; index++;
                    }
                }
            }

            // if clip changed, update m_clips
            if (m_clipChanged)
                UpdateClipState();

            // handle read frame task
            if (pTask && !pTask->IsDiscarded())
            {
                const int64_t readPos = pTask->ReadPos();
                if (!pTask->IsInited() && !pTask->IsDiscarded())
                {
                    UpdateReadIterator(readPos);
                    VideoClip::Holder hClip1, hClip2;
                    VideoOverlap::Holder hOvlp;
                    if (m_readOverlapIter != m_overlaps.end() && readPos >= (*m_readOverlapIter)->Start() && readPos < (*m_readOverlapIter)->End())
                    {
                        hOvlp = *m_readOverlapIter;
                        hClip1 = hOvlp->FrontClip();
                        hClip2 = hOvlp->RearClip();
                    }
                    else if (m_readClipIter != m_clips.end() && readPos >= (*m_readClipIter)->Start() && readPos < (*m_readClipIter)->End())
                    {
                        hClip1 = *m_readClipIter;
                    }
                    pTask->Initialize(hClip1, hClip2, hOvlp);
                    list<VideoClip::Holder> clips;
                    {
                        lock_guard<recursive_mutex> lk(m_clipChangeLock);
                        clips = m_clips;
                    }
                    for (auto& c : clips)
                        c->NotifyReadPos(readPos);
                }
                if (!pTask->IsSourceFrameReady() && !pTask->IsDiscarded())
                {
                    if (pTask->NeedSeek() && !pTask->HasSeeked())
                    {
                        SeekClipPos(readPos);
                        pTask->SetSeeked();
                    }
                    pTask->DoReadSourceFrame();
                    if (pTask->IsSourceFrameReady())
                    {
                        // m_logger->Log(DEBUG) << "Track#" << m_id << ", frameIndex=" << pTask->FrameIndex() << "  SOURCE READY" << endl;
                        idleLoop = false;
                    }
                }
                if (pTask->IsSourceFrameReady() && pTask->NeedProcess() && !pTask->IsDiscarded())
                {
                    pTask->ProcessFrame();
                    // m_logger->Log(DEBUG) << "Track#" << m_id << ", frameIndex=" << pTask->FrameIndex() << "  OUTPUT READY" << endl;
                    idleLoop = false;
                }
            }

            if (idleLoop)
            {
                // m_logger->Log(DEBUG) << "Track#" << m_id << " slept" << endl;
                this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_TIME));
            }
        }
    }

    void SeekClipPos(int64_t readPos)
    {
        m_logger->Log(DEBUG) << "----> SeekClipPos(" << readPos << ")" << endl;
        for (auto& c : m_clips)
            c->SeekTo(readPos-c->Start());
    }

    bool CheckClipRangeValid(int64_t clipId, int64_t start, int64_t end)
    {
        // make sure a time span can only be overlapped by two clips at most, no more layers of overlap is allowed
        for (auto& overlap : m_overlaps2)
        {
            if (clipId == overlap->FrontClip()->Id() || clipId == overlap->RearClip()->Id())
                continue;
            if ((start > overlap->Start() && start < overlap->End()) ||
                (end > overlap->Start() && end < overlap->End()))
                return false;
        }
        return true;
    }

    void UpdateClipOverlap()
    {
        if (m_clips2.empty())
        {
            m_overlaps2.clear();
            return;
        }

        list<VideoOverlap::Holder> newOverlaps;
        auto clipIter1 = m_clips2.begin();
        auto clipIter2 = clipIter1; clipIter2++;
        while (clipIter2 != m_clips2.end())
        {
            const auto& clip1 = *clipIter1;
            const auto& clip2 = *clipIter2++;
            const auto cid1 = clip1->Id();
            const auto cid2 = clip2->Id();
            if (VideoOverlap::HasOverlap(clip1, clip2))
            {
                auto ovlpIter = m_overlaps2.begin();
                while (ovlpIter != m_overlaps2.end())
                {
                    const auto& ovlp = *ovlpIter;
                    const auto fid = ovlp->FrontClip()->Id();
                    const auto rid = ovlp->RearClip()->Id();
                    if (cid1 != fid && cid1 != rid || cid2 != fid && cid2 != rid)
                    {
                        ovlpIter++;
                        continue;
                    }
                    break;
                }
                if (ovlpIter != m_overlaps2.end())
                {
                    auto& ovlp = *ovlpIter;
                    ovlp->Update();
                    assert(ovlp->Duration() > 0);
                    newOverlaps.push_back(*ovlpIter);
                }
                else
                {
                    newOverlaps.push_back(VideoOverlap::CreateInstance(0, clip1, clip2));
                }
            }
            if (clipIter2 == m_clips2.end())
            {
                clipIter1++;
                clipIter2 = clipIter1;
                clipIter2++;
            }
        }

        m_overlaps2 = std::move(newOverlaps);
        m_overlaps2.sort(OVERLAP_SORT_CMP);
    }

    void UpdateReadIterator(int64_t readPos)
    {
        if (m_needUpdateReadIter.exchange(false))
        {
            if (m_readForward)
            {
                // update read clip iterator
                m_readClipIter = m_clips.end();
                {
                    auto iter = m_clips.begin();
                    while (iter != m_clips.end())
                    {
                        const VideoClip::Holder& hClip = *iter;
                        int64_t clipPos = readPos-hClip->Start();
                        if (m_readClipIter == m_clips.end() && clipPos < hClip->Duration())
                            m_readClipIter = iter;
                        iter++;
                    }
                }
                // update read overlap iterator
                m_readOverlapIter = m_overlaps.end();
                {
                    auto iter = m_overlaps.begin();
                    while (iter != m_overlaps.end())
                    {
                        const VideoOverlap::Holder& hOverlap = *iter;
                        int64_t overlapPos = readPos-hOverlap->Start();
                        if (m_readOverlapIter == m_overlaps.end() && overlapPos < hOverlap->Duration())
                        {
                            m_readOverlapIter = iter;
                            break;
                        }
                        iter++;
                    }
                }
            }
            else
            {
                m_readClipIter = m_clips.end();
                {
                    auto riter = m_clips.rbegin();
                    while (riter != m_clips.rend())
                    {
                        const VideoClip::Holder& hClip = *riter;
                        int64_t clipPos = readPos-hClip->Start();
                        if (m_readClipIter == m_clips.end() && clipPos >= 0)
                            m_readClipIter = riter.base();
                        riter++;
                    }
                }
                m_readOverlapIter = m_overlaps.end();
                {
                    auto riter = m_overlaps.rbegin();
                    while (riter != m_overlaps.rend())
                    {
                        const VideoOverlap::Holder& hOverlap = *riter;
                        int64_t overlapPos = readPos-hOverlap->Start();
                        if (m_readOverlapIter == m_overlaps.end() && overlapPos >= 0)
                            m_readOverlapIter = riter.base();
                        riter++;
                    }
                }
            }
        }
        else
        {
            if (!m_clips.empty())
            {
                if (m_readClipIter == m_clips.end())
                    m_readClipIter--;
                if (m_readForward)
                {
                    if (m_readClipIter != m_clips.begin())
                    {
                        auto prevClipIter = m_readClipIter;
                        prevClipIter--;
                        while (m_readClipIter != m_clips.begin() && readPos < (*prevClipIter)->End())
                        {
                            m_readClipIter = prevClipIter;
                            if (prevClipIter == m_clips.begin())
                                break;
                            prevClipIter--;
                        }
                    }
                    if (readPos >= (*m_readClipIter)->End())
                    {
                        while (m_readClipIter != m_clips.end() && readPos >= (*m_readClipIter)->End())
                            m_readClipIter++;
                    }
                }
                else
                {
                    auto nextClipIter = m_readClipIter;
                    nextClipIter++;
                    while (nextClipIter != m_clips.end() && readPos >= (*nextClipIter)->Start())
                    {
                        m_readClipIter = nextClipIter;
                        nextClipIter++;
                    }
                    if (readPos < (*m_readClipIter)->Start())
                    {
                        while (m_readClipIter != m_clips.begin() && readPos < (*m_readClipIter)->Start())
                            m_readClipIter--;
                    }
                }
            }
            else
            {
                m_readClipIter = m_clips.end();
            }
            if (!m_overlaps.empty())
            {
                if (m_readOverlapIter == m_overlaps.end())
                    m_readOverlapIter--;
                if (m_readForward)
                {
                    if (m_readOverlapIter != m_overlaps.begin())
                    {
                        auto prevOvlpIter = m_readOverlapIter;
                        prevOvlpIter--;
                        while (m_readOverlapIter != m_overlaps.begin() && readPos < (*prevOvlpIter)->End())
                        {
                            m_readOverlapIter = prevOvlpIter;
                            if (prevOvlpIter == m_overlaps.begin())
                                break;
                            prevOvlpIter--;
                        }
                    }
                    if (readPos >= (*m_readOverlapIter)->End())
                    {
                        while (m_readOverlapIter != m_overlaps.end() && readPos >= (*m_readOverlapIter)->End())
                            m_readOverlapIter++;
                    }
                }
                else
                {
                    auto nextOvlpIter = m_readOverlapIter;
                    nextOvlpIter++;
                    while (nextOvlpIter != m_overlaps.end() && readPos >= (*nextOvlpIter)->Start())
                    {
                        m_readOverlapIter = nextOvlpIter;
                        nextOvlpIter++;
                    }
                    if (readPos < (*m_readOverlapIter)->Start())
                    {
                        while (m_readOverlapIter != m_overlaps.begin() && readPos < (*m_readOverlapIter)->Start())
                            m_readOverlapIter--;
                    }
                }
            }
            else
            {
                m_readOverlapIter = m_overlaps.end();
            }
        }
    }

    VideoClip::Holder GetClipById2(int64_t id)
    {
        auto iter = find_if(m_clips2.begin(), m_clips2.end(), [id] (const VideoClip::Holder& clip) {
            return clip->Id() == id;
        });
        if (iter != m_clips2.end())
            return *iter;
        return nullptr;
    }

private:
    ALogger* m_logger;
    recursive_mutex m_apiLock;
    int64_t m_id;
    SharedSettings::Holder m_hSettings;
    list<VideoClip::Holder> m_clips;
    list<VideoClip::Holder>::iterator m_readClipIter;
    list<VideoClip::Holder> m_clips2;
    bool m_clipChanged{false};
    atomic_bool m_needUpdateReadIter{false};
    recursive_mutex m_clipChangeLock;
    list<VideoOverlap::Holder> m_overlaps;
    list<VideoOverlap::Holder>::iterator m_readOverlapIter;
    list<VideoOverlap::Holder> m_overlaps2;
    int64_t m_duration{0}, m_duration2{0};
    bool m_readForward{true};
    bool m_visible{true};
    thread m_readThread;
    bool m_quitThread{false};
    list<ReadFrameTask::Holder> m_readFrameTasks;
    int m_iPreReadMaxNum{4};
    mutex m_readFrameTasksLock;
};

static const auto VIDEO_TRACK_HOLDER_DELETER = [] (VideoTrack* p) {
    VideoTrack_Impl* ptr = dynamic_cast<VideoTrack_Impl*>(p);
    delete ptr;
};

VideoTrack::Holder VideoTrack::CreateInstance(int64_t id, SharedSettings::Holder hSettings)
{
    return VideoTrack::Holder(new VideoTrack_Impl(id, hSettings), VIDEO_TRACK_HOLDER_DELETER);
}

VideoTrack::Holder VideoTrack_Impl::Clone(SharedSettings::Holder hSettings)
{
    lock_guard<recursive_mutex> lk(m_apiLock);
    if (m_clipChanged)
        UpdateClipState();

    VideoTrack_Impl* newInstance = new VideoTrack_Impl(m_id, hSettings);
    // duplicate the clips
    for (auto clip : m_clips)
    {
        auto newClip = clip->Clone(hSettings);
        newClip->SetTrackId(m_id);
        newInstance->m_clips2.push_back(newClip);
    }
    newInstance->UpdateClipOverlap();
    newInstance->m_clipChanged = true;
    newInstance->UpdateClipState();
    // clone the transitions on the overlaps
    for (auto overlap : m_overlaps)
    {
        auto iter = find_if(newInstance->m_overlaps.begin(), newInstance->m_overlaps.end(), [overlap] (auto& ovlp) {
            return overlap->FrontClip()->Id() == ovlp->FrontClip()->Id() && overlap->RearClip()->Id() == ovlp->RearClip()->Id();
        });
        if (iter != newInstance->m_overlaps.end())
        {
            auto trans = overlap->GetTransition();
            if (trans)
                (*iter)->SetTransition(trans->Clone());
        }
    }
    return VideoTrack::Holder(newInstance, VIDEO_TRACK_HOLDER_DELETER);
}

ostream& operator<<(ostream& os, VideoTrack_Impl& track)
{
    os << "{ clips(" << track.m_clips.size() << "): [";
    auto clipIter = track.m_clips.begin();
    while (clipIter != track.m_clips.end())
    {
        os << *clipIter;
        clipIter++;
        if (clipIter != track.m_clips.end())
            os << ", ";
        else
            break;
    }
    os << "], overlaps(" << track.m_overlaps.size() << "): [";
    auto ovlpIter = track.m_overlaps.begin();
    while (ovlpIter != track.m_overlaps.end())
    {
        os << *ovlpIter;
        ovlpIter++;
        if (ovlpIter != track.m_overlaps.end())
            os << ", ";
        else
            break;
    }
    os << "] }";
    return os;
}

ostream& operator<<(ostream& os, VideoTrack::Holder hTrack)
{
    VideoTrack_Impl* pTrkImpl = dynamic_cast<VideoTrack_Impl*>(hTrack.get());
    os << *pTrkImpl;
    return os;
}
}
