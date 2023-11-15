#include "Snapshot.h"
#include "MediaReader.h"
#include "AudioRender.h"
#include <chrono>

using Clock = std::chrono::steady_clock;

namespace MEC
{
    class SimplePcmStream : public MediaCore::AudioRender::ByteStream
    {
    public:
        SimplePcmStream(MediaCore::MediaReader::Holder audrdr) : m_audrdr(audrdr) {}

        uint32_t Read(uint8_t* buff, uint32_t buffSize, bool blocking) override
        {
            if (!m_audrdr)
                return 0;
            uint32_t readSize = buffSize;
            int64_t pos;
            bool eof;
            if (!m_audrdr->ReadAudioSamples(buff, readSize, pos, eof, blocking))
                return 0;
            m_audPos = (double)pos/1000;
            return readSize;
        }

        void Flush() override {}

        bool GetTimestampMs(int64_t& ts) override
        {
            return false;
        }

    public:
        double m_audPos {0};

    private:
        MediaCore::MediaReader::Holder m_audrdr;
    };

    class MediaPlayer
    {
    public:
        MediaPlayer();
        ~MediaPlayer();
    public:
        RenderUtils::TextureManager::Holder m_txmgr;
        RenderUtils::ManagedTexture::Holder m_tx;
        bool m_isOpening {false};
        MediaCore::MediaParser::Holder m_mediaParser;
        bool m_useHwAccel {true};
        int32_t m_audioStreamCount {0};
        int32_t m_chooseAudioIndex {-1};
        MediaCore::MediaReader::Holder m_vidrdr; // video
        double m_playStartPos = 0.f;
        Clock::time_point m_playStartTp;
        bool m_isPlay {false};
        MediaCore::MediaReader::Holder m_audrdr; // audio
        MediaCore::AudioRender* m_audrnd {nullptr};
        MediaCore::AudioRender::PcmFormat c_audioRenderFormat {MediaCore::AudioRender::PcmFormat::FLOAT32};
        int c_audioRenderChannels {2};
        int c_audioRenderSampleRate {44100};
        SimplePcmStream* m_pcmStream {nullptr};
    };
}