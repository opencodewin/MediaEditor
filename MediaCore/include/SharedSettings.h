#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "MediaCore.h"
#include "MediaInfo.h"
#include "HwaccelManager.h"
#include "immat.h"
#include "imgui_json.h"

namespace MediaCore
{

struct SharedSettings
{
    using Holder = std::shared_ptr<SharedSettings>;
    static MEDIACORE_API Holder CreateInstance();
    virtual Holder Clone() = 0;

    virtual uint32_t VideoOutWidth() const = 0;
    virtual uint32_t VideoOutHeight() const = 0;
    virtual Ratio VideoOutFrameRate() const = 0;
    virtual ImColorFormat VideoOutColorFormat() const = 0;
    virtual ImDataType VideoOutDataType() const = 0;
    virtual HwaccelManager::Holder GetHwaccelManager() const = 0;
    virtual bool IsVideoSrcKeepOriginalSize() const = 0;
    virtual uint32_t AudioOutChannels() const = 0;
    virtual uint32_t AudioOutSampleRate() const = 0;
    virtual ImDataType AudioOutDataType() const = 0;
    virtual bool AudioOutIsPlanar() const = 0;
    virtual std::string AudioOutSampleFormatName() const = 0;

    virtual void SetVideoOutWidth(uint32_t width) = 0;
    virtual void SetVideoOutHeight(uint32_t height) = 0;
    virtual void SetVideoOutFrameRate(const Ratio& frameRate) = 0;
    virtual void SetVideoOutColorFormat(ImColorFormat colorFormat) = 0;
    virtual void SetVideoOutDataType(ImDataType dataType) = 0;
    virtual void SetHwaccelManager(HwaccelManager::Holder hHwaMgr) = 0;
    virtual void SetVideoSrcKeepOriginalSize(bool enable) = 0;
    virtual void SetAudioOutChannels(uint32_t channels) = 0;
    virtual void SetAudioOutSampleRate(uint32_t sampleRate) = 0;
    virtual void SetAudioOutDataType(ImDataType dataType) = 0;
    virtual void SetAudioOutIsPlanar(bool isPlanar) = 0;

    virtual void SyncVideoSettingsFrom(const SharedSettings* pSettings) = 0;
    virtual void SyncAudioSettingsFrom(const SharedSettings* pSettings) = 0;

    virtual bool SaveAsJson(imgui_json::value& jnSettings) const = 0;
    static MEDIACORE_API Holder CreateInstanceFromJson(const imgui_json::value& jnSettings);
};

}