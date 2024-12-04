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

#include <cstdlib>
#include <sstream>
#include <SDL.h>
#include "AudioRender.h"
#include "Logger.h"

using namespace std;
using namespace Logger;

#define SDL_AUDIO_MIN_BUFFER_SIZE 512
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

#define MAX(a,b) ((a) > (b) ? (a) : (b))
const uint8_t log2_tab[256]=
{
        0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

static inline int log2_c(unsigned int v)
{
    int n = 0;
    if (v & 0xffff0000) 
    {
        v >>= 16;
        n += 16;
    }
    if (v & 0xff00) 
    {
        v >>= 8;
        n += 8;
    }
    n += log2_tab[v];
    return n;
}

namespace MediaCore
{
static void sdl_audio_callback(void *opaque, Uint8 *stream, int len);

class AudioRender_Impl_Sdl2 : public AudioRender
{
public:
    AudioRender_Impl_Sdl2() = default;
    AudioRender_Impl_Sdl2(const AudioRender_Impl_Sdl2&) = delete;
    AudioRender_Impl_Sdl2(AudioRender_Impl_Sdl2&&) = delete;
    AudioRender_Impl_Sdl2& operator=(const AudioRender_Impl_Sdl2&) = delete;

    virtual ~AudioRender_Impl_Sdl2()
    {
        if (m_initSdl)
            SDL_Quit();
    }

    bool Initialize() override
    {
        if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)
        {
            ostringstream oss;
            oss << "FAILED to invoke 'SDL_OpenAudioDevice()'! Error is '" << string(SDL_GetError()) << "'.";
            m_errMessage = oss.str();
            return false;
        }
        m_initSdl = true;
        return true;
    }

    bool OpenDevice(uint32_t sampleRate, uint32_t channels, PcmFormat format, ByteStream* pcmStream) override
    {
        CloseDevice();
        SDL_AudioSpec desiredAudSpec, obtainedAudSpec;
        desiredAudSpec.channels = channels;
        desiredAudSpec.freq = sampleRate;
        desiredAudSpec.format = PcmFormatToSDLAudioFormat(format);
        desiredAudSpec.silence = 0;
        desiredAudSpec.samples = MAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << log2_c(desiredAudSpec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
        desiredAudSpec.callback = sdl_audio_callback;
        desiredAudSpec.userdata = this;
        Log(DEBUG) << "[AudioRender_SDL2] Open device as: channels=" << channels << ", sample-rate=" << sampleRate << ", pcm-format=" << (int)format
                << ", buffered-samples=" << desiredAudSpec.samples << "." << endl;
        m_audDevId = SDL_OpenAudioDevice(NULL, 0, &desiredAudSpec, &obtainedAudSpec, 0);
        if (m_audDevId == 0)
        {
            ostringstream oss;
            oss << "FAILED to invoke 'SDL_OpenAudioDevice()'! Error is '" << string(SDL_GetError()) << "'.";
            m_errMessage = oss.str();
            return false;
        }
        m_sampleRate = sampleRate;
        m_channels = channels;
        m_pcmFormat = format;
        m_pcmStream = pcmStream;
        m_renderBufferSize = obtainedAudSpec.samples*GetBytesPerSampleByFormat(format)*channels;
        return true;
    }

    void CloseDevice() override
    {
        if (m_audDevId > 0)
        {
            SDL_CloseAudioDevice(m_audDevId);
            m_audDevId = 0;
        }
        m_sampleRate = 0;
        m_channels = 0;
        m_pcmFormat = PcmFormat::UNKNOWN;
        m_pcmStream = nullptr;
    }

    bool Pause() override
    {
        if (m_audDevId > 0)
            SDL_PauseAudioDevice(m_audDevId, 1);
        return true;
    }

    bool Resume() override
    {
        if (m_audDevId > 0)
            SDL_PauseAudioDevice(m_audDevId, 0);
        return true;
    }

    void Flush() override
    {
        if (m_audDevId > 0)
            SDL_ClearQueuedAudio(m_audDevId);
        if (m_pcmStream)
            m_pcmStream->Flush();
    }

    uint32_t GetBufferedDataSize() override
    {
        if (m_audDevId > 0)
            return SDL_GetQueuedAudioSize(m_audDevId)+m_renderBufferSize;
        return m_renderBufferSize;
    }

    string GetError() const override
    {
        return m_errMessage;
    }

    void ReadPcm(uint8_t* buf, uint32_t buffSize)
    {
        uint32_t readSize = m_pcmStream->Read(buf, buffSize, true);
        if (readSize < buffSize)
            memset(buf+readSize, 0, buffSize-readSize);
    }

private:
    static SDL_AudioFormat PcmFormatToSDLAudioFormat(PcmFormat format)
    {
        switch (format)
        {
            case PcmFormat::SINT16:
                return AUDIO_S16SYS;
            case PcmFormat::FLOAT32:
                return AUDIO_F32SYS;
            default:
                break;
        }
        return 0;
    }

private:
    bool m_initSdl{false};
    uint32_t m_sampleRate{0};
    uint32_t m_channels{0};
    PcmFormat m_pcmFormat{PcmFormat::UNKNOWN};
    ByteStream* m_pcmStream{nullptr};
    SDL_AudioDeviceID m_audDevId{0};
    std::string m_errMessage;
    int64_t m_pcmdataEndTimestamp{0};
    int32_t m_renderBufferSize{0};
};

void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    AudioRender_Impl_Sdl2* audrnd = static_cast<AudioRender_Impl_Sdl2*>(opaque);
    audrnd->ReadPcm(stream, len);
}

AudioRender* AudioRender::CreateInstance()
{
    return static_cast<AudioRender*>(new AudioRender_Impl_Sdl2());
}

void AudioRender::ReleaseInstance(AudioRender** audrnd)
{
    if (!audrnd)
        return;
    AudioRender_Impl_Sdl2* sdlrnd = dynamic_cast<AudioRender_Impl_Sdl2*>(*audrnd);
    if (!sdlrnd)
        return;
    sdlrnd->CloseDevice();
    delete sdlrnd;
    *audrnd = nullptr;
}

uint8_t AudioRender::GetBytesPerSampleByFormat(PcmFormat format)
{
    uint8_t bytesPerSample = 0;
    switch (format)
    {
    case PcmFormat::SINT16:
        return 2;
    case PcmFormat::FLOAT32:
        return 4;
    default:
        break;
    }
    return 0;
}
}