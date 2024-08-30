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
#include <functional>
#include <mutex>
#include <list>
#include "MediaCore.h"
#include "SharedSettings.h"
#include "VideoClip.h"

namespace MediaCore
{
struct ReadFrameTask
{
    using Holder = std::shared_ptr<ReadFrameTask>;

    virtual int64_t FrameIndex() const = 0;
    virtual bool IsSourceFrameReady() const = 0;
    virtual void StartProcessing() = 0;
    virtual void Reprocess() = 0;
    virtual bool IsOutputFrameReady() const = 0;
    virtual VideoFrame::Holder GetVideoFrame() = 0;
    virtual bool IsStarted() const = 0;
    virtual void SetDiscarded() = 0;
    virtual bool IsDiscarded() const = 0;
    virtual bool IsVisible() const = 0;
    virtual void SetVisible(bool visible) = 0;
    virtual void UpdateHostFrames() = 0;

    struct Callback
    {
        virtual bool TriggerDrop() = 0;
        virtual bool TriggerStart() = 0;
        virtual void UpdateOutputFrames(const std::vector<CorrelativeVideoFrame::Holder>& corVidFrames) = 0;
    };
    virtual void SetCallback(Callback* pCallback) = 0;
};

struct VideoTrack
{
    using Holder = std::shared_ptr<VideoTrack>;
    static MEDIACORE_API Holder CreateInstance(int64_t id, SharedSettings::Holder hSettings);
    virtual Holder Clone(SharedSettings::Holder hSettings) = 0;

    virtual int64_t Id() const = 0;
    virtual uint32_t OutWidth() const = 0;
    virtual uint32_t OutHeight() const = 0;
    virtual Ratio FrameRate() const = 0;
    virtual int64_t Duration() const = 0;
    virtual void SetDirection(bool forward) = 0;
    virtual bool Direction() const = 0;
    virtual void SetVisible(bool visible) = 0;
    virtual bool IsVisible() const = 0;
    virtual ReadFrameTask::Holder CreateReadFrameTask(int64_t frameIndex, bool canDrop, bool needSeek, bool bypassBgNode, ReadFrameTask::Callback* pCb) = 0;

    virtual VideoClip::Holder AddVideoClip(int64_t clipId, MediaParser::Holder hParser, int64_t start, int64_t end, int64_t startOffset, int64_t endOffset, int64_t readPos) = 0;
    virtual VideoClip::Holder AddImageClip(int64_t clipId, MediaParser::Holder hParser, int64_t start, int64_t length) = 0;
    virtual void InsertClip(VideoClip::Holder hClip) = 0;
    virtual void MoveClip(int64_t id, int64_t start) = 0;
    virtual void ChangeClipRange(int64_t id, int64_t startOffset, int64_t endOffset) = 0;
    virtual VideoClip::Holder RemoveClipById(int64_t clipId) = 0;
    virtual VideoClip::Holder RemoveClipByIndex(uint32_t index) = 0;
    virtual VideoClip::Holder GetClipByIndex(uint32_t index) = 0;
    virtual VideoClip::Holder GetClipById(int64_t id) = 0;
    virtual VideoOverlap::Holder GetOverlapById(int64_t id) = 0;
    virtual void UpdateClipState() = 0;
    virtual void UpdateSettings(SharedSettings::Holder hSettings) = 0;
    virtual void SetPreReadMaxNum(int iMaxNum) = 0;

    virtual std::list<VideoClip::Holder> GetClipList() = 0;
    virtual std::list<VideoOverlap::Holder> GetOverlapList() = 0;

    virtual void SetLogLevel(Logger::Level l) = 0;
};

MEDIACORE_API std::ostream& operator<<(std::ostream& os, VideoTrack::Holder hTrack);
}