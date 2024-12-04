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
#include <vector>
#include "immat.h"
#include "MediaParser.h"
#include "Logger.h"
#include "MediaCore.h"

namespace MediaCore
{
struct Overview
{
    using Holder = std::shared_ptr<Overview>;
    static MEDIACORE_API Holder CreateInstance();
    static MEDIACORE_API Logger::ALogger* GetLogger();

    virtual bool Open(const std::string& url, uint32_t snapshotCount = 20) = 0;
    virtual bool Open(MediaParser::Holder hParser, uint32_t snapshotCount = 20) = 0;
    virtual MediaParser::Holder GetMediaParser() const = 0;
    virtual void Close() = 0;
    virtual bool GetSnapshots(std::vector<ImGui::ImMat>& snapshots) = 0;

    struct Waveform
    {
        using Holder = std::shared_ptr<Waveform>;
        double aggregateSamples;
        double aggregateDuration;
        float minSample{0}, maxSample{0};
        std::vector<std::vector<float>> pcm;
        int64_t validSampleCount{0};
        bool parseDone{false};
    };
    virtual Waveform::Holder GetWaveform() const = 0;
    virtual bool SetSingleFramePixels(uint32_t pixels) = 0;
    virtual bool SetFixedAggregateSamples(double aggregateSamples) = 0;

    virtual bool IsOpened() const = 0;
    virtual bool IsDone() const = 0;
    virtual bool HasVideo() const = 0;
    virtual bool HasAudio() const = 0;
    virtual uint32_t GetSnapshotCount() const = 0;

    virtual bool SetSnapshotSize(uint32_t width, uint32_t height) = 0;
    virtual bool SetSnapshotResizeFactor(float widthFactor, float heightFactor) = 0;
    virtual bool SetKeepAspectRatio(bool bEnable) = 0;
    virtual bool SetOutColorFormat(ImColorFormat clrfmt) = 0;
    virtual bool SetResizeInterpolateMode(ImInterpolateMode interp) = 0;

    virtual MediaInfo::Holder GetMediaInfo() const = 0;
    virtual const VideoStream* GetVideoStream() const = 0;
    virtual const AudioStream* GetAudioStream() const = 0;

    virtual uint32_t GetVideoWidth() const = 0;
    virtual uint32_t GetVideoHeight() const = 0;
    virtual int64_t GetVideoDuration() const = 0;
    virtual int64_t GetVideoFrameCount() const = 0;
    virtual uint32_t GetAudioChannel() const = 0;
    virtual uint32_t GetAudioSampleRate() const = 0;
    virtual void GetSnapshotSize(uint32_t& width, uint32_t& height) const = 0;
    virtual bool IsKeepAspectRatio() const = 0;

    virtual bool IsHwAccelEnabled() const = 0;
    virtual void EnableHwAccel(bool enable) = 0;
    virtual std::string GetError() const = 0;
};
}
