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
#include <string>
#include "MediaCore.h"

namespace MediaCore
{
struct AudioRender
{
    enum class PcmFormat
    {
        UNKNOWN = 0,
        SINT16,
        FLOAT32,
    };

    struct ByteStream
    {
        virtual uint32_t Read(uint8_t* buff, uint32_t buffSize, bool blocking = false) = 0;
        virtual void Flush() = 0;
        virtual bool GetTimestampMs(int64_t& ts) = 0;
    };

    virtual bool Initialize() = 0;
    virtual bool OpenDevice(uint32_t sampleRate, uint32_t channels, PcmFormat format, ByteStream* pcmStream) = 0;
    virtual void CloseDevice() = 0;
    virtual bool Pause() = 0;
    virtual bool Resume() = 0;
    virtual void Flush() = 0;
    virtual uint32_t GetBufferedDataSize() = 0;

    virtual std::string GetError() const = 0;

    static MEDIACORE_API uint8_t GetBytesPerSampleByFormat(PcmFormat format);
    static MEDIACORE_API AudioRender* CreateInstance();
    static MEDIACORE_API void ReleaseInstance(AudioRender** audrnd);
};
}
