#include <sstream>
#include <algorithm>
#include "AudioTrack.h"

using namespace std;

AudioTrack::AudioTrack(uint32_t outChannels, uint32_t outSampleRate)
    : m_outChannels(outChannels), m_outSampleRate(outSampleRate)
{
    m_iterRead = m_clips.begin();
    m_audFrameSize = outChannels*4;
}

uint32_t AudioTrack::AddNewClip(const std::string& url, double timeLineOffset, double startOffset, double endOffset)
{
    lock_guard<recursive_mutex> lk(m_apiLock);

    MediaParserHolder hParser = CreateMediaParser();
    if (!hParser->Open(url))
        throw runtime_error(hParser->GetError());

    return AddNewClip(hParser, timeLineOffset, startOffset, endOffset);
}

uint32_t AudioTrack::AddNewClip(MediaParserHolder hParser, double timeLineOffset, double startOffset, double endOffset)
{
    lock_guard<recursive_mutex> lk(m_apiLock);

    AudioClipHolder hClip(new AudioClip(hParser, m_outChannels, m_outSampleRate, timeLineOffset, startOffset, endOffset));
    const uint32_t clipId = hClip->Id();
    InsertClip(hClip, timeLineOffset);

    return clipId;
}

void AudioTrack::InsertClip(AudioClipHolder hClip, double timeLineOffset)
{
    lock_guard<recursive_mutex> lk(m_apiLock);

    const double clipDur = hClip->ClipDuration();
    const double clipStart = timeLineOffset;
    const double clipEnd = timeLineOffset+clipDur;
    auto iter = m_clips.begin();
    while (iter != m_clips.end())
    {
        const double iterStart = (*iter)->TimeLineOffset();
        const double iterEnd = iterStart+(*iter)->ClipDuration();
        if (iterStart >= clipStart && iterStart < clipEnd)
        {
            if (iterEnd <= clipEnd)
            {
                // clip (*iter) resides within the new added clip,
                // it should be removed.
                iter = m_clips.erase(iter);
            }
            else
            {
                // the front part of clip (*iter) resides within the new added clip,
                // it should increase its startOffset && timeLineOffset.
                double moveDistance = clipEnd-iterStart;
                (*iter)->ChangeStartOffset((*iter)->StartOffset()+moveDistance);
                (*iter)->SetTimeLineOffset(clipEnd);
            }
        }
        else if (iterEnd > clipStart && iterEnd <= clipEnd)
        {
            // the end part of clip (*iter) resides within the new added clip,
            // it should increase its endOffset.
            double moveDistance = iterEnd-clipStart;
            (*iter)->ChangeEndOffset((*iter)->EndOffset()-moveDistance);
        }
        else if (iterStart < clipStart && iterEnd > clipEnd)
        {
            // clip (*iter) is separated to two clips by the new added clip
            double moveDistance = clipEnd-iterStart;
            AudioClipHolder h2ndClip(new AudioClip(
                (*iter)->GetMediaParser(), m_outChannels, m_outSampleRate, clipEnd, (*iter)->StartOffset()+moveDistance, (*iter)->EndOffset()));
            m_clips.push_back(h2ndClip);
            moveDistance = iterEnd-clipStart;
            (*iter)->ChangeEndOffset((*iter)->EndOffset()-moveDistance);
        }
    }
    m_clips.push_back(hClip);

    m_clips.sort([](const AudioClipHolder& a, const AudioClipHolder& b) {
        return a->TimeLineOffset() > b->TimeLineOffset();
    });

    double readPos = (double)m_readSamples/m_outSampleRate;
    if (readPos >= clipStart && readPos < clipEnd || m_iterRead == m_clips.end())
        SeekTo(readPos);

    AudioClipHolder lastClip = m_clips.back();
    m_duration = lastClip->TimeLineOffset()+lastClip->ClipDuration();
}

AudioClipHolder AudioTrack::RemoveClip(uint32_t clipId)
{
    lock_guard<recursive_mutex> lk(m_apiLock);

    auto iter = find_if(m_clips.begin(), m_clips.end(), [clipId](const AudioClipHolder& clip) {
        return clip->Id() == clipId;
    });
    if (iter == m_clips.end())
    {
        ostringstream oss;
        oss << "Can NOT find clip with id #" << clipId << "!";
        throw invalid_argument(oss.str());
    }

    m_clips.sort([](const AudioClipHolder& a, const AudioClipHolder& b) {
        return a->TimeLineOffset() > b->TimeLineOffset();
    });

    const double clipStart = (*iter)->TimeLineOffset();
    const double clipEnd = clipStart+(*iter)->ClipDuration();
    double readPos = (double)m_readSamples/m_outSampleRate;
    if (readPos >= clipStart && readPos < clipEnd)
        SeekTo(readPos);

    AudioClipHolder lastClip = m_clips.back();
    m_duration = lastClip->TimeLineOffset()+lastClip->ClipDuration();
    return *iter;
}

void AudioTrack::SeekTo(double pos)
{
    lock_guard<recursive_mutex> lk(m_apiLock);

    if (pos < 0)
        throw invalid_argument("Argument 'pos' can NOT be NEGATIVE!");
    m_iterRead = find_if(m_clips.begin(), m_clips.end(), [pos](const AudioClipHolder& a) {
        return pos >= a->TimeLineOffset() && pos < a->TimeLineOffset()+a->ClipDuration() ||
            pos < a->TimeLineOffset();
    });
    if (m_iterRead != m_clips.end() && pos >= (*m_iterRead)->TimeLineOffset())
        (*m_iterRead)->SeekTo(pos-(*m_iterRead)->TimeLineOffset());
    m_readSamples = (int64_t)(pos*m_outSampleRate);
}

void AudioTrack::ReadAudioSamples(uint8_t* buf, uint32_t& size)
{
    lock_guard<recursive_mutex> lk(m_apiLock);

    if (m_iterRead == m_clips.end())
    {
        memset(buf, 0, size);
        m_readSamples += size/m_audFrameSize;
    }
    else
    {
        uint32_t readSize = 0;
        do {
            double readPos = (double)m_readSamples/m_outSampleRate;
            if (readPos < (*m_iterRead)->TimeLineOffset())
            {
                int64_t skipSamples = (int64_t)((*m_iterRead)->TimeLineOffset()*m_outSampleRate)-m_readSamples;
                if (skipSamples > 0)
                {
                    if (skipSamples*m_audFrameSize > size-readSize)
                        skipSamples = (size-readSize)/m_audFrameSize;
                    uint32_t skipSize = (uint32_t)(skipSamples*m_audFrameSize);
                    memset(buf+readSize, 0, skipSize);
                    readSize += skipSize;
                    m_readSamples += skipSamples;
                }
            }
            if (readSize >= size)
                break;

            uint8_t* readPtr = buf+readSize;
            uint32_t bufSize = size-readSize;
            bool eof = false;
            (*m_iterRead)->ReadAudioSamples(readPtr, bufSize, eof);
            readSize += bufSize;
            m_readSamples += bufSize/m_audFrameSize;
            if (eof)
                m_iterRead++;
        } while (readSize < size && m_iterRead != m_clips.end());
        if (readSize < size)
        {
            memset(buf+readSize, 0, size-readSize);
            m_readSamples += (size-readSize)/m_audFrameSize;
        }
    }
}