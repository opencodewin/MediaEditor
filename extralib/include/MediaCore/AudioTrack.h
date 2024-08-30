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
#include <mutex>
#include <list>
#include <string>
#include <ostream>
#include "MediaCore.h"
#include "SharedSettings.h"
#include "AudioClip.h"
#include "AudioEffectFilter.h"
#include "Logger.h"

namespace MediaCore
{
struct AudioTrack
{
    using Holder = std::shared_ptr<AudioTrack>;
    static MEDIACORE_API Holder CreateInstance(int64_t id, SharedSettings::Holder hSettings);
    static MEDIACORE_API Logger::ALogger* GetLogger();
    virtual Holder Clone(SharedSettings::Holder hSettings) = 0;
    virtual bool UpdateSettings(SharedSettings::Holder hSettings) = 0;

    virtual int64_t Id() const = 0;
    virtual int64_t Duration() const = 0;
    virtual uint32_t OutChannels() const = 0;
    virtual uint32_t OutSampleRate() const = 0;
    virtual std::string OutSampleFormat() const = 0;
    virtual uint32_t OutFrameSize() const = 0;
    virtual void SetDirection(bool forward) = 0;
    virtual void SetMuted(bool muted) = 0;
    virtual bool IsMuted() const = 0;
    virtual ImGui::ImMat ReadAudioSamples(uint32_t readSamples) = 0;
    virtual void SeekTo(int64_t pos) = 0;
    virtual AudioEffectFilter::Holder GetAudioEffectFilter() = 0;

    virtual AudioClip::Holder AddNewClip(int64_t clipId, MediaParser::Holder hParser, int64_t start, int64_t end, int64_t startOffset, int64_t endOffset) = 0;
    virtual void InsertClip(AudioClip::Holder hClip) = 0;
    virtual void MoveClip(int64_t id, int64_t start) = 0;
    virtual void ChangeClipRange(int64_t id, int64_t startOffset, int64_t endOffset) = 0;
    virtual AudioClip::Holder RemoveClipById(int64_t clipId) = 0;
    virtual AudioClip::Holder RemoveClipByIndex(uint32_t index) = 0;
    virtual AudioClip::Holder GetClipByIndex(uint32_t index) = 0;
    virtual AudioClip::Holder GetClipById(int64_t id) = 0;
    virtual AudioOverlap::Holder GetOverlapById(int64_t id) = 0;

    virtual uint32_t ClipCount() const = 0;
    virtual std::list<AudioClip::Holder>::iterator ClipListBegin() = 0;
    virtual std::list<AudioClip::Holder>::iterator ClipListEnd() = 0;
    virtual uint32_t OverlapCount() const = 0;
    virtual std::list<AudioOverlap::Holder>::iterator OverlapListBegin() = 0;
    virtual std::list<AudioOverlap::Holder>::iterator OverlapListEnd() = 0;
};

MEDIACORE_API std::ostream& operator<<(std::ostream& os, AudioTrack::Holder hTrack);
}