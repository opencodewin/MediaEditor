#include "AudioClip.h"

using namespace std;

atomic_uint32_t AudioClip::s_idCounter{1};

AudioClip::AudioClip(MediaParserHolder hParser, uint32_t outChannels, uint32_t outSampleRate, double timeLineOffset, double startOffset, double endOffset)
    : m_timeLineOffset(timeLineOffset)
{
    m_hInfo = hParser->GetMediaInfo();
    if (hParser->GetBestAudioStreamIndex() < 0)
        throw invalid_argument("Argument 'hParser' has NO AUDIO stream!");
    m_srcReader = CreateMediaReader();
    if (!m_srcReader->Open(hParser))
        throw runtime_error(m_srcReader->GetError());
    if (!m_srcReader->ConfigAudioReader(outChannels, outSampleRate))
        throw runtime_error(m_srcReader->GetError());
    m_srcDuration = m_srcReader->GetAudioStream()->duration;
    if (startOffset < 0)
        throw invalid_argument("Argument 'startOffset' can NOT be NEGATIVE!");
    if (endOffset > 0)
        throw invalid_argument("Argument 'endOffset' can NOT be POSITIVE!");
    if (startOffset-endOffset >= m_srcDuration)
        throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
    m_startOffset = startOffset;
    m_endOffset = endOffset;
    if (!m_srcReader->Start())
        throw runtime_error(m_srcReader->GetError());

    m_timeLineOffset = timeLineOffset;
    m_id = s_idCounter++;
}

AudioClip::~AudioClip()
{
    ReleaseMediaReader(&m_srcReader);
}

bool AudioClip::IsStartOffsetValid(double startOffset)
{
    if (startOffset < 0 || startOffset-m_endOffset >= m_srcDuration)
        return false;
    return true;
}

void AudioClip::ChangeStartOffset(double startOffset)
{
    if (startOffset < 0)
        throw invalid_argument("Argument 'startOffset' can NOT be NEGATIVE!");
    if (startOffset-m_endOffset >= m_srcDuration)
        throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
    m_startOffset = startOffset;
}

void AudioClip::ChangeEndOffset(double endOffset)
{
    if (endOffset > 0)
        throw invalid_argument("Argument 'endOffset' can NOT be POSITIVE!");
    if (m_startOffset-endOffset >= m_srcDuration)
        throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
    m_endOffset = endOffset;
}

void AudioClip::SeekTo(double pos)
{
    if (pos < 0 || pos >= ClipDuration())
        throw invalid_argument("Argument 'pos' can NOT be NEGATIVE or larger than clip duration!");
    if (!m_srcReader->SeekTo(pos+m_startOffset))
        throw runtime_error(m_srcReader->GetError());
}

void AudioClip::ReadAudioSamples(uint8_t* buf, uint32_t& size, bool& eof)
{
    uint32_t readSize = size;
    double pos;
    if (!m_srcReader->ReadAudioSamples(buf, readSize, pos, eof))
        throw runtime_error(m_srcReader->GetError());
    if (readSize < size)
        memset(buf+readSize, 0, size-readSize);
}
