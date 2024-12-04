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

#pragma once
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <ostream>
#include <string>
#include "immat.h"
#include "MediaCore.h"
#include "SharedSettings.h"
#include "VideoTrack.h"
#include "SubtitleTrack.h"
#include "Logger.h"

namespace MediaCore
{
struct MultiTrackVideoReader
{
    using Holder = std::shared_ptr<MultiTrackVideoReader>;
    static MEDIACORE_API Holder CreateInstance();
    static MEDIACORE_API Holder CreateSingleTrackInstance();
    static MEDIACORE_API Logger::ALogger* GetLogger();

    virtual bool Configure(SharedSettings::Holder hSettings) = 0;
    virtual bool Configure(uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate, ImDataType outDtype = IM_DT_FLOAT32) = 0;
    virtual Holder CloneAndConfigure(SharedSettings::Holder hSettings) = 0;
    virtual Holder CloneAndConfigure(uint32_t outWidth, uint32_t outHeight, const Ratio& frameRate, ImDataType outDtype = IM_DT_FLOAT32) = 0;
    virtual SharedSettings::Holder GetSharedSettings() = 0;

    virtual bool Start() = 0;
    virtual void Close() = 0;
    virtual VideoTrack::Holder AddTrack(int64_t trackId, int64_t insertAfterId = -1) = 0;  // insertAfterId: -1, insert after the tail; -2, insert before the head
    virtual VideoTrack::Holder RemoveTrackByIndex(uint32_t index) = 0;
    virtual VideoTrack::Holder RemoveTrackById(int64_t trackId) = 0;
    virtual bool ChangeTrackViewOrder(int64_t targetId, int64_t insertAfterId) = 0;
    virtual bool SetDirection(bool forward, int64_t pos = -1) = 0;
    virtual bool SeekTo(int64_t pos, bool bForceReseek = false) = 0;
    virtual bool SeekToByIdx(int64_t frmIdx, bool bForceReseek = false) = 0;
    virtual bool ConsecutiveSeek(int64_t pos) = 0;
    virtual bool StopConsecutiveSeek() = 0;
    virtual bool SetTrackVisible(int64_t id, bool visible) = 0;
    virtual bool IsTrackVisible(int64_t id) = 0;
    virtual bool ReadVideoFrameByPosEx(int64_t pos, std::vector<CorrelativeFrame>& frames, bool nonblocking = false, bool precise = true) = 0;
    virtual bool ReadVideoFrameByPos(int64_t pos, ImGui::ImMat& vmat, bool nonblocking = false) = 0;
    virtual bool ReadVideoFrameByIdxEx(int64_t frmIdx, std::vector<CorrelativeFrame>& frames, bool nonblocking = false, bool precise = true) = 0;
    virtual bool ReadVideoFrameByIdx(int64_t frmIdx, ImGui::ImMat& vmat, bool nonblocking = false) = 0;
    virtual bool ReadNextVideoFrameEx(std::vector<CorrelativeFrame>& frames) = 0;
    virtual bool ReadNextVideoFrame(ImGui::ImMat& vmat) = 0;
    virtual int64_t MillsecToFrameIndex(int64_t mts, int iMode = 0) = 0;  // iMode: 1 -> round, 2 -> cell, other -> floor
    virtual int64_t FrameIndexToMillsec(int64_t frmIdx) = 0;
    virtual void UpdateDuration() = 0;
    virtual bool Refresh(bool updateDuration = true) = 0;
    virtual bool RefreshTrackView(const std::unordered_set<int64_t>& trackIds) = 0;
    virtual bool UpdateSettings(SharedSettings::Holder hSettings) = 0;
    virtual size_t GetCacheFrameNum() const = 0;
    virtual void SetCacheFrameNum(size_t szCacheNum) = 0;

    virtual int64_t Duration() const = 0;
    virtual int64_t ReadPos() const = 0;

    virtual uint32_t TrackCount() const = 0;
    virtual std::list<VideoTrack::Holder>::iterator TrackListBegin() = 0;
    virtual std::list<VideoTrack::Holder>::iterator TrackListEnd() = 0;
    virtual VideoTrack::Holder GetTrackByIndex(uint32_t idx) = 0;
    virtual VideoTrack::Holder GetTrackById(int64_t trackId, bool createIfNotExists = false) = 0;
    virtual VideoClip::Holder GetClipById(int64_t clipId) = 0;
    virtual VideoOverlap::Holder GetOverlapById(int64_t ovlpId) = 0;

    virtual SubtitleTrackHolder BuildSubtitleTrackFromFile(int64_t id, const std::string& url, int64_t insertAfterId = -1) = 0;
    virtual SubtitleTrackHolder NewEmptySubtitleTrack(int64_t id, int64_t insertAfterId = -1) = 0;
    virtual SubtitleTrackHolder GetSubtitleTrackById(int64_t trackId) = 0;
    virtual SubtitleTrackHolder RemoveSubtitleTrackById(int64_t trackId) = 0;
    virtual bool ChangeSubtitleTrackViewOrder(int64_t targetId, int64_t insertAfterId = -1) = 0;

    virtual std::string GetError() const = 0;
};

MEDIACORE_API std::ostream& operator<<(std::ostream& os, MultiTrackVideoReader::Holder hMtvReader);
}
