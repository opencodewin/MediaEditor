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
#include <memory>
#include <atomic>
#include <string>
#include <ostream>
#include <imgui_json.h>
#include "MediaCore.h"
#include "SharedSettings.h"
#include "MediaReader.h"

namespace MediaCore
{
struct AudioClip;
struct AudioFilter
{
    using Holder = std::shared_ptr<AudioFilter>;

    virtual const std::string GetFilterName() const = 0;
    virtual Holder Clone() = 0;
    virtual void ApplyTo(AudioClip* clip) = 0;
    virtual const MediaCore::AudioClip* GetAudioClip() const = 0;
    virtual ImGui::ImMat FilterPcm(const ImGui::ImMat& amat, int64_t pos, int64_t dur) = 0;
    virtual imgui_json::value SaveAsJson() const = 0;
};

struct AudioClip
{
    using Holder = std::shared_ptr<AudioClip>;
    static MEDIACORE_API Holder CreateInstance(
        int64_t id, MediaParser::Holder hParser, SharedSettings::Holder hSettings,
        int64_t start, int64_t end, int64_t startOffset, int64_t endOffset);
    virtual Holder Clone(SharedSettings::Holder hSettings) const = 0;
    virtual bool UpdateSettings(SharedSettings::Holder hSettings) = 0;

    virtual MediaParser::Holder GetMediaParser() const = 0;
    virtual int64_t Id() const = 0;
    virtual int64_t TrackId() const = 0;
    virtual int64_t Start() const = 0;
    virtual int64_t End() const = 0;
    virtual int64_t StartOffset() const = 0;
    virtual int64_t EndOffset() const = 0;
    virtual int64_t Duration() const = 0;
    virtual int64_t ReadPos() const = 0;
    virtual uint32_t OutChannels() const = 0;
    virtual uint32_t OutSampleRate() const = 0;
    virtual uint32_t LeftSamples() const = 0;

    virtual void SetTrackId(int64_t trackId) = 0;
    virtual void SetStart(int64_t start) = 0;
    virtual void ChangeStartOffset(int64_t startOffset) = 0;
    virtual void ChangeEndOffset(int64_t endOffset) = 0;
    virtual void SeekTo(int64_t pos) = 0;
    virtual ImGui::ImMat ReadAudioSamples(uint32_t& readSamples, bool& eof) = 0;
    virtual void SetDirection(bool forward) = 0;
    virtual void SetFilter(AudioFilter::Holder filter) = 0;
    virtual AudioFilter::Holder GetFilter() const = 0;

    virtual void SetLogLevel(Logger::Level l) = 0;

    friend std::ostream& operator<<(std::ostream& os, Holder hClip);
};

struct AudioOverlap;
struct AudioTransition
{
    using Holder = std::shared_ptr<AudioTransition>;

    virtual void ApplyTo(AudioOverlap* overlap) = 0;
    virtual ImGui::ImMat MixTwoAudioMats(const ImGui::ImMat& amat1, const ImGui::ImMat& amat2, int64_t pos) = 0;
};

struct AudioOverlap
{
    using Holder = std::shared_ptr<AudioOverlap>;
    static MEDIACORE_API bool HasOverlap(AudioClip::Holder hClip1, AudioClip::Holder hClip2);
    static MEDIACORE_API Holder CreateInstance(int64_t id, AudioClip::Holder hClip1, AudioClip::Holder hClip2);

    virtual void Update() = 0;
    virtual AudioTransition::Holder GetTransition() const = 0;
    virtual void SetTransition(AudioTransition::Holder hTrans) = 0;

    virtual int64_t Id() const = 0;
    virtual void SetId(int64_t id) = 0;
    virtual int64_t Start() const = 0;
    virtual int64_t End() const = 0;
    virtual int64_t Duration() const = 0;
    virtual AudioClip::Holder FrontClip() const = 0;
    virtual AudioClip::Holder RearClip() const = 0;

    virtual void SeekTo(int64_t pos) = 0;
    virtual ImGui::ImMat ReadAudioSamples(uint32_t& readSamples, bool& eof) = 0;

    friend std::ostream& operator<<(std::ostream& os, const Holder& hOverlap);
};
}
