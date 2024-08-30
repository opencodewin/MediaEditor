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
#include <string>
#include <list>
#include "immat.h"
#include "MediaCore.h"
#include "SharedSettings.h"
#include "AudioTrack.h"
#include "AudioEffectFilter.h"
#include "Logger.h"

namespace MediaCore
{
struct MultiTrackAudioReader
{
    using Holder = std::shared_ptr<MultiTrackAudioReader>;
    static MEDIACORE_API Holder CreateInstance();
    static MEDIACORE_API Logger::ALogger* GetLogger();

    virtual bool Configure(SharedSettings::Holder hSettings, uint32_t outSamplesPerFrame = 1024) = 0;
    virtual bool Configure(uint32_t outChannels, uint32_t outSampleRate, const std::string& sampleFormat, uint32_t outSamplesPerFrame = 1024) = 0;
    virtual Holder CloneAndConfigure(SharedSettings::Holder hSettings, uint32_t outSamplesPerFrame) = 0;
    virtual Holder CloneAndConfigure(uint32_t outChannels, uint32_t outSampleRate, const std::string& sampleFormat, uint32_t outSamplesPerFrame) = 0;
    virtual SharedSettings::Holder GetSharedSettings() = 0;
    virtual SharedSettings::Holder GetTrackSharedSettings() = 0;
    virtual bool UpdateSettings(SharedSettings::Holder hSettings) = 0;

    virtual bool Start() = 0;
    virtual void Close() = 0;
    virtual AudioTrack::Holder AddTrack(int64_t trackId) = 0;
    virtual AudioTrack::Holder RemoveTrackByIndex(uint32_t index) = 0;
    virtual AudioTrack::Holder RemoveTrackById(int64_t trackId) = 0;
    virtual bool SetDirection(bool forward, int64_t pos = -1) = 0;
    virtual bool SeekTo(int64_t pos, bool probeMode = false) = 0;
    virtual bool SetTrackMuted(int64_t id, bool muted) = 0;
    virtual bool IsTrackMuted(int64_t id) = 0;
    virtual bool ReadAudioSamplesEx(std::vector<CorrelativeFrame>& amats, bool& eof) = 0;
    virtual bool ReadAudioSamples(ImGui::ImMat& amat, bool& eof) = 0;
    virtual void UpdateDuration() = 0;
    virtual bool Refresh(bool updateDuration = true) = 0;
    virtual int64_t SizeToDuration(uint32_t sizeInByte) = 0;

    virtual int64_t Duration() const = 0;
    virtual int64_t ReadPos() const = 0;

    virtual uint32_t TrackCount() const = 0;
    virtual std::list<AudioTrack::Holder>::iterator TrackListBegin() = 0;
    virtual std::list<AudioTrack::Holder>::iterator TrackListEnd() = 0;
    virtual AudioTrack::Holder GetTrackByIndex(uint32_t idx) = 0;
    virtual AudioTrack::Holder GetTrackById(int64_t trackId, bool createIfNotExists = false) = 0;
    virtual AudioClip::Holder GetClipById(int64_t clipId) = 0;
    virtual AudioOverlap::Holder GetOverlapById(int64_t ovlpId) = 0;
    virtual AudioEffectFilter::Holder GetAudioEffectFilter() = 0;

    virtual std::string GetError() const = 0;
};

MEDIACORE_API std::ostream& operator<<(std::ostream& os, MultiTrackAudioReader::Holder hMtaReader);
}
