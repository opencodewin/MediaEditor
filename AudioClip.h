#pragma once
#include <string>
#include <memory>
#include <atomic>
#include "MediaReader.h"

class AudioClip
{
public:
    AudioClip(MediaParserHolder hParser, uint32_t outChannels, uint32_t outSampleRate, double timeLineOffset, double startOffset, double endOffset);
    AudioClip(const AudioClip&) = delete;
    AudioClip(AudioClip&&) = delete;
    AudioClip& operator=(const AudioClip&) = delete;
    ~AudioClip();

    uint32_t Id() const { return m_id; }
    MediaParserHolder GetMediaParser() const { return m_srcReader->GetMediaParser(); }
    double ClipDuration() const { return m_srcDuration+m_endOffset-m_startOffset; }
    double TimeLineOffset() const { return m_timeLineOffset; }
    void SetTimeLineOffset(double timeLineOffset) { m_timeLineOffset = timeLineOffset; }
    double StartOffset() const { return m_startOffset; }
    double EndOffset() const { return m_endOffset; }
    
    bool IsStartOffsetValid(double startOffset);
    void ChangeStartOffset(double startOffset);
    bool IsEndOffsetValid(double endOffset);
    void ChangeEndOffset(double endOffset);

    void SeekTo(double pos);
    void ReadAudioSamples(uint8_t* buf, uint32_t& size, bool& eof);

private:
    static std::atomic_uint32_t s_idCounter;
    uint32_t m_id;
    MediaInfo::InfoHolder m_hInfo;
    MediaReader* m_srcReader;
    double m_timeLineOffset;
    double m_srcDuration;
    double m_startOffset;
    double m_endOffset;
    uint32_t m_pcmSizePerSec{0};
    uint32_t m_pcmFrameSize{0};
    bool m_eof{false};
};

using AudioClipHolder = std::shared_ptr<AudioClip>;