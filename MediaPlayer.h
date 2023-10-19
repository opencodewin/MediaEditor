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
            g_audPos = (double)pos/1000;
            return readSize;
        }

        void Flush() override {}

        bool GetTimestampMs(int64_t& ts) override
        {
            return false;
        }

    public:
        double g_audPos {0};

    private:
        MediaCore::MediaReader::Holder m_audrdr;
    };

    class MediaPlayer
    {
    public:
        MediaPlayer();
        ~MediaPlayer();
    public:
        RenderUtils::TextureManager::Holder g_txmgr;
        RenderUtils::ManagedTexture::Holder g_tx;
        bool g_isOpening {false};
        MediaCore::MediaParser::Holder g_mediaParser;
        bool g_useHwAccel {true};
        int32_t g_audioStreamCount {0};
        int32_t g_chooseAudioIndex {-1};
        MediaCore::MediaReader::Holder g_vidrdr; // video
        double g_playStartPos = 0.f;
        Clock::time_point g_playStartTp;
        bool g_isPlay {false};
        MediaCore::MediaReader::Holder g_audrdr; // audio
        MediaCore::AudioRender* g_audrnd {nullptr};
        MediaCore::AudioRender::PcmFormat c_audioRenderFormat {MediaCore::AudioRender::PcmFormat::FLOAT32};
        int c_audioRenderChannels {2};
        int c_audioRenderSampleRate {44100};
        SimplePcmStream* g_pcmStream {nullptr};
    };
}