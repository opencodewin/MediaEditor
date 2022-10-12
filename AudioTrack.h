#pragma once
#include <functional>
#include <mutex>
#include <list>
#include "AudioClip.h"

namespace DataLayer
{
    class AudioTrack;
    using AudioTrackHolder = std::shared_ptr<AudioTrack>;

    class AudioTrack
    {
    public:
        AudioTrack(int64_t id, uint32_t outChannels, uint32_t outSampleRate);
        AudioTrack(const AudioTrack&) = delete;
        AudioTrack(AudioTrack&&) = delete;
        AudioTrack& operator=(const AudioTrack&) = delete;
        AudioTrackHolder Clone(uint32_t outChannels, uint32_t outSampleRate);

        AudioClipHolder AddNewClip(int64_t clipId, MediaParserHolder hParser, int64_t start, int64_t startOffset, int64_t endOffset);
        void InsertClip(AudioClipHolder hClip);
        void MoveClip(int64_t id, int64_t start);
        void ChangeClipRange(int64_t id, int64_t startOffset, int64_t endOffset);
        AudioClipHolder RemoveClipById(int64_t clipId);
        AudioClipHolder RemoveClipByIndex(uint32_t index);

        AudioClipHolder GetClipByIndex(uint32_t index);
        AudioClipHolder GetClipById(int64_t id);
        AudioOverlapHolder GetOverlapById(int64_t id);
        uint32_t ClipCount() const { return m_clips.size(); }
        std::list<AudioClipHolder>::iterator ClipListBegin() { return m_clips.begin(); }
        std::list<AudioClipHolder>::iterator ClipListEnd() { return m_clips.end(); }
        uint32_t OverlapCount() const { return m_overlaps.size(); }
        std::list<AudioOverlapHolder>::iterator OverlapListBegin() { return m_overlaps.begin(); }
        std::list<AudioOverlapHolder>::iterator OverlapListEnd() { return m_overlaps.end(); }

        void SeekTo(int64_t pos);
        void ReadAudioSamples(uint8_t* buf, uint32_t& size, double& pos);
        ImGui::ImMat ReadAudioSamples(uint32_t readSamples);
        void SetDirection(bool forward);

        int64_t Id() const { return m_id; }
        int64_t Duration() const { return m_duration; }
        uint32_t OutChannels() const { return m_outChannels; }
        uint32_t OutSampleRate() const { return m_outSampleRate; }
        uint32_t OutFrameSize() const { return m_frameSize; }

    private:
        static std::function<bool(const AudioClipHolder&, const AudioClipHolder&)> CLIP_SORT_CMP;
        static std::function<bool(const AudioOverlapHolder&, const AudioOverlapHolder&)> OVERLAP_SORT_CMP;
        bool CheckClipRangeValid(int64_t clipId, int64_t start, int64_t end);
        void UpdateClipOverlap(AudioClipHolder hClip, bool remove = false);
        uint32_t ReadClipData(uint8_t* buf, uint32_t size);

    private:
        int64_t m_id;
        std::recursive_mutex m_apiLock;
        uint32_t m_outChannels;
        uint32_t m_outSampleRate;
        uint32_t m_bytesPerSample{4};
        uint32_t m_frameSize;
        uint32_t m_pcmSizePerSec;
        std::list<AudioClipHolder> m_clips;
        std::list<AudioClipHolder>::iterator m_readClipIter;
        std::list<AudioOverlapHolder> m_overlaps;
        std::list<AudioOverlapHolder>::iterator m_readOverlapIter;
        int64_t m_readSamples{0};
        int64_t m_duration{0};
        bool m_readForward{true};
    };
}