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
#include <string>
#include <list>
#include "Logger.h"
#include "MediaCore.h"
#include "immat.h"

namespace MediaCore
{
    struct AudioEffectFilter
    {
        using Holder = std::shared_ptr<AudioEffectFilter>;
        static MEDIACORE_API Holder CreateInstance(const std::string& loggerName = "");
        static MEDIACORE_API Logger::ALogger* GetLogger();

        static MEDIACORE_API const uint32_t VOLUME;
        static MEDIACORE_API const uint32_t PAN;
        static MEDIACORE_API const uint32_t GATE;
        static MEDIACORE_API const uint32_t LIMITER;
        static MEDIACORE_API const uint32_t EQUALIZER;
        static MEDIACORE_API const uint32_t COMPRESSOR;

        virtual bool Init(uint32_t composeFlags, const std::string& sampleFormat, uint32_t channels, uint32_t sampleRate) = 0;
        virtual bool ProcessData(const ImGui::ImMat& in, std::list<ImGui::ImMat>& out) = 0;
        virtual bool HasFilter(uint32_t composeFlags) const = 0;
        virtual void CopyParamsFrom(AudioEffectFilter* pAeFilter) = 0;

        struct VolumeParams
        {
            float volume{1.f};
        };
        virtual bool SetVolumeParams(VolumeParams* params) = 0;
        virtual VolumeParams GetVolumeParams() const = 0;

        struct PanParams
        {
            float x{0.5f};
            float y{0.5f};
        };
        virtual bool SetPanParams(PanParams* params) = 0;
        virtual PanParams GetPanParams() const = 0;

        struct LimiterParams
        {
            float limit{1.f};
            float attack{5};
            float release{50};
        };
        virtual bool SetLimiterParams(LimiterParams* params) = 0;
        virtual LimiterParams GetLimiterParams() const = 0;

        struct GateParams
        {
            float threshold{0};
            float range{0};
            float ratio{2};
            float attack{20};
            float release{250};
            float makeup{1};
            float knee{2.82843f};
        };
        virtual bool SetGateParams(GateParams* params) = 0;
        virtual GateParams GetGateParams() const = 0;

        struct CompressorParams
        {
            float threshold{1};
            float ratio{2};
            float knee{2.82843f};
            float mix{1};
            float attack{20};
            float release{250};
            float makeup{1};
            float levelIn{1};
        };
        virtual bool SetCompressorParams(CompressorParams* params) = 0;
        virtual CompressorParams GetCompressorParams() const = 0;

        struct EqualizerBandInfo
        {
            uint32_t bandCount;
            const uint32_t *centerFreqList;
            const uint32_t *bandWidthList;
        };
        virtual EqualizerBandInfo GetEqualizerBandInfo() const = 0;

        struct EqualizerParams
        {
            int32_t gain;
        };
        virtual bool SetEqualizerParamsByIndex(EqualizerParams* params, uint32_t index) = 0;
        virtual EqualizerParams GetEqualizerParamsByIndex(uint32_t index) const = 0;

        virtual void SetMuted(bool muted) = 0;
        virtual bool IsMuted() const = 0;

        virtual std::string GetError() const = 0;
    };
}