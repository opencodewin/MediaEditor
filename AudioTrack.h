#pragma once
#include <mutex>
#include <list>
#include "AudioClip.h"

class AudioTrack
{
public:
    AudioTrack(uint32_t outChannels, uint32_t outSampleRate);
    AudioTrack(const AudioTrack&) = delete;
    AudioTrack(AudioTrack&&) = delete;
    AudioTrack& operator=(const AudioTrack&) = delete;

    uint32_t AddNewClip(const std::string& url, double timeLineOffset, double startOffset, double endOffset);
    uint32_t AddNewClip(MediaParserHolder hParser, double timeLineOffset, double startOffset, double endOffset);
    void InsertClip(AudioClipHolder hClip, double timeLineOffset);
    AudioClipHolder RemoveClip(uint32_t clipId);

    std::list<AudioClipHolder>::iterator ClipListBegin() { return m_clips.begin(); }
    std::list<AudioClipHolder>::iterator ClipListEnd() { return m_clips.end(); }

    void SeekTo(double pos);
    void ReadAudioSamples(uint8_t* buf, uint32_t& size);

    double Duration() const { return m_duration; }

private:
    std::recursive_mutex m_apiLock;
    uint32_t m_outChannels;
    uint32_t m_outSampleRate;
    uint32_t m_audFrameSize;
    std::list<AudioClipHolder> m_clips;
    std::list<AudioClipHolder>::iterator m_iterRead;
    int64_t m_readSamples{0};
    double m_duration{0};
};

using AudioTrackHolder = std::shared_ptr<AudioTrack>;