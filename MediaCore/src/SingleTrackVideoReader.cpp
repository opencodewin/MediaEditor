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
class SingleTrackVideoReader_Impl : public MultiTrackVideoReader
{
public:
    static ALogger* s_logger;

    SingleTrackVideoReader_Impl()
    {
        m_logger = Logger::GetLogger("STVReader");
    }

    SingleTrackVideoReader_Impl(const SingleTrackVideoReader_Impl&) = delete;
    SingleTrackVideoReader_Impl(SingleTrackVideoReader_Impl&&) = delete;
    SingleTrackVideoReader_Impl& operator=(const SingleTrackVideoReader_Impl&) = delete;

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
        if (dataType != IM_DT_INT8 && dataType != IM_DT_FLOAT32)
        {
            ostringstream oss; oss << "INVALID argument! VideoOutDataType=" << dataType << "is not supported. ONLY support output INT8 & FLOAT32 data type.";
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

        m_track = nullptr;
        m_singleFrmTasks.clear();
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
        if (m_track)
        {
            m_errMsg = "This SingleTrackVideoReader instance already has a track!";
            return nullptr;
        }

        TerminateMixingThread();

        VideoTrack::Holder hNewTrack = VideoTrack::CreateInstance(trackId, m_hSettings);
        // hNewTrack->SetLogLevel(DEBUG);
        hNewTrack->SetDirection(m_readForward);
        {
            lock_guard<recursive_mutex> lk2(m_trackLock);
            m_track = hNewTrack;
            m_tracks.push_back(hNewTrack);
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
        if (index >= 1)
        {
            m_errMsg = "Invalid value for argument 'index'!";
            return nullptr;
        }

        TerminateMixingThread();

        VideoTrack::Holder delTrack;
        {
            lock_guard<recursive_mutex> lk2(m_trackLock);
            m_track = nullptr;
            m_tracks.clear();
            UpdateDuration();
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

        auto delTrack = m_track;
        if (m_track && m_track->Id() == trackId)
        {
            m_track = nullptr;
            m_tracks.clear();
            UpdateDuration();
        }

        SeekTo(ReadPos());
        StartMixingThread();
        return delTrack;
    }

    bool ChangeTrackViewOrder(int64_t targetId, int64_t insertAfterId) override
    {
        return true;
    }

    bool SetDirection(bool forward, int64_t pos) override
    {
        if (m_readForward == forward)
            return true;

        TerminateMixingThread();

        m_readForward = forward;
        if (m_track)
            m_track->SetDirection(forward);
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
        ClearAllSingleFrameTasks();
        m_prevOutFrame = nullptr;
        m_readFrameIdx = frmIdx;
        int step = m_readForward ? 1 : -1;
        AddSingleFrameTask(m_readFrameIdx, bForceReseek, true);
        for (auto i = 1; i < m_szCacheFrameNum; i++)
            AddSingleFrameTask(m_readFrameIdx+i*step, false, false);
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
        AddSingleFrameTask(m_readFrameIdx, false, true, true);
        int step = m_readForward ? 1 : -1;
        for (auto i = 1; i < m_szCacheFrameNum; i++)
            AddSingleFrameTask(m_readFrameIdx+i*step, false, false);
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
        m_duration = m_track ? m_track->Duration() : 0;
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

        const auto trkid = m_track->Id();
        auto iter = find(trackIds.begin(), trackIds.end(), trkid);
        if (iter == trackIds.end())
            return true;

        lock_guard<recursive_mutex> lk2(m_singleFrmTasksLock);
        for (auto& sft : m_singleFrmTasks)
        {
            sft->rft->Reprocess();
            sft->outputReady = false;
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
        if (m_track) m_track->UpdateSettings(hSettings);
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
        if (m_track) m_track->SetPreReadMaxNum(szCacheNum);
    }

    uint32_t TrackCount() const override
    {
        return m_track ? 1 : 0;
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
        if (idx == 0 && m_track)
            return m_track;
        return nullptr;
    }

    VideoTrack::Holder GetTrackById(int64_t id, bool createIfNotExists) override
    {
        lock(m_apiLock, m_trackLock);
        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        lock_guard<recursive_mutex> lk2(m_trackLock, adopt_lock);
        if (m_track && m_track->Id() == id)
            return m_track;
        else if (!m_track)
            return AddTrack(id, -1);
        return nullptr;
    }

    VideoClip::Holder GetClipById(int64_t clipId) override
    {
        lock(m_apiLock, m_trackLock);
        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        lock_guard<recursive_mutex> lk2(m_trackLock, adopt_lock);
        VideoClip::Holder clip;
        if (m_track)
            return m_track->GetClipById(clipId);
        return nullptr;
    }

    VideoOverlap::Holder GetOverlapById(int64_t ovlpId) override
    {
        lock(m_apiLock, m_trackLock);
        lock_guard<recursive_mutex> lk(m_apiLock, adopt_lock);
        lock_guard<recursive_mutex> lk2(m_trackLock, adopt_lock);
        VideoOverlap::Holder ovlp;
        if (m_track)
            return m_track->GetOverlapById(ovlpId);
        return nullptr;
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

        SingleFrameTask::Holder hCandiFrame;
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
            if (hCandiFrame)
            {
                m_prevOutFrame = hCandiFrame;
                frames = hCandiFrame->GetOutputFrames();
            }

            int step = m_readForward ? 1 : -1;
            if (precise)
            {
                if (!hCandiFrame)
                    AddSingleFrameTask(frameIndex, false, false);
                for (auto i = 1; i < m_szCacheFrameNum; i++)
                    AddSingleFrameTask(frameIndex+i*step, false, false);
            }
            else
            {
                AddSingleFrameTask(frameIndex+step, true, false);
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
                            oss << "CANNOT find corresponding SingleFrameTask for frameIndex=" << frameIndex << "!";
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
        m_mixingThread = thread(&SingleTrackVideoReader_Impl::MixingThreadProc, this);
        SysUtils::SetThreadName(m_mixingThread, "MtvMixing");
        m_mixingThread2 = thread(&SingleTrackVideoReader_Impl::MixingThreadProc2, this);
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

    struct SingleFrameTask : public ReadFrameTask::Callback
    {
        using Holder = shared_ptr<SingleFrameTask>;

        SingleFrameTask(bool seeking = false) : m_seeking(seeking)
        {}

        ~SingleFrameTask()
        {
            rft = nullptr;
            m_outputFrames.clear();
        }

        int64_t frameIndex;
        ReadFrameTask::Holder rft;
        bool processingStarted{false};
        bool outputReady{false};
        atomic_uint8_t state{0};  // lsb#1 means this task is dropped, lsb#2 means this task is started
        static const uint8_t DROP_BIT, START_BIT;

        bool IsProcessingStarted() const { return processingStarted; }

        void StartProcessing()
        {
            rft->StartProcessing();
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
        bool m_seeking;
        vector<CorrelativeVideoFrame::Holder> m_outputFrames;
        mutex m_mtxOutputFrames;
    };

    SingleFrameTask::Holder FindCandidateAndRemoveDeprecatedTasks(int64_t targetIndex, bool precise)
    {
        SingleFrameTask::Holder hCandiFrame;
        lock_guard<recursive_mutex> lk(m_singleFrmTasksLock);
        if (m_readForward)
        {
            auto mftIter = m_singleFrmTasks.begin();
            while (mftIter != m_singleFrmTasks.end())
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
            auto mftIter = m_singleFrmTasks.begin();
            while (mftIter != m_singleFrmTasks.end())
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
            auto eraseIter = m_singleFrmTasks.begin();
            while (eraseIter != m_singleFrmTasks.end())
            {
                if ((*eraseIter)->frameIndex == hCandiFrame->frameIndex)
                    break;
                auto& delfrm = *eraseIter;
                delfrm->rft->SetDiscarded();
                m_logger->Log(DEBUG) << "---- Remove single-frame task, frameIndex=" << delfrm->frameIndex << endl;
                eraseIter = m_singleFrmTasks.erase(eraseIter);
            }
        }
        return hCandiFrame;
    }

    SingleFrameTask::Holder AddSingleFrameTask(int64_t frameIndex, bool canDrop, bool needSeek, bool clearBeforeAdd = false)
    {
        if (frameIndex < 0 || !m_track)
            return nullptr;
        lock_guard<recursive_mutex> lk(m_singleFrmTasksLock);
        list<SingleFrameTask::Holder>::iterator mftIter;
        if (clearBeforeAdd)
        {
            ClearAllSingleFrameTasks();
            mftIter = m_singleFrmTasks.end();
        }
        else
        {
            mftIter = find_if(m_singleFrmTasks.begin(), m_singleFrmTasks.end(), [frameIndex] (auto& t) {
                return t->frameIndex == frameIndex;
            });
        }
        SingleFrameTask::Holder hTask;
        if (mftIter == m_singleFrmTasks.end())
        {
            bool needClearTaskList = false;
            if (!m_singleFrmTasks.empty())
            {
                auto backTaskFrameIndex = m_singleFrmTasks.back()->frameIndex;
                needClearTaskList = m_readForward ? frameIndex < backTaskFrameIndex : frameIndex > backTaskFrameIndex;
            }
            if (needClearTaskList)
                ClearAllSingleFrameTasks();

            VideoTrack::Holder track;
            {
                lock_guard<recursive_mutex> trackLk(m_trackLock);
                track = m_track;
            }
            hTask = SingleFrameTask::Holder(new SingleFrameTask());
            hTask->frameIndex = frameIndex;
            auto rft = track->CreateReadFrameTask(frameIndex, canDrop, needSeek || needClearTaskList, false, dynamic_cast<ReadFrameTask::Callback*>(hTask.get()));
            rft->SetVisible(track->IsVisible());
            hTask->rft = rft;
            m_logger->Log(DEBUG) << "++ AddSingleFrameTask: frameIndex=" << frameIndex << ", canDrop=" << canDrop << endl;
            m_singleFrmTasks.push_back(hTask);
        }
        else
        {
            hTask = *mftIter;
        }
        return hTask;
    }

    SingleFrameTask::Holder AddSingleFrameTask(SingleFrameTask::Holder hMft, bool clearBeforeAdd)
    {
        int64_t frameIndex = hMft->frameIndex;
        lock_guard<recursive_mutex> lk(m_singleFrmTasksLock);
        if (clearBeforeAdd)
        {
            ClearAllSingleFrameTasks();
            m_singleFrmTasks.push_back(hMft);
            m_logger->Log(DEBUG) << "++ AddSingleFrameTask[2-0]: frameIndex=" << frameIndex << endl;
        }
        else
        {
            auto mftIter = find_if(m_singleFrmTasks.begin(), m_singleFrmTasks.end(), [frameIndex] (auto& t) {
                return t->frameIndex == frameIndex;
            });
            if (mftIter == m_singleFrmTasks.end())
            {
                bool needClearTaskList = false;
                if (!m_singleFrmTasks.empty())
                {
                    auto backTaskFrameIndex = m_singleFrmTasks.back()->frameIndex;
                    needClearTaskList = m_readForward ? frameIndex < backTaskFrameIndex : frameIndex > backTaskFrameIndex;
                }
                if (needClearTaskList)
                    ClearAllSingleFrameTasks();

                m_logger->Log(DEBUG) << "++ AddSingleFrameTask[2-1]: frameIndex=" << frameIndex << endl;
                m_singleFrmTasks.push_back(hMft);
            }
            else
            {
                hMft = *mftIter;
            }
        }
        return hMft;
    }

    void ClearAllSingleFrameTasks()
    {
        if (m_singleFrmTasks.empty())
            return;
        m_logger->Log(DEBUG) << "------ Clear All SingleFrameTask" << endl;
        lock_guard<recursive_mutex> lk(m_singleFrmTasksLock);
        auto mftIter = m_singleFrmTasks.begin();
        while (mftIter != m_singleFrmTasks.end())
        {
            auto& mft = *mftIter;
            mft->rft->SetDiscarded();
            mftIter = m_singleFrmTasks.erase(mftIter);
        }
    }

    SingleFrameTask::Holder FindSeekingFlashAndRemoveDeprecatedTasks(int64_t targetIndex)
    {
        SingleFrameTask::Holder hCandiFrame;
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
                delfrm->rft->SetDiscarded();
                m_logger->Log(DEBUG) << "---- Remove seeking task, frameIndex=" << delfrm->frameIndex << endl;
                sktIter = m_seekingTasks.erase(sktIter);
            }
        }
        return hCandiFrame;
    }

    SingleFrameTask::Holder AddSeekingTask(int64_t frameIndex)
    {
        if (frameIndex < 0)
            return nullptr;
        lock_guard<mutex> lk(m_seekingTasksLock);
        auto sktIter = find_if(m_seekingTasks.begin(), m_seekingTasks.end(), [frameIndex] (auto& t) {
            return t->frameIndex == frameIndex;
        });
        SingleFrameTask::Holder hTask;
        if (sktIter == m_seekingTasks.end())
        {
            VideoTrack::Holder track;
            {
                lock_guard<recursive_mutex> trackLk(m_trackLock);
                track = m_track;
            }
            hTask = SingleFrameTask::Holder(new SingleFrameTask(true));
            hTask->frameIndex = frameIndex;
            auto rft = track->CreateReadFrameTask(frameIndex, true, true, true, dynamic_cast<ReadFrameTask::Callback*>(hTask.get()));
            hTask->rft = rft;
            m_logger->Log(DEBUG) << "++ AddSeekingTask: frameIndex=" << frameIndex << endl;
            m_seekingTasks.push_back(hTask);
        }
        else
        {
            hTask = *sktIter;
        }
        return hTask;
    }

    SingleFrameTask::Holder ExtractSeekingTask(int64_t frameIndex)
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
            mft->rft->SetDiscarded();
            mftIter = m_seekingTasks.erase(mftIter);
        }
    }

    void RemoveDiscardedTasks(list<SingleFrameTask::Holder>& taskList)
    {
        auto iter = taskList.begin();
        while (iter != taskList.end())
        {
            auto& mft = *iter;
            if (mft->state == SingleFrameTask::DROP_BIT)
            {
                mft->rft->SetDiscarded();
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

            list<SingleFrameTask::Holder> frameTasks;
            if (m_inSeeking)
            {
                if (prevInSeekingState != m_inSeeking)
                {
                    prevInSeekingState = m_inSeeking;
                    ClearAllSingleFrameTasks();
                }
                lock_guard<mutex> lk(m_seekingTasksLock);
                // auto statusLog = PrintMixFrameTaskListStatus(m_seekingTasks, "SeekingTasks");
                // m_logger->Log(DEBUG) << statusLog << endl;
                auto s1 = m_seekingTasks.size();
                RemoveDiscardedTasks(m_seekingTasks);
                frameTasks = m_seekingTasks;
            }
            else
            {
                if (prevInSeekingState != m_inSeeking)
                {
                    prevInSeekingState = m_inSeeking;
                    ClearAllSeekingTasks();
                }
                lock_guard<recursive_mutex> lk(m_singleFrmTasksLock);
                // auto statusLog = PrintMixFrameTaskListStatus(m_singleFrmTasks, "MixFrameTasks");
                // m_logger->Log(DEBUG) << statusLog << endl;
                RemoveDiscardedTasks(m_singleFrmTasks);
                frameTasks = m_singleFrmTasks;
            }
            auto sftIter = frameTasks.begin();
            while (sftIter != frameTasks.end())
            {
                auto& sft = *sftIter++;
                if (sft->outputReady || sft->IsProcessingStarted())
                    continue;

                bool allSourceReady = true;
                auto& rft = sft->rft;
                if (!rft->IsSourceFrameReady())
                    allSourceReady = false;
                else
                    rft->UpdateHostFrames();
                if (!allSourceReady)
                    continue;

                rft->UpdateHostFrames();
                m_seekingFlash = sft->GetOutputFrames();
                m_logger->Log(DEBUG) << "---------> Got SOURCE frame at frameIndex=" << sft->frameIndex << endl;
                sft->StartProcessing();
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

            list<SingleFrameTask::Holder> frameTasks;
            if (m_inSeeking)
            {
                lock_guard<mutex> lk(m_seekingTasksLock);
                frameTasks = m_seekingTasks;
            }
            else
            {
                lock_guard<recursive_mutex> lk(m_singleFrmTasksLock);
                frameTasks = m_singleFrmTasks;
            }
            auto sftIter = frameTasks.begin();
            while (sftIter != frameTasks.end())
            {
                auto& sft = *sftIter++;
                if (sft->outputReady || !sft->IsProcessingStarted())
                    continue;

                bool allProcessed = true;
                auto& rft = sft->rft;
                if (!rft->IsOutputFrameReady())
                    allProcessed = false;
                else
                    rft->UpdateHostFrames();
                if (!allProcessed)
                    continue;

                double timestamp = (double)sft->frameIndex*frameRate.den/frameRate.num;
                ImGui::ImMat outFrame;
                auto hVfrm = sft->rft->GetVideoFrame();
                if (hVfrm)
                    hVfrm->GetMat(outFrame);
                if (outFrame.empty())
                {
                    outFrame.create_type(outWidth, outHeight, 4, matDtype);
                    memset(outFrame.data, 0, outFrame.total()*outFrame.elemsize);
                }
                outFrame.time_stamp = timestamp;
                outFrame.flags |= IM_MAT_FLAGS_VIDEO_FRAME;
                outFrame.rate.num = frameRate.num;
                outFrame.rate.den = frameRate.den;
                outFrame.index_count = sft->frameIndex;
                sft->UpdateOutputFrames({ CorrelativeVideoFrame::Holder(new CorrelativeVideoFrame(CorrelativeFrame::PHASE_AFTER_MIXING, 0, 0, VideoFrame::CreateMatInstance(outFrame))) });
                sft->outputReady = true;
                // m_seekingFlash = sft->GetOutputFrames();
                m_logger->Log(DEBUG) << "---------> Got out frame at frameIndex=" << sft->frameIndex << ", pos=" << (int64_t)(timestamp*1000) << endl;
                idleLoop = false;
                break;
            }

            if (idleLoop)
                this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_TIME));
        }

        m_logger->Log(DEBUG) << "Leave MixingThreadProc2(VIDEO)." << endl;
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
    VideoTrack::Holder m_track;
    list<VideoTrack::Holder> m_tracks;
    recursive_mutex m_trackLock;
    VideoBlender::Holder m_hMixBlender;

    list<SingleFrameTask::Holder> m_singleFrmTasks;
    size_t m_szCacheFrameNum{1};
    recursive_mutex m_singleFrmTasksLock;
    SingleFrameTask::Holder m_prevOutFrame;
    bool m_inSeeking{false};
    list<SingleFrameTask::Holder> m_seekingTasks;
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

const uint8_t SingleTrackVideoReader_Impl::SingleFrameTask::DROP_BIT = 0x1;
const uint8_t SingleTrackVideoReader_Impl::SingleFrameTask::START_BIT = 0x2;

static const auto SINGLE_TRACK_VIDEO_READER_DELETER = [] (MultiTrackVideoReader* p) {
    SingleTrackVideoReader_Impl* ptr = dynamic_cast<SingleTrackVideoReader_Impl*>(p);
    ptr->Close();
    delete ptr;
};

MultiTrackVideoReader::Holder MultiTrackVideoReader::CreateSingleTrackInstance()
{
    return MultiTrackVideoReader::Holder(new SingleTrackVideoReader_Impl(), SINGLE_TRACK_VIDEO_READER_DELETER);
}

MultiTrackVideoReader::Holder SingleTrackVideoReader_Impl::CloneAndConfigure(SharedSettings::Holder hSettings)
{
    lock_guard<recursive_mutex> lk(m_apiLock);
    SingleTrackVideoReader_Impl* newInstance = new SingleTrackVideoReader_Impl();
    if (!newInstance->Configure(hSettings))
    {
        m_errMsg = newInstance->GetError();
        newInstance->Close(); delete newInstance;
        return nullptr;
    }

    // clone all the video tracks
    {
        lock_guard<recursive_mutex> lk2(m_trackLock);
        if (m_track)
            newInstance->m_track = m_track->Clone(hSettings);
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
    return MultiTrackVideoReader::Holder(newInstance, SINGLE_TRACK_VIDEO_READER_DELETER);
}
}
