#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "MediaCore.h"
#include "Logger.h"

namespace MediaCore
{
struct HwaccelManager
{
    using Holder = std::shared_ptr<HwaccelManager>;
    static MEDIACORE_API Holder CreateInstance();
    static MEDIACORE_API Holder GetDefaultInstance();

    virtual bool Init() = 0;

    struct HwaccelTypeInfo
    {
        std::string name;
        bool usable;
    };
    virtual std::vector<const HwaccelTypeInfo*> GetHwaccelTypes() = 0;

    enum CodecType
    {
        DECODER = 0x1,
        ENCODER = 0x2,
        VIDEO = 0x10,
        AUDIO = 0x20,
    };
    virtual std::vector<const HwaccelTypeInfo*> GetHwaccelTypesForCodec(const std::string& codecName, uint32_t codecTypeFlag) = 0;
    virtual void IncreaseDecoderInstanceCount(const std::string& devType) = 0;
    virtual void DecreaseDecoderInstanceCount(const std::string& devType) = 0;

    virtual void SetLogLevel(Logger::Level l) = 0;
    virtual std::string GetError() const = 0;
};
}