#pragma once
#include <memory>
#include <atomic>
#include "MediaReader.h"

namespace DataLayer
{
    class AudioClip;
    using AudioClipHolder = std::shared_ptr<AudioClip>;

    class AudioClip
    {
    public:
        AudioClip(int64_t id, MediaParserHolder hParser, uint32_t outChannels, uint32_t outSampleRate, int64_t start, int64_t startOffset, int64_t endOffset, int64_t readPos);
        AudioClip(const AudioClip&) = delete;
        AudioClip(AudioClip&&) = delete;
        AudioClip& operator=(const AudioClip&) = delete;
        ~AudioClip();
        AudioClipHolder Clone(uint32_t outChannels, uint32_t outSampleRate) const;

        int64_t Id() const { return m_id; }
        int64_t TrackId() const { return m_trackId; }
        void SetTrackId(int64_t trackId) { m_trackId = trackId; }
        MediaParserHolder GetMediaParser() const { return m_srcReader->GetMediaParser(); }
        void SetStart(int64_t start) { m_start = start; }
        int64_t Duration() const { return m_srcDuration-m_startOffset-m_endOffset; }
        int64_t Start() const { return m_start; }
        int64_t End() const { return m_start+Duration(); }
        int64_t StartOffset() const { return m_startOffset; }
        int64_t EndOffset() const { return m_endOffset; }
        int64_t ReadPos() const { return m_readPos; }
        
        bool IsStartOffsetValid(int64_t startOffset);
        void ChangeStartOffset(int64_t startOffset);
        bool IsEndOffsetValid(int64_t endOffset);
        void ChangeEndOffset(int64_t endOffset);

        void SeekTo(int64_t pos);
        void ReadAudioSamples(uint8_t* buf, uint32_t& size, bool& eof);
        uint32_t ReadAudioSamples(ImGui::ImMat& amat, uint32_t readSamples, bool& eof);
        void SetDirection(bool forward);

    private:
        int64_t m_id;
        int64_t m_trackId{-1};
        MediaInfo::InfoHolder m_hInfo;
        MediaReader* m_srcReader;
        int64_t m_srcDuration;
        int64_t m_start;
        int64_t m_startOffset;
        int64_t m_endOffset;
        uint32_t m_pcmSizePerSec{0};
        uint32_t m_pcmFrameSize{0};
        int64_t m_readPos{0};
        bool m_eof{false};
    };

    struct AudioFilter
    {
        virtual ~AudioFilter() {}
        virtual void ApplyTo(AudioClip* clip) = 0;
        virtual ImGui::ImMat FilterPcm(const ImGui::ImMat& amat, int64_t pos) = 0;
    };

    struct AudioTransition;
    using AudioTransitionHolder = std::shared_ptr<AudioTransition>;

    class AudioOverlap
    {
    public:
        static bool HasOverlap(AudioClipHolder hClip1, AudioClipHolder hClip2)
        {
            return (hClip1->Start() >= hClip2->Start() && hClip1->Start() < hClip2->End()) ||
                   (hClip1->End() > hClip2->Start() && hClip1->End() <= hClip2->End()) ||
                   (hClip1->Start() < hClip2->Start() && hClip1->End() > hClip2->End());
        }

        AudioOverlap(int64_t id, AudioClipHolder hClip1, AudioClipHolder hClip2);

        void Update();
        void SetTransition(AudioTransitionHolder trans);

        int64_t Id() const { return m_id; }
        void SetId(int64_t id) { m_id = id; }
        int64_t Start() const { return m_start; }
        int64_t End() const { return m_end; }
        int64_t Duration() const { return m_end-m_start; }
        AudioClipHolder FrontClip() const { return m_frontClip; }
        AudioClipHolder RearClip() const { return m_rearClip; }

        void SeekTo(int64_t pos);
        uint32_t ReadAudioSamples(uint8_t* buf, uint32_t size, bool& eof);
        void ReadAudioSamples(ImGui::ImMat& amat, uint32_t readSamples, bool& eof);

        friend std::ostream& operator<<(std::ostream& os, AudioOverlap& overlap);

    private:
        int64_t m_id;
        AudioClipHolder m_frontClip;
        AudioClipHolder m_rearClip;
        int64_t m_start{0};
        int64_t m_end{0};
        AudioTransitionHolder m_transition;
    };

    using AudioOverlapHolder = std::shared_ptr<AudioOverlap>;

    struct AudioTransition
    {
        virtual ~AudioTransition() {}
        virtual void ApplyTo(AudioOverlap* overlap) = 0;
        virtual ImGui::ImMat MixTwoAudioMats(const ImGui::ImMat& amat1, const ImGui::ImMat& amat2, int64_t pos) = 0;
        virtual void MixTwoPcmBuffers(const uint8_t* src1, const uint8_t* src2, uint8_t* dst, uint32_t size, int64_t pos) = 0;
    };
}
