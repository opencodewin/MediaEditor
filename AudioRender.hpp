#pragma once
#include <cstdint>
#include <string>

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
};

AudioRender* CreateAudioRender();
void ReleaseAudioRender(AudioRender** audrnd);
