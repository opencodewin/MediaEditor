#pragma once
#include <SDL.h>
#include <SDL_thread.h>
#include "AudioRender.hpp"

class AudioRender_Impl_Sdl2 : public AudioRender
{
public:
    AudioRender_Impl_Sdl2() = default;
    AudioRender_Impl_Sdl2(const AudioRender_Impl_Sdl2&) = delete;
    AudioRender_Impl_Sdl2(AudioRender_Impl_Sdl2&&) = delete;
    AudioRender_Impl_Sdl2& operator=(const AudioRender_Impl_Sdl2&) = delete;
    virtual ~AudioRender_Impl_Sdl2();

    bool Initialize() override;
    bool OpenDevice(uint32_t sampleRate, uint32_t channels, PcmFormat format, ByteStream* pcmStream) override;
    void CloseDevice() override;
    bool Pause() override;
    bool Resume() override;
    void Flush() override;
    uint32_t GetBufferedDataSize() override;

    std::string GetError() const override;

    void ReadPcm(uint8_t* buf, uint32_t buffSize);

    static SDL_AudioFormat PcmFormatToSDLAudioFormat(PcmFormat format);

private:
    bool m_initSdl{false};
    uint32_t m_sampleRate{0};
    uint32_t m_channels{0};
    PcmFormat m_pcmFormat{PcmFormat::UNKNOWN};
    ByteStream* m_pcmStream{nullptr};
    SDL_AudioDeviceID m_audDevId{0};
    std::string m_errMessage;
    int64_t m_pcmdataEndTimestamp{0};
};