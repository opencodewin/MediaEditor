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
#include <algorithm>
#include <atomic>
#include <sstream>
#include <cmath>
#include <iomanip>
#include "MultiTrackVideoReader.h"
#include "VideoBlender.h"
#include "FFUtils.h"
#include "ThreadUtils.h"
#include "DebugHelper.h"

using namespace std;
using namespace Logger;

namespace MediaCore
{
class MultiTrackVideoReader_Impl : public MultiTrackVideoReader
{
public:
    static ALogger* s_logger;

    MultiTrackVideoReader_Impl()
    {
        m_logger = MultiTrackVideoReader::GetLogger();
    }

    MultiTrackVideoReader_Impl(const MultiTrackVideoReader_Impl&) = delete;
    MultiTrackVideoReader_Impl(MultiTrackVideoReader_Impl&&) = delete;
    MultiTrackVideoReader_Impl& operator=(const MultiTrackVideoReader_Impl&) = delete;

    bool Configure(SharedSettings::Holder hSettings) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is already started!";
            return false;
        }
        const auto outWidth = hSettings->VideoOutWidth();
        if (outWidth == 0 || outWidth > 16384)
        {
            ostringstream oss; oss << "INVALID argument! VideoOutWidth=" << outWidth << " is not supported. Valid range is (0, 16384].";
            m_errMsg = oss.str();
            return false;
        }
        const auto outHeight = hSettings->VideoOutHeight();
        if (outHeight == 0 || outHeight > 16384)
        {
            ostringstream oss; oss << "INVALID argument! VideoOutHeight=" << outHeight << " is not supported. Valid range is (0, 16384].";
            m_errMsg = oss.str();
            return false;
        }
        const auto frameRate = hSettings->VideoOutFrameRate();
        if (!Ratio::IsValid(frameRate))
        {
            ostringstream oss; oss << "INVALID argument! VideoOutFrameRate={" << frameRate.num << "/" << frameRate.den << "} is invalid.";
            m_errMsg = oss.str();
            return false;
        }
        const auto colorFormat = hSettings->VideoOutColorFormat();
        if (colorFormat != IM_CF_RGBA)
        {
            ostringstream oss; oss << "INVALID argument! VideoOutColorFormat=" << colorFormat << "is not supported. ONLY support output RGBA format.";
            m_errMsg = oss.str();
            return false;
        }
        const auto dataType = hSettings->VideoOutDataType();
        if (dataType != IM_DT_INT8 && dataType != IM_DT_INT16 && dataType != IM_DT_FLOAT32)
        {
            ostringstream oss; oss << "INVALID argument! VideoOutDataType=" << dataType << "is not supported. ONLY support output INT8/INT16/FLOAT32 data type.";
            m_errMsg = oss.str();
            return false;
        }

        Close();

        m_hSettings = hSettings;
        m_outFrameRate = hSettings->VideoOutFrameRate();
        m_readFrameIdx = 0;
        m_frameInterval = (double)frameRate.den/frameRate.num;

        m_hMixBlender = VideoBlender::CreateInstance();
        if (!m_hMixBlender)
        {
            m_errMsg = "CANNOT create new 'VideoBlender' instance for mixing!";
            return false;
        }
        m_hSubBlender = VideoBlender::CreateInstance();
        if (!m_hSubBlender)
        {
            m_errMsg = "CANNOT create new 'VideoBlender' instance for subtitle!";
            return false;
        }
        if (!hSettings->GetHwaccelManager())
        {
            hSettings->SetHwaccelManager(HwaccelManager::GetDefaultInstance());
        }

        m_configured = true;
        return true;
    }

    bool Configure(uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate, ImDataType outDtype) override
    {
        auto hSettings = SharedSettings::CreateInstance();
        hSettings->SetVideoOutWidth(outWidth);
        hSettings->SetVideoOutHeight(outHeight);
        hSettings->SetVideoOutFrameRate(frameRate);
        hSettings->SetVideoOutColorFormat(IM_CF_RGBA);
        hSettings->SetVideoOutDataType(outDtype);
        return Configure(hSettings);
    }

    Holder CloneAndConfigure(SharedSettings::Holder hSettings) override;

    Holder CloneAndConfigure(uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate, ImDataType outDtype) override
    {
        auto hSettings = SharedSettings::CreateInstance();
        hSettings->SetVideoOutWidth(outWidth);
        hSettings->SetVideoOutHeight(outHeight);
        hSettings->SetVideoOutFrameRate(frameRate);
        hSettings->SetVideoOutColorFormat(IM_CF_RGBA);
        hSettings->SetVideoOutDataType(outDtype);
        return CloneAndConfigure(hSettings);
    }

    SharedSettings::Holder GetSharedSettings() override
    {
        return m_hSettings;
    }

    bool Start() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (m_started)
        {
            return true;
        }
        if (!m_configured)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT configured yet!";
            return false;
        }

        StartMixingThread();

        m_started = true;
        return true;
    }

    void Close() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        TerminateMixingThread();

        m_tracks.clear();
        m_mixFrameTasks.clear();
        m_seekingTasks.clear();
        m_prevOutFrame = nullptr;
        m_configured = false;
        m_started = false;
        m_frameInterval = 0;
        m_hSettings = nullptr;
    }

    VideoTrack::Holder AddTrack(int64_t trackId, int64_t insertAfterId) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return nullptr;
        }

        TerminateMixingThread();

        VideoTrack::Holder hNewTrack = VideoTrack::CreateInstance(trackId, m_hSettings);
        // hNewTrack->SetLogLevel(DEBUG);
        hNewTrack->SetDirection(m_readForward);
        {
            lock_guard<recursive_mutex> lk2(m_trackLock);
            if (insertAfterId == -1)
            {
                m_tracks.push_back(hNewTrack);
            }
            else
            {
                auto insertBeforeIter = m_tracks.begin();
                if (insertAfterId != -2)
                {
                    insertBeforeIter = find_if(m_tracks.begin(), m_tracks.end(), [insertAfterId] (auto trk) {
                        return trk->Id() == insertAfterId;
                    });
                    if (insertBeforeIter == m_tracks.end())
                    {
                        ostringstream oss;
                        oss << "CANNOT find the video track specified by argument 'insertAfterId' " << insertAfterId << "!";
                        m_errMsg = oss.str();
                        return nullptr;
                    }
                    insertBeforeIter++;
                }
                m_tracks.insert(insertBeforeIter, hNewTrack);
            }
        }

        SeekTo(ReadPos());
        StartMixingThread();
        return hNewTrack;
    }

    VideoTrack::Holder RemoveTrackByIndex(uint32_t index) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return nullptr;
        }
        if (index >= m_tracks.size())
        {
            m_errMsg = "Invalid value for argument 'index'!";
            return nullptr;
        }

        TerminateMixingThread();

        VideoTrack::Holder delTrack;
        {
            lock_guard<recursive_mutex> lk2(m_trackLock);
            auto iter = m_tracks.begin();
            while (index > 0 && iter != m_tracks.end())
            {
                iter++;
                index--;
            }
            if (iter != m_tracks.end())
            {
                delTrack = *iter;
                m_tracks.erase(iter);
                UpdateDuration();
            }
        }

        SeekTo(ReadPos());
        StartMixingThread();
        return delTrack;
    }

    VideoTrack::Holder RemoveTrackById(int64_t trackId) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return nullptr;
        }

        TerminateMixingThread();

        VideoTrack::Holder delTrack;
        {
            lock_guard<recursive_mutex> lk2(m_trackLock);
            auto iter = find_if(m_tracks.begin(), m_tracks.end(), [trackId] (const VideoTrack::Holder& track) {
                return track->Id() == trackId;
            });
            if (iter != m_tracks.end())
            {
                delTrack = *iter;
                m_tracks.erase(iter);
                UpdateDuration();
            }
        }

        SeekTo(ReadPos());
        StartMixingThread();
        return delTrack;
    }

    bool ChangeTrackViewOrder(int64_t targetId, int64_t insertAfterId) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (targetId == insertAfterId)
        {
            m_errMsg = "INVALID arguments! 'targetId' must NOT be the SAME as 'insertAfterId'!";
            return false;
        }

        lock_guard<recursive_mutex> lk2(m_trackLock);
        auto targetTrackIter = find_if(m_tracks.begin(), m_tracks.end(), [targetId] (auto trk) {
            return trk->Id() == targetId;
        });
        if (targetTrackIter == m_tracks.end())
        {
            ostringstream oss;
            oss << "CANNOT find the video track specified by argument 'targetId' " << targetId << "!";
            m_errMsg = oss.str();
            return false;
        }
        if (insertAfterId == -1)
        {
            auto moveTrack = *targetTrackIter;
            m_tracks.erase(targetTrackIter);
            m_tracks.push_back(moveTrack);
        }
        else
        {
            auto insertBeforeIter = m_tracks.begin();
            if (insertAfterId != -2)
            {
                insertBeforeIter = find_if(m_tracks.begin(), m_tracks.end(), [insertAfterId] (auto trk) {
                    return trk->Id() == insertAfterId;
                });
                if (insertBeforeIter == m_tracks.end())
                {
                    ostringstream oss;
                    oss << "CANNOT find the video track specified by argument 'insertAfterId' " << insertAfterId << "!";
                    m_errMsg = oss.str();
                    return false;
                }
                insertBeforeIter++;
            }
            auto moveTrack = *targetTrackIter;
            m_tracks.erase(targetTrackIter);
            m_tracks.insert(insertBeforeIter, moveTrack);
        }
        return true;
    }

    bool SetDirection(bool forward, int64_t pos) override
    {
        if (m_readForward == forward)
            return true;

        TerminateMixingThread();

        m_readForward = forward;
        for (auto& track : m_tracks)
            track->SetDirection(forward);
        SeekToByIdx(m_readFrameIdx);

        StartMixingThread();
        return true;
    }

    bool SeekTo(int64_t pos, bool bForceReseek = false) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }
        m_logger->Log(DEBUG) << "------> SeekTo pos=" << pos << endl;
        auto frmIdx = MillsecToFrameIndex(pos);
        return SeekToByIdx(frmIdx, bForceReseek);
    }

    bool SeekToByIdx(int64_t frmIdx, bool bForceReseek = false) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }
        m_logger->Log(DEBUG) << "------> SeekTo frameIndex=" << frmIdx << endl;
        ClearAllMixFrameTasks();
        m_prevOutFrame = nullptr;
        m_readFrameIdx = frmIdx;
        int step = m_readForward ? 1 : -1;
        AddMixFrameTask(m_readFrameIdx, bForceReseek, true);
        for (auto i = 1; i < m_szCacheFrameNum; i++)
            AddMixFrameTask(m_readFrameIdx+i*step, false, false);
        return true;
    }

    bool ConsecutiveSeek(int64_t pos) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }
        m_logger->Log(DEBUG) << "======> ConsecutiveSeek pos=" << pos << endl;
        m_prevOutFrame = nullptr;
        m_readFrameIdx = MillsecToFrameIndex(pos, 1);
        AddSeekingTask(m_readFrameIdx);
        m_inSeeking = true;
        return true;
    }

    bool StopConsecutiveSeek() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }
        m_logger->Log(DEBUG) << "=======> StopConsecutiveSeek" << endl;
        m_inSeeking = false;
        int step = m_readForward ? 1 : -1;
        auto reuseTask = ExtractSeekingTask(m_readFrameIdx);
        if (reuseTask && reuseTask->TriggerStart())
        {
            AddMixFrameTask(reuseTask, true);
        }
        else
        {
            AddMixFrameTask(m_readFrameIdx, false, true, true);
        }
        for (auto i = 1; i < m_szCacheFrameNum; i++)
            AddMixFrameTask(m_readFrameIdx+i*step, false, false);
        return true;
    }

    bool SetTrackVisible(int64_t id, bool visible) override
    {
        auto track = GetTrackById(id, false);
        if (track)
        {
            track->SetVisible(visible);
            return true;
        }
        ostringstream oss;
        oss << "Track with id=" << id << " does NOT EXIST!";
        m_errMsg = oss.str();
        return false;
    }

    bool IsTrackVisible(int64_t id) override
    {
        auto track = GetTrackById(id, false);
        if (track)
            return track->IsVisible();
        return false;
    }

    bool ReadVideoFrameByPosEx(int64_t pos, vector<CorrelativeFrame>& frames, bool nonblocking, bool precise) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }
        if (pos < 0)
        {
            m_errMsg = "Invalid argument value for 'pos'! Can NOT be NEGATIVE.";
            return false;
        }

        int64_t targetIndex = MillsecToFrameIndex(pos);
        bool ret = ReadVideoFrameWithoutSubtitle(targetIndex, frames, nonblocking, precise);
        if (ret && !m_subtrks.empty())
        {
            auto iter = find_if(frames.begin(), frames.end(), [](const CorrelativeFrame& corFrame) {
                return corFrame.phase == CorrelativeFrame::PHASE_AFTER_MIXING;
            });
            if (iter != frames.end())
            {
                auto& vmat = iter->frame;
                vmat = BlendSubtitle(vmat);
            }
        }
        return ret;
    }

    bool ReadVideoFrameByPos(int64_t pos, ImGui::ImMat& vmat, bool nonblocking) override
    {
        vector<CorrelativeFrame> frames;
        bool success = ReadVideoFrameByPosEx(pos, frames, nonblocking, true);
        if (!success)
            return false;
        auto iter = find_if(frames.begin(), frames.end(), [](const CorrelativeFrame& corFrame) {
            return corFrame.phase == CorrelativeFrame::PHASE_AFTER_MIXING;
        });
        if (iter != frames.end())
        {
            vmat = iter->frame;
            return true;
        }
        return false;
    }


    bool ReadVideoFrameByIdxEx(int64_t frmIdx, vector<CorrelativeFrame>& frames, bool nonblocking, bool precise) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }
        if (frmIdx < 0)
        {
            m_errMsg = "Invalid argument value for 'frmIdx'! Can NOT be NEGATIVE.";
            return false;
        }

        bool ret = ReadVideoFrameWithoutSubtitle(frmIdx, frames, nonblocking, precise);
        if (ret && !m_subtrks.empty())
        {
            auto iter = find_if(frames.begin(), frames.end(), [](const CorrelativeFrame& corFrame) {
                return corFrame.phase == CorrelativeFrame::PHASE_AFTER_MIXING;
            });
            if (iter != frames.end())
            {
                auto& vmat = iter->frame;
                vmat = BlendSubtitle(vmat);
            }
        }
        return ret;
    }

    bool ReadVideoFrameByIdx(int64_t frmIdx, ImGui::ImMat& vmat, bool nonblocking) override
    {
        vector<CorrelativeFrame> frames;
        bool success = ReadVideoFrameByIdxEx(frmIdx, frames, nonblocking, true);
        if (!success)
            return false;
        auto iter = find_if(frames.begin(), frames.end(), [](const CorrelativeFrame& corFrame) {
            return corFrame.phase == CorrelativeFrame::PHASE_AFTER_MIXING;
        });
        if (iter != frames.end())
        {
            vmat = iter->frame;
            return true;
        }
        return false;
    }

    bool ReadNextVideoFrameEx(vector<CorrelativeFrame>& frames) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }

        int step = m_readForward ? 1 : -1;
        int64_t targetIndex = m_readFrameIdx+step;
        if (targetIndex < 0) targetIndex = 0;
        bool ret = ReadVideoFrameWithoutSubtitle(targetIndex, frames, false, true);
        if (ret && !m_subtrks.empty())
        {
            auto iter = find_if(frames.begin(), frames.end(), [](const CorrelativeFrame& corFrame) {
                return corFrame.phase == CorrelativeFrame::PHASE_AFTER_MIXING;
            });
            if (iter != frames.end())
            {
                auto& vmat = iter->frame;
                vmat = BlendSubtitle(vmat);
            }
        }
        return ret;
    }

    bool ReadNextVideoFrame(ImGui::ImMat& vmat) override
    {
        vector<CorrelativeFrame> frames;
        bool success = ReadNextVideoFrameEx(frames);
        if (!success)
            return false;
        auto iter = find_if(frames.begin(), frames.end(), [](const CorrelativeFrame& corFrame) {
            return corFrame.phase == CorrelativeFrame::PHASE_AFTER_MIXING;
        });
        if (iter != frames.end())
        {
            vmat = iter->frame;
            return true;
        }
        return false;
    }

    int64_t MillsecToFrameIndex(int64_t mts, int iMode = 0) override
    {
        if (iMode == 1)
            return (int64_t)round((double)mts*m_outFrameRate.num/(m_outFrameRate.den*1000));
        else if (iMode == 2)
            return (int64_t)ceil((double)mts*m_outFrameRate.num/(m_outFrameRate.den*1000));
        return (int64_t)floor((double)mts*m_outFrameRate.num/(m_outFrameRate.den*1000));
    }

    int64_t FrameIndexToMillsec(int64_t frmIdx) override
    {
        return (int64_t)round((double)frmIdx*m_outFrameRate.den*1000/m_outFrameRate.num);
    }

    void UpdateDuration() override
    {
        lock_guard<recursive_mutex> lk(m_trackLock);
        int64_t dur = 0;
        for (auto& track : m_tracks)
        {
            const int64_t trackDur = track->Duration();
            if (trackDur > dur)
                dur = trackDur;
        }
        m_duration = dur;
    }

    bool Refresh(bool updateDuration) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }

        if (updateDuration)
            UpdateDuration();

        SeekToByIdx(m_readFrameIdx);
        return true;
    }

    bool RefreshTrackView(const unordered_set<int64_t>& trackIds) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!m_started)
        {
            m_errMsg = "This MultiTrackVideoReader instance is NOT started yet!";
            return false;
        }

        {
            lock_guard<recursive_mutex> lk2(m_mixFrameTasksLock);
            for (auto& mft : m_mixFrameTasks)
            {
                bool foundTrack = false;
                for (auto& elem : mft->readFrameTaskTable)
                {
                    auto& trk = elem.first;
                    const int64_t trkid = trk->Id();
                    auto iter = find(trackIds.begin(), trackIds.end(), trkid);
                    if (iter != trackIds.end())
                    {
                        foundTrack = true;
                        auto& rft = elem.second;
                        rft->Reprocess();
                    }
                }
                if (foundTrack)
                    mft->outputReady = false;
            }
        }
        return true;
    }

    bool UpdateSettings(SharedSettings::Holder hSettings) override
    {
        lock(m_apiLock, m_trackLock);
        lock_guard<recursive_mutex> lk0(m_apiLock, adopt_lock);
        lock_guard<recursive_mutex> lk1(m_trackLock, adopt_lock);
        if (hSettings->VideoOutWidth() == m_hSettings->VideoOutWidth() && hSettings->VideoOutHeight() == m_hSettings->VideoOutHeight()
            && hSettings->VideoOutFrameRate() == m_hSettings->VideoOutFrameRate() && hSettings->VideoOutColorFormat() == m_hSettings->VideoOutColorFormat()
            && hSettings->VideoOutDataType() == m_hSettings->VideoOutDataType())
            return true;
        if (hSettings->VideoOutFrameRate() != m_hSettings->VideoOutFrameRate())
        {
            m_errMsg = "Frame rate CANNOT be changed in UpdateSettings()!";
            return false;
        }
        if (hSettings->VideoOutColorFormat() != m_hSettings->VideoOutColorFormat())
        {
            m_errMsg = "Color format CANNOT be changed in UpdateSettings()!";
            return false;
        }
        if (hSettings->VideoOutDataType() != m_hSettings->VideoOutDataType())
        {
            m_errMsg = "Video data type CANNOT be changed in UpdateSettings()!";
            return false;
        }

        TerminateMixingThread();
        for (auto& hTrack : m_tracks)
            hTrack->UpdateSettings(hSettings);
        m_hSettings->SyncVideoSettingsFrom(hSettings.get());
        SeekToByIdx(m_readFrameIdx, true);
        StartMixingThread();
        return true;
    }

    size_t GetCacheFrameNum() const override
    {
        return m_szCacheFrameNum;
    }

    void SetCacheFrameNum(size_t szCacheNum) override
    {
        m_szCacheFrameNum = szCacheNum;
        for (auto& hTrack : m_tracks)
            hTrack->SetPreReadMaxNum(szCacheNum);
    }

    uint32_t TrackCount() const override
    {
        return m_tracks.size();
    }

    list<VideoTrack::Holder>::iterator TrackListBegin() override
    {
        return m_tracks.begin();
    }

    list<VideoTrack::Holder>::iterator TrackListEnd() override
    {
        return m_tracks.end();
    }

    VideoTrack::Holder GetTrackByIndex(uint32_t idx) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (idx >= m_tracks.size())
            return nullptr;
        lock_guard<recursive_mutex> lk2(m_trackLock);
        auto iter = m_tracks.begin();
        while (idx-- > 0 && iter != m_tracks.end())
            iter++;
        return iter != m_tracks.end() ? *iter : nullptr;
    }

    VideoTrack::Holder GetTrackById(int64_t id, bool createIfNotExists) override
    {
        lock(m_apiLock, m_trackLock);
        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        lock_guard<recursive_mutex> lk2(m_trackLock, adopt_lock);
        auto iter = find_if(m_tracks.begin(), m_tracks.end(), [id] (const VideoTrack::Holder& track) {
            return track->Id() == id;
        });
        if (iter != m_tracks.end())
            return *iter;
        if (createIfNotExists)
            return AddTrack(id, -1);
        else
            return nullptr;
    }

    VideoClip::Holder GetClipById(int64_t clipId) override
    {
        lock(m_apiLock, m_trackLock);
        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        lock_guard<recursive_mutex> lk2(m_trackLock, adopt_lock);
        VideoClip::Holder clip;
        for (auto& track : m_tracks)
        {
            clip = track->GetClipById(clipId);
            if (clip)
                break;
        }
        if (!clip)
        {
            ostringstream oss;
            oss << "CANNOT find clip with id " << clipId << "!";
            m_errMsg = oss.str();
        }
        return clip;
    }

    VideoOverlap::Holder GetOverlapById(int64_t ovlpId) override
    {
        lock(m_apiLock, m_trackLock);
        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        lock_guard<recursive_mutex> lk2(m_trackLock, adopt_lock);
        VideoOverlap::Holder ovlp;
        for (auto& track : m_tracks)
        {
            ovlp = track->GetOverlapById(ovlpId);
            if (ovlp)
                break;
        }
        return ovlp;
    }

    int64_t Duration() const override
    {
        return m_duration;
    }

    int64_t ReadPos() const override
    {
        const auto frameRate = m_hSettings->VideoOutFrameRate();
        return round((double)m_readFrameIdx*1000*frameRate.den/frameRate.num);
    }

    SubtitleTrackHolder BuildSubtitleTrackFromFile(int64_t id, const string& url, int64_t insertAfterId) override
    {
        const auto outWidth = m_hSettings->VideoOutWidth();
        const auto outHeight = m_hSettings->VideoOutHeight();
        SubtitleTrackHolder newSubTrack = SubtitleTrack::BuildFromFile(id, url);
        newSubTrack->SetFrameSize(outWidth, outHeight);
        newSubTrack->SetAlignment(5);
        newSubTrack->SetOffsetCompensationV((int32_t)((double)outHeight*0.43));
        newSubTrack->SetOffsetCompensationV(0.43f);
        newSubTrack->EnableFullSizeOutput(false);
        lock_guard<mutex> lk(m_subtrkLock);
        if (insertAfterId == -1)
        {
            m_subtrks.push_back(newSubTrack);
        }
        else
        {
            auto insertBeforeIter = m_subtrks.begin();
            if (insertAfterId != -2)
            {
                insertBeforeIter = find_if(m_subtrks.begin(), m_subtrks.end(), [insertAfterId] (auto trk) {
                    return trk->Id() == insertAfterId;
                });
                if (insertBeforeIter == m_subtrks.end())
                {
                    ostringstream oss;
                    oss << "CANNOT find the subtitle track specified by argument 'insertAfterId' " << insertAfterId << "!";
                    m_errMsg = oss.str();
                    return nullptr;
                }
                insertBeforeIter++;
            }
            m_subtrks.insert(insertBeforeIter, newSubTrack);
        }
        return newSubTrack;
    }

    SubtitleTrackHolder NewEmptySubtitleTrack(int64_t id, int64_t insertAfterId) override
    {
        const auto outWidth = m_hSettings->VideoOutWidth();
        const auto outHeight = m_hSettings->VideoOutHeight();
        SubtitleTrackHolder newSubTrack = SubtitleTrack::NewEmptyTrack(id);
        newSubTrack->SetFrameSize(outWidth, outHeight);
        newSubTrack->SetAlignment(5);
        newSubTrack->SetOffsetCompensationV((int32_t)((double)outHeight*0.43));
        newSubTrack->SetOffsetCompensationV(0.43f);
        newSubTrack->EnableFullSizeOutput(false);
        lock_guard<mutex> lk(m_subtrkLock);
        if (insertAfterId == -1)
        {
            m_subtrks.push_back(newSubTrack);
        }
        else
        {
            auto insertBeforeIter = m_subtrks.begin();
            if (insertAfterId != -2)
            {
                insertBeforeIter = find_if(m_subtrks.begin(), m_subtrks.end(), [insertAfterId] (auto trk) {
                    return trk->Id() == insertAfterId;
                });
                if (insertBeforeIter == m_subtrks.end())
                {
                    ostringstream oss;
                    oss << "CANNOT find the subtitle track specified by argument 'insertAfterId' " << insertAfterId << "!";
                    m_errMsg = oss.str();
                    return nullptr;
                }
                insertBeforeIter++;
            }
            m_subtrks.insert(insertBeforeIter, newSubTrack);
        }
        return newSubTrack;
    }

    SubtitleTrackHolder GetSubtitleTrackById(int64_t trackId) override
    {
        lock_guard<mutex> lk(m_subtrkLock);
        auto iter = find_if(m_subtrks.begin(), m_subtrks.end(), [trackId] (SubtitleTrackHolder& hTrk) {
            return hTrk->Id() == trackId;
        });
        if (iter == m_subtrks.end())
            return nullptr;
        return *iter;
    }

    SubtitleTrackHolder RemoveSubtitleTrackById(int64_t trackId) override
    {
        lock_guard<mutex> lk(m_subtrkLock);
        auto iter = find_if(m_subtrks.begin(), m_subtrks.end(), [trackId] (SubtitleTrackHolder& hTrk) {
            return hTrk->Id() == trackId;
        });
        if (iter == m_subtrks.end())
            return nullptr;
        SubtitleTrackHolder hTrk = *iter;
        m_subtrks.erase(iter);
        return hTrk;
    }

    bool ChangeSubtitleTrackViewOrder(int64_t targetId, int64_t insertAfterId) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (targetId == insertAfterId)
        {
            m_errMsg = "INVALID arguments! 'targetId' must NOT be the SAME as 'insertAfterId'!";
            return false;
        }

        lock_guard<mutex> lk2(m_subtrkLock);
        auto targetTrackIter = find_if(m_subtrks.begin(), m_subtrks.end(), [targetId] (auto trk) {
            return trk->Id() == targetId;
        });
        if (targetTrackIter == m_subtrks.end())
        {
            ostringstream oss;
            oss << "CANNOT find the video track specified by argument 'targetId' " << targetId << "!";
            m_errMsg = oss.str();
            return false;
        }
        if (insertAfterId == -1)
        {
            auto moveTrack = *targetTrackIter;
            m_subtrks.erase(targetTrackIter);
            m_subtrks.push_back(moveTrack);
        }
        else
        {
            auto insertBeforeIter = m_subtrks.begin();
            if (insertAfterId != -2)
            {
                insertBeforeIter = find_if(m_subtrks.begin(), m_subtrks.end(), [insertAfterId] (auto trk) {
                    return trk->Id() == insertAfterId;
                });
                if (insertBeforeIter == m_subtrks.end())
                {
                    ostringstream oss;
                    oss << "CANNOT find the video track specified by argument 'insertAfterId' " << insertAfterId << "!";
                    m_errMsg = oss.str();
                    return false;
                }
                insertBeforeIter++;
            }
            auto moveTrack = *targetTrackIter;
            m_subtrks.erase(targetTrackIter);
            m_subtrks.insert(insertBeforeIter, moveTrack);
        }
        return true;
    }

    string GetError() const override
    {
        return m_errMsg;
    }

private:
    bool ReadVideoFrameWithoutSubtitle(int64_t frameIndex, vector<CorrelativeFrame>& frames, bool nonblocking, bool precise)
    {
        m_readFrameIdx = frameIndex;
        if (m_prevOutFrame && m_prevOutFrame->frameIndex == frameIndex && m_prevOutFrame->outputReady && !precise)
        {
            frames = m_prevOutFrame->GetOutputFrames();
            return true;
        }

        MixFrameTask::Holder hCandiFrame;
        if (m_inSeeking)
        {
            hCandiFrame = FindSeekingFlashAndRemoveDeprecatedTasks(frameIndex);
            if (hCandiFrame)
            {
                m_prevOutFrame = hCandiFrame;
                frames = hCandiFrame->GetOutputFrames();
            }
            else if (!m_seekingFlash.empty())
            {
                frames = m_seekingFlash;
            }
        }
        else
        {
            hCandiFrame = FindCandidateAndRemoveDeprecatedTasks(frameIndex, precise);
            if (hCandiFrame && hCandiFrame->outputReady)
            {
                m_prevOutFrame = hCandiFrame;
                frames = hCandiFrame->GetOutputFrames();
            }

            int step = m_readForward ? 1 : -1;
            if (precise)
            {
                if (!hCandiFrame)
                    AddMixFrameTask(frameIndex, false, false);
                for (auto i = 1; i < m_szCacheFrameNum; i++)
                    AddMixFrameTask(frameIndex+i*step, false, false);
            }
            else
            {
                AddMixFrameTask(frameIndex+step, true, false);
            }

            if (!nonblocking && precise)
            {
                if (!hCandiFrame || !hCandiFrame->outputReady)
                {
                    while (!m_quit)
                    {
                        this_thread::sleep_for(chrono::milliseconds(5));
                        hCandiFrame = FindCandidateAndRemoveDeprecatedTasks(frameIndex, precise);
                        if (!hCandiFrame)
                        {
                            ostringstream oss;
                            oss << "CANNOT find corresponding MixFrameTask for frameIndex=" << frameIndex << "!";
                            m_errMsg = oss.str();
                            break;
                        }
                        else if (hCandiFrame->outputReady)
                            break;
                    }
                }
                if (hCandiFrame)
                {
                    m_prevOutFrame = hCandiFrame;
                    frames = hCandiFrame->GetOutputFrames();
                    return true;
                }
            }
        }

        if (frames.empty())
            return false;
        return true;
    }

    void StartMixingThread()
    {
        m_quit = false;
        m_mixingThread = thread(&MultiTrackVideoReader_Impl::MixingThreadProc, this);
        SysUtils::SetThreadName(m_mixingThread, "MtvMixing");
        m_mixingThread2 = thread(&MultiTrackVideoReader_Impl::MixingThreadProc2, this);
        SysUtils::SetThreadName(m_mixingThread2, "MtvMixing2");
    }

    void TerminateMixingThread()
    {
        m_quit = true;
        if (m_mixingThread.joinable())
            m_mixingThread.join();
        if (m_mixingThread2.joinable())
            m_mixingThread2.join();
    }

    struct MixFrameTask : public ReadFrameTask::Callback
    {
        using Holder = shared_ptr<MixFrameTask>;

        ~MixFrameTask()
        {
            for (auto& elem : readFrameTaskTable)
            {
                auto& rft = elem.second;
                rft->SetDiscarded();
            }
            readFrameTaskTable.clear();
            m_outputFrames.clear();
        }

        int64_t frameIndex;
        vector<pair<VideoTrack::Holder, ReadFrameTask::Holder>> readFrameTaskTable;
        bool processingStarted{false};
        bool outputReady{false};
        atomic_uint8_t state{0};  // lsb#1 means this task is dropped, lsb#2 means this task is started
        static const uint8_t DROP_BIT, START_BIT;

        bool IsProcessingStarted() const { return processingStarted; }

        void StartProcessing()
        {
            for (auto& elem : readFrameTaskTable)
            {
                auto& rft = elem.second;
                rft->StartProcessing();
            }
            processingStarted = true;
        }

        bool TriggerDrop() override
        {
            uint8_t testVal = 0;
            if (state.compare_exchange_strong(testVal, DROP_BIT))
                return true;
            if (testVal == DROP_BIT)
                return true;
            return false;
        }

        bool TriggerStart() override
        {
            uint8_t testVal = 0;
            if (state.compare_exchange_strong(testVal, START_BIT))
                return true;
            if (testVal == START_BIT)
                return true;
            return false;
        }

        vector<CorrelativeFrame> GetOutputFrames()
        {
            vector<CorrelativeFrame> result;
            lock_guard<mutex> lg(m_mtxOutputFrames);
            for (const auto& elem : m_outputFrames)
            {
                if (elem->frame.empty() && elem->hVfrm)
                    elem->hVfrm->GetMat(elem->frame);
                result.push_back(*elem);
            }
            return std::move(result);
        }

        void UpdateOutputFrames(const vector<CorrelativeVideoFrame::Holder>& corVidFrames) override
        {
            lock_guard<mutex> lg(m_mtxOutputFrames);
            for (const auto& elem : corVidFrames)
            {
                auto iter = find(m_outputFrames.begin(), m_outputFrames.end(), elem);
                if (iter != m_outputFrames.end())
                    continue;
                iter = find_if(m_outputFrames.begin(), m_outputFrames.end(), [elem](const auto& elem2) {
                    return elem->phase == elem2->phase && elem->clipId == elem2->clipId && elem->trackId == elem2->trackId;
                });
                if (iter == m_outputFrames.end())
                    m_outputFrames.push_back(elem);
                else
                    *iter = elem;
            }
        }

    private:
        vector<CorrelativeVideoFrame::Holder> m_outputFrames;
        mutex m_mtxOutputFrames;
    };

    MixFrameTask::Holder FindCandidateAndRemoveDeprecatedTasks(int64_t targetIndex, bool precise)
    {
        MixFrameTask::Holder hCandiFrame;
        lock_guard<recursive_mutex> lk(m_mixFrameTasksLock);
        if (m_readForward)
        {
            auto mftIter = m_mixFrameTasks.begin();
            while (mftIter != m_mixFrameTasks.end())
            {
                auto& mft = *mftIter++;
                if (mft->frameIndex <= targetIndex)
                {
                    if (!precise && mft->outputReady)
                        hCandiFrame = mft;
                    else if (precise && mft->frameIndex == targetIndex)
                        hCandiFrame = mft;
                    if (mft->frameIndex == targetIndex)
                        break;
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            auto mftIter = m_mixFrameTasks.begin();
            while (mftIter != m_mixFrameTasks.end())
            {
                auto mft = *mftIter++;
                if (mft->frameIndex >= targetIndex)
                {
                    if (!precise && mft->outputReady)
                        hCandiFrame = mft;
                    else if (precise && mft->frameIndex == targetIndex)
                        hCandiFrame = mft;
                    if (mft->frameIndex == targetIndex)
                        break;
                }
                else
                {
                    break;
                }
            }
        }

        if (hCandiFrame)
        {
            auto eraseIter = m_mixFrameTasks.begin();
            while (eraseIter != m_mixFrameTasks.end())
            {
                if ((*eraseIter)->frameIndex == hCandiFrame->frameIndex)
                    break;
                auto& delfrm = *eraseIter;
                for (auto& elem : delfrm->readFrameTaskTable)
                {
                    auto& rft = elem.second;
                    rft->SetDiscarded();
                }
                m_logger->Log(DEBUG) << "---- Remove mixframe task, frameIndex=" << delfrm->frameIndex << endl;
                eraseIter = m_mixFrameTasks.erase(eraseIter);
            }
        }
        return hCandiFrame;
    }

    MixFrameTask::Holder AddMixFrameTask(int64_t frameIndex, bool canDrop, bool needSeek, bool clearBeforeAdd = false)
    {
        if (frameIndex < 0)
            return nullptr;
        lock_guard<recursive_mutex> lk(m_mixFrameTasksLock);
        list<MixFrameTask::Holder>::iterator mftIter;
        if (clearBeforeAdd)
        {
            ClearAllMixFrameTasks();
            mftIter = m_mixFrameTasks.end();
        }
        else
        {
            mftIter = find_if(m_mixFrameTasks.begin(), m_mixFrameTasks.end(), [frameIndex] (auto& t) {
                return t->frameIndex == frameIndex;
            });
        }
        MixFrameTask::Holder hTask;
        if (mftIter == m_mixFrameTasks.end())
        {
            bool needClearTaskList = false;
            if (!m_mixFrameTasks.empty())
            {
                auto backTaskFrameIndex = m_mixFrameTasks.back()->frameIndex;
                needClearTaskList = m_readForward ? frameIndex < backTaskFrameIndex : frameIndex > backTaskFrameIndex;
            }
            if (needClearTaskList)
                ClearAllMixFrameTasks();

            list<VideoTrack::Holder> tracks;
            {
                lock_guard<recursive_mutex> trackLk(m_trackLock);
                tracks = m_tracks;
            }
            hTask = MixFrameTask::Holder(new MixFrameTask());
            hTask->frameIndex = frameIndex;
            for (auto& trk : tracks)
            {
                auto rft = trk->CreateReadFrameTask(frameIndex, canDrop, needSeek || needClearTaskList, false, dynamic_cast<ReadFrameTask::Callback*>(hTask.get()));
                rft->SetVisible(trk->IsVisible());
                hTask->readFrameTaskTable.push_back({trk, rft});
            }
            m_logger->Log(DEBUG) << "++ AddMixFrameTask: frameIndex=" << frameIndex << ", canDrop=" << canDrop << endl;
            m_mixFrameTasks.push_back(hTask);
        }
        else
        {
            hTask = *mftIter;
        }
        return hTask;
    }

    MixFrameTask::Holder AddMixFrameTask(MixFrameTask::Holder hMft, bool clearBeforeAdd)
    {
        int64_t frameIndex = hMft->frameIndex;
        lock_guard<recursive_mutex> lk(m_mixFrameTasksLock);
        if (clearBeforeAdd)
        {
            ClearAllMixFrameTasks();
            m_mixFrameTasks.push_back(hMft);
            m_logger->Log(DEBUG) << "++ AddMixFrameTask[2-0]: frameIndex=" << frameIndex << endl;
        }
        else
        {
            auto mftIter = find_if(m_mixFrameTasks.begin(), m_mixFrameTasks.end(), [frameIndex] (auto& t) {
                return t->frameIndex == frameIndex;
            });
            if (mftIter == m_mixFrameTasks.end())
            {
                bool needClearTaskList = false;
                if (!m_mixFrameTasks.empty())
                {
                    auto backTaskFrameIndex = m_mixFrameTasks.back()->frameIndex;
                    needClearTaskList = m_readForward ? frameIndex < backTaskFrameIndex : frameIndex > backTaskFrameIndex;
                }
                if (needClearTaskList)
                    ClearAllMixFrameTasks();

                m_logger->Log(DEBUG) << "++ AddMixFrameTask[2-1]: frameIndex=" << frameIndex << endl;
                m_mixFrameTasks.push_back(hMft);
            }
            else
            {
                hMft = *mftIter;
            }
        }
        return hMft;
    }

    void ClearAllMixFrameTasks()
    {
        if (m_mixFrameTasks.empty())
            return;
        m_logger->Log(DEBUG) << "------ Clear All MixFrameTasks" << endl;
        lock_guard<recursive_mutex> lk(m_mixFrameTasksLock);
        auto mftIter = m_mixFrameTasks.begin();
        while (mftIter != m_mixFrameTasks.end())
        {
            auto& mft = *mftIter;
            for (auto& elem : mft->readFrameTaskTable)
            {
                auto& rft = elem.second;
                rft->SetDiscarded();
            }
            mftIter = m_mixFrameTasks.erase(mftIter);
        }
    }

    MixFrameTask::Holder FindSeekingFlashAndRemoveDeprecatedTasks(int64_t targetIndex)
    {
        MixFrameTask::Holder hCandiFrame;
        lock_guard<mutex> lk(m_seekingTasksLock);
        for (auto& skt : m_seekingTasks)
        {
            if (!skt->outputReady)
                continue;
            if (!hCandiFrame || std::abs(hCandiFrame->frameIndex-targetIndex) > std::abs(skt->frameIndex-targetIndex))
                hCandiFrame = skt;
        }
        if (hCandiFrame)
        {
            auto sktIter = m_seekingTasks.begin();
            while (sktIter != m_seekingTasks.end())
            {
                if ((*sktIter)->frameIndex == hCandiFrame->frameIndex)
                {
                    sktIter++;
                    continue;
                }
                auto nextIter = sktIter; nextIter++;
                if (nextIter == m_seekingTasks.end())
                    break;
                auto& delfrm = *sktIter;
                if (!delfrm->outputReady)
                {
                    sktIter++;
                    continue;
                }
                for (auto& elem : delfrm->readFrameTaskTable)
                {
                    auto& rft = elem.second;
                    rft->SetDiscarded();
                }
                m_logger->Log(DEBUG) << "---- Remove seeking task, frameIndex=" << delfrm->frameIndex << endl;
                sktIter = m_seekingTasks.erase(sktIter);
            }
        }
        return hCandiFrame;
    }

    MixFrameTask::Holder AddSeekingTask(int64_t frameIndex)
    {
        if (frameIndex < 0)
            return nullptr;
        lock_guard<mutex> lk(m_seekingTasksLock);
        auto sktIter = find_if(m_seekingTasks.begin(), m_seekingTasks.end(), [frameIndex] (auto& t) {
            return t->frameIndex == frameIndex;
        });
        MixFrameTask::Holder hTask;
        if (sktIter == m_seekingTasks.end())
        {
            list<VideoTrack::Holder> tracks;
            {
                lock_guard<recursive_mutex> trackLk(m_trackLock);
                tracks = m_tracks;
            }
            hTask = MixFrameTask::Holder(new MixFrameTask());
            hTask->frameIndex = frameIndex;
            for (auto& trk : tracks)
            {
                auto rft = trk->CreateReadFrameTask(frameIndex, true, true, true, dynamic_cast<ReadFrameTask::Callback*>(hTask.get()));
                hTask->readFrameTaskTable.push_back({trk, rft});
            }
            m_logger->Log(DEBUG) << "++ AddSeekingTask: frameIndex=" << frameIndex << endl;
            m_seekingTasks.push_back(hTask);
        }
        else
        {
            hTask = *sktIter;
        }
        return hTask;
    }

    MixFrameTask::Holder ExtractSeekingTask(int64_t frameIndex)
    {
        if (frameIndex < 0)
            return nullptr;
        lock_guard<mutex> lk(m_seekingTasksLock);
        auto sktIter = find_if(m_seekingTasks.begin(), m_seekingTasks.end(), [frameIndex] (auto& t) {
            return t->frameIndex == frameIndex;
        });
        if (sktIter == m_seekingTasks.end())
            return nullptr;
        auto hTask = *sktIter;
        m_seekingTasks.erase(sktIter);
        return hTask;
    }

    void ClearAllSeekingTasks()
    {
        if (m_seekingTasks.empty())
            return;
        m_logger->Log(DEBUG) << "------ Clear All SeekingTasks" << endl;
        lock_guard<mutex> lk(m_seekingTasksLock);
        auto mftIter = m_seekingTasks.begin();
        while (mftIter != m_seekingTasks.end())
        {
            auto& mft = *mftIter;
            for (auto& elem : mft->readFrameTaskTable)
            {
                auto& rft = elem.second;
                rft->SetDiscarded();
            }
            mftIter = m_seekingTasks.erase(mftIter);
        }
    }

    void RemoveDiscardedTasks(list<MixFrameTask::Holder>& taskList)
    {
        auto iter = taskList.begin();
        while (iter != taskList.end())
        {
            auto& mft = *iter;
            if (mft->state == MixFrameTask::DROP_BIT)
            {
                for (auto& elem : mft->readFrameTaskTable)
                {
                    auto& rft = elem.second;
                    rft->SetDiscarded();
                }
                iter = taskList.erase(iter);
            }
            else
            {
                iter++;
            }
        }
    }

    void MixingThreadProc()
    {
        m_logger->Log(DEBUG) << "Enter MixingThreadProc(VIDEO)..." << endl;

        const auto outWidth = m_hSettings->VideoOutWidth();
        const auto outHeight = m_hSettings->VideoOutHeight();
        const auto frameRate = m_hSettings->VideoOutFrameRate();
        const auto matDtype = m_hSettings->VideoOutDataType();
        bool afterSeek = false;
        bool prevInSeekingState = m_inSeeking;
        while (!m_quit)
        {
            bool idleLoop = true;

            list<MixFrameTask::Holder> mixFrameTasks;
            if (m_inSeeking)
            {
                if (prevInSeekingState != m_inSeeking)
                {
                    prevInSeekingState = m_inSeeking;
                    ClearAllMixFrameTasks();
                }
                lock_guard<mutex> lk(m_seekingTasksLock);
                // auto statusLog = PrintMixFrameTaskListStatus(m_seekingTasks, "SeekingTasks");
                // m_logger->Log(DEBUG) << statusLog << endl;
                RemoveDiscardedTasks(m_seekingTasks);
                mixFrameTasks = m_seekingTasks;
            }
            else
            {
                if (prevInSeekingState != m_inSeeking)
                {
                    prevInSeekingState = m_inSeeking;
                    ClearAllSeekingTasks();
                }
                lock_guard<recursive_mutex> lk(m_mixFrameTasksLock);
                // auto statusLog = PrintMixFrameTaskListStatus(m_mixFrameTasks, "MixFrameTasks");
                // m_logger->Log(DEBUG) << statusLog << endl;
                RemoveDiscardedTasks(m_mixFrameTasks);
                mixFrameTasks = m_mixFrameTasks;
            }
            auto mftIter = mixFrameTasks.begin();
            while (mftIter != mixFrameTasks.end())
            {
                auto& mft = *mftIter++;
                if (mft->outputReady || mft->IsProcessingStarted())
                    continue;

                bool allSourceReady = true;
                for (auto& elem : mft->readFrameTaskTable)
                {
                    auto& rft = elem.second;
                    if (!rft->IsSourceFrameReady())
                    {
                        allSourceReady = false;
                        break;
                    }
                }
                if (!allSourceReady)
                    continue;

                for (auto& elem : mft->readFrameTaskTable)
                {
                    auto& rft = elem.second;
                    rft->UpdateHostFrames();
                }

                mft->StartProcessing();
                idleLoop = false;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_TIME));
        }

        m_logger->Log(DEBUG) << "Leave MixingThreadProc(VIDEO)." << endl;
    }

    void MixingThreadProc2()
    {
        m_logger->Log(DEBUG) << "Enter MixingThreadProc2(VIDEO)..." << endl;

        const auto outWidth = m_hSettings->VideoOutWidth();
        const auto outHeight = m_hSettings->VideoOutHeight();
        const auto frameRate = m_hSettings->VideoOutFrameRate();
        const auto matDtype = m_hSettings->VideoOutDataType();
        bool afterSeek = false;
        bool prevInSeekingState = m_inSeeking;
        while (!m_quit)
        {
            bool idleLoop = true;

            list<MixFrameTask::Holder> mixFrameTasks;
            if (m_inSeeking)
            {
                lock_guard<mutex> lk(m_seekingTasksLock);
                mixFrameTasks = m_seekingTasks;
            }
            else
            {
                lock_guard<recursive_mutex> lk(m_mixFrameTasksLock);
                mixFrameTasks = m_mixFrameTasks;
            }
            auto mftIter = mixFrameTasks.begin();
            while (mftIter != mixFrameTasks.end())
            {
                auto& mft = *mftIter++;
                if (mft->outputReady || !mft->IsProcessingStarted())
                    continue;

                bool allProcessed = true;
                for (auto& elem : mft->readFrameTaskTable)
                {
                    auto& rft = elem.second;
                    if (!rft->IsOutputFrameReady())
                    {
                        allProcessed = false;
                        break;
                    }
                }
                if (!allProcessed)
                    continue;

                for (auto& elem : mft->readFrameTaskTable)
                {
                    auto& rft = elem.second;
                    rft->UpdateHostFrames();
                }

                ImGui::ImMat mixedFrame;
                double timestamp = (double)mft->frameIndex*frameRate.den/frameRate.num;
                auto rftIter = mft->readFrameTaskTable.rbegin();
                int mixFrameCnt = 0;
                while (rftIter != mft->readFrameTaskTable.rend())
                {
                    auto elem = *rftIter++;
                    auto& trk = elem.first;
                    auto& rft = elem.second;
                    VideoFrame::Holder hVfrm;
                    if (trk->IsVisible())
                    {
                        hVfrm = rft->GetVideoFrame();
                        mixFrameCnt++;
                    }
                    ImGui::ImMat vmat;
                    if (hVfrm) hVfrm->GetMat(vmat);
                    if (!vmat.empty())
                    {
                        const auto fOpacity = hVfrm->Opacity();
                        if (mixedFrame.empty() && fOpacity < 1.f)
                        {
                            mixedFrame.create_type(outWidth, outHeight, 4, matDtype);
                            memset(mixedFrame.data, 0, mixedFrame.total()*mixedFrame.elemsize);
                        }
                        if (mixedFrame.empty())
                            mixedFrame = vmat;
                        else
                            mixedFrame = m_hMixBlender->Blend(mixedFrame, vmat, fOpacity);
                        if (abs(timestamp-vmat.time_stamp) > 0.001)
                            m_logger->Log(WARN) << "'vmat' read from track #" << trk->Id() << " has WRONG TIMESTAMP! timestamp("
                                << timestamp << ") != vmat(" << vmat.time_stamp << ")." << endl;
                    }
                }

                const bool bMixedFrameIsEmpty = mixedFrame.empty();
                if (bMixedFrameIsEmpty)
                {
                    mixedFrame.create_type(outWidth, outHeight, 4, matDtype);
                    memset(mixedFrame.data, 0, mixedFrame.total()*mixedFrame.elemsize);
                }
                mixedFrame.time_stamp = timestamp;
                mixedFrame.flags |= IM_MAT_FLAGS_VIDEO_FRAME;
                mixedFrame.rate.num = frameRate.num;
                mixedFrame.rate.den = frameRate.den;
                mixedFrame.index_count = mft->frameIndex;
                mft->UpdateOutputFrames({ CorrelativeVideoFrame::Holder(new CorrelativeVideoFrame(CorrelativeFrame::PHASE_AFTER_MIXING, 0, 0, VideoFrame::CreateMatInstance(mixedFrame))) });
                mft->outputReady = true;
                if (mixFrameCnt == 0 || !bMixedFrameIsEmpty)
                    m_seekingFlash = mft->GetOutputFrames();
                m_logger->Log(DEBUG) << "---------> Got mixed frame at frameIndex=" << mft->frameIndex << ", pos=" << (int64_t)(timestamp*1000) << endl;
                idleLoop = false;
                break;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_TIME));
        }

        m_logger->Log(DEBUG) << "Leave MixingThreadProc2(VIDEO)." << endl;
    }

    string PrintMixFrameTaskListStatus(list<MixFrameTask::Holder>& taskList, const string& listName)
    {
        ostringstream oss;
        oss << endl << "-------------------------  " << listName << "  ----------------------------" << endl;
        if (taskList.empty())
        {
            oss << "(empty)" << endl;
        }
        else
        {
            int idx = 0;
            for (auto& mft : taskList)
                oss << setw(6) << mft->frameIndex;
            oss << endl;
            while (true)
            {
                for (auto& mft : taskList)
                {
                    string status = "";
                    int i = 0;
                    auto iter = mft->readFrameTaskTable.begin();
                    while (iter != mft->readFrameTaskTable.end() && i++ < idx)
                        iter++;
                    if (iter != mft->readFrameTaskTable.end())
                    {
                        auto& rft = iter->second;
                        if (rft->IsOutputFrameReady())
                            status = "R2";
                        else if (rft->IsSourceFrameReady())
                            status = "R1";
                        else if (rft->IsStarted())
                            status = "ST";
                        else if (rft->IsDiscarded())
                            status = "XX";
                        else
                            status = "NEW";
                    }
                    oss << setw(6) << status;
                }
                oss << endl;
                idx++;
                if (idx >= m_tracks.size())
                    break;
            }
        }
        oss << "-----------------------------------------------------------------" << endl;
        return oss.str();
    }

    ImGui::ImMat BlendSubtitle(ImGui::ImMat& vmat)
    {
        if (m_subtrks.empty())
            return vmat;

        ImGui::ImMat res = vmat;
        bool cloned = false;
        const auto pos = (int64_t)(vmat.time_stamp*1000);
        lock_guard<mutex> lk(m_subtrkLock);
        for (auto& hSubTrack : m_subtrks)
        {
            if (!hSubTrack->IsVisible())
                continue;

            auto hSubClip = hSubTrack->GetClipByTime(pos);
            if (hSubClip)
            {
                auto subImg = hSubClip->Image(pos-hSubClip->StartTime());
                if (subImg.Valid())
                {
                    // blend subtitle-image
                    SubtitleImage::Rect dispRect = subImg.Area();
                    ImGui::ImMat submat = subImg.Vmat();
                    res = m_hSubBlender->Blend(res, submat, dispRect.x, dispRect.y);
                    if (res.empty())
                    {
                        m_logger->Log(Error) << "FAILED to blend subtitle on the output image! Error message is '" << m_hSubBlender->GetError() << "'." << endl;
                    }
                }
                else
                {
                    m_logger->Log(Error) << "Invalid 'SubtitleImage' at " << MillisecToString(pos) << "." << endl;
                }
            }
        }
        return res;
    }

private:
    ALogger* m_logger;
    string m_errMsg;
    recursive_mutex m_apiLock;

    thread m_mixingThread;
    thread m_mixingThread2;
    list<VideoTrack::Holder> m_tracks;
    recursive_mutex m_trackLock;
    VideoBlender::Holder m_hMixBlender;

    list<MixFrameTask::Holder> m_mixFrameTasks;
    size_t m_szCacheFrameNum{1};
    recursive_mutex m_mixFrameTasksLock;
    MixFrameTask::Holder m_prevOutFrame;
    bool m_inSeeking{false};
    list<MixFrameTask::Holder> m_seekingTasks;
    mutex m_seekingTasksLock;
    vector<CorrelativeFrame> m_seekingFlash;

    SharedSettings::Holder m_hSettings;
    Ratio m_outFrameRate;
    double m_frameInterval{0};
    int64_t m_duration{0};
    int64_t m_readFrameIdx{0};
    bool m_readForward{true};

    list<SubtitleTrackHolder> m_subtrks;
    mutex m_subtrkLock;
    VideoBlender::Holder m_hSubBlender;

    bool m_configured{false};
    bool m_started{false};
    bool m_quit{false};
};

const uint8_t MultiTrackVideoReader_Impl::MixFrameTask::DROP_BIT = 0x1;
const uint8_t MultiTrackVideoReader_Impl::MixFrameTask::START_BIT = 0x2;

static const auto MULTI_TRACK_VIDEO_READER_DELETER = [] (MultiTrackVideoReader* p) {
    MultiTrackVideoReader_Impl* ptr = dynamic_cast<MultiTrackVideoReader_Impl*>(p);
    ptr->Close();
    delete ptr;
};

MultiTrackVideoReader::Holder MultiTrackVideoReader::CreateInstance()
{
    return MultiTrackVideoReader::Holder(new MultiTrackVideoReader_Impl(), MULTI_TRACK_VIDEO_READER_DELETER);
}

MultiTrackVideoReader::Holder MultiTrackVideoReader_Impl::CloneAndConfigure(SharedSettings::Holder hSettings)
{
    lock_guard<recursive_mutex> lk(m_apiLock);
    MultiTrackVideoReader_Impl* newInstance = new MultiTrackVideoReader_Impl();
    if (!newInstance->Configure(hSettings))
    {
        m_errMsg = newInstance->GetError();
        newInstance->Close(); delete newInstance;
        return nullptr;
    }

    // clone all the video tracks
    {
        lock_guard<recursive_mutex> lk2(m_trackLock);
        for (auto track : m_tracks)
        {
            newInstance->m_tracks.push_back(track->Clone(hSettings));
        }
    }
    newInstance->UpdateDuration();

    // clone all the subtitle tracks
    {
        lock_guard<mutex> lk2(m_subtrkLock);
        for (auto subtrk : m_subtrks)
        {
            if (!subtrk->IsVisible())
                continue;
            newInstance->m_subtrks.push_back(subtrk->Clone(hSettings->VideoOutWidth(), hSettings->VideoOutHeight()));
        }
    }

    // start new instance
    if (!newInstance->Start())
    {
        m_errMsg = newInstance->GetError();
        newInstance->Close(); delete newInstance;
        return nullptr;
    }
    return MultiTrackVideoReader::Holder(newInstance, MULTI_TRACK_VIDEO_READER_DELETER);
}

ALogger* MultiTrackVideoReader::GetLogger()
{
    return Logger::GetLogger("MTVReader");
}

ostream& operator<<(ostream& os, MultiTrackVideoReader::Holder hMtvReader)
{
    os << ">>> MultiTrackVideoReader :" << std::endl;
    auto trackIter = hMtvReader->TrackListBegin();
    while (trackIter != hMtvReader->TrackListEnd())
    {
        auto& track = *trackIter;
        os << "\t Track#" << track->Id() << " : " << track << std::endl;
        trackIter++;
    }
    os << "<<< [END]MultiTrackVideoReader";
    return os;
}
}
