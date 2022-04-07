#include <sstream>
#include <algorithm>
#include "AudioTrack.h"

using namespace std;

namespace DataLayer
{
    std::function<bool(const AudioClipHolder&, const AudioClipHolder&)> AudioTrack::CLIP_SORT_CMP =
        [](const AudioClipHolder& a, const AudioClipHolder& b)
        {
            return a->Start() < b->Start();
        };

    std::function<bool(const AudioOverlapHolder&, const AudioOverlapHolder&)> AudioTrack::OVERLAP_SORT_CMP =
        [](const AudioOverlapHolder& a, const AudioOverlapHolder& b)
        {
            return a->Start() < b->Start();
        };

    AudioTrack::AudioTrack(int64_t id, uint32_t outChannels, uint32_t outSampleRate)
        : m_id(id), m_outChannels(outChannels), m_outSampleRate(outSampleRate)
    {
        m_readClipIter = m_clips.begin();
        m_frameSize = outChannels*m_bytesPerSample;
        m_pcmSizePerSec = m_frameSize*m_outSampleRate;
    }

    AudioClipHolder AudioTrack::AddNewClip(int64_t clipId, MediaParserHolder hParser, int64_t start, int64_t startOffset, int64_t endOffset)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        int64_t readPos = m_readSamples*1000/m_outSampleRate;
        AudioClipHolder hClip(new AudioClip(clipId, hParser, m_outChannels, m_outSampleRate, start, startOffset, endOffset, readPos));
        InsertClip(hClip);
        return hClip;
    }

    void AudioTrack::InsertClip(AudioClipHolder hClip)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (!CheckClipRangeValid(hClip->Id(), hClip->Start(), hClip->End()))
            throw invalid_argument("Invalid argument for inserting clip!");

        // add this clip into clip list
        hClip->SetDirection(m_readForward);
        m_clips.push_back(hClip);
        hClip->SetTrackId(m_id);
        m_clips.sort(CLIP_SORT_CMP);
        // update track duration
        AudioClipHolder lastClip = m_clips.back();
        m_duration = lastClip->Start()+lastClip->Duration();
        // update overlap
        UpdateClipOverlap(hClip);
    }

    void AudioTrack::MoveClip(int64_t id, int64_t start)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        AudioClipHolder hClip = GetClipById(id);
        if (!hClip)
            throw invalid_argument("Invalid value for argument 'id'!");

        if (hClip->Start() == start)
            return;
        else
            hClip->SetStart(start);

        if (!CheckClipRangeValid(id, hClip->Start(), hClip->End()))
            throw invalid_argument("Invalid argument for moving clip!");

        // update clip order
        m_clips.sort(CLIP_SORT_CMP);
        // update track duration
        AudioClipHolder lastClip = m_clips.back();
        m_duration = lastClip->Start()+lastClip->Duration();
        // update overlap
        UpdateClipOverlap(hClip);
    }

    void AudioTrack::ChangeClipRange(int64_t id, int64_t startOffset, int64_t endOffset)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        AudioClipHolder hClip = GetClipById(id);
        if (!hClip)
            throw invalid_argument("Invalid value for argument 'id'!");

        bool rangeChanged = false;
        if (startOffset != hClip->StartOffset())
        {
            hClip->ChangeStartOffset(startOffset);
            rangeChanged = true;
        }
        if (endOffset != hClip->EndOffset())
        {
            hClip->ChangeEndOffset(endOffset);
            rangeChanged = true;
        }
        if (!rangeChanged)
            return;

        if (!CheckClipRangeValid(id, hClip->Start(), hClip->End()))
            throw invalid_argument("Invalid argument for changing clip range!");

        // update clip order
        m_clips.sort(CLIP_SORT_CMP);
        // update track duration
        AudioClipHolder lastClip = m_clips.back();
        m_duration = lastClip->Start()+lastClip->Duration();
        // update overlap
        UpdateClipOverlap(hClip);
    }

    AudioClipHolder AudioTrack::RemoveClipById(int64_t clipId)
    {
         lock_guard<recursive_mutex> lk(m_apiLock);
        auto iter = find_if(m_clips.begin(), m_clips.end(), [clipId](const AudioClipHolder& clip) {
            return clip->Id() == clipId;
        });
        if (iter == m_clips.end())
            return nullptr;

        AudioClipHolder hClip = (*iter);
        m_clips.erase(iter);
        hClip->SetTrackId(-1);
        UpdateClipOverlap(hClip, true);

        if (m_clips.empty())
            m_duration = 0;
        else
        {
            AudioClipHolder lastClip = m_clips.back();
            m_duration = lastClip->Start()+lastClip->Duration();
        }
        return hClip;
    }

    AudioClipHolder AudioTrack::RemoveClipByIndex(uint32_t index)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (index >= m_clips.size())
            throw invalid_argument("Argument 'index' exceeds the count of clips!");

        auto iter = m_clips.begin();
        while (index > 0)
        {
            iter++; index--;
        }

        AudioClipHolder hClip = (*iter);
        m_clips.erase(iter);
        hClip->SetTrackId(-1);
        UpdateClipOverlap(hClip, true);

        if (m_clips.empty())
            m_duration = 0;
        else
        {
            AudioClipHolder lastClip = m_clips.back();
            m_duration = lastClip->Start()+lastClip->Duration();
        }
        return hClip;
    }

    void AudioTrack::SeekTo(int64_t pos)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (pos < 0)
            throw invalid_argument("Argument 'pos' can NOT be NEGATIVE!");

        if (m_readForward)
        {
            // update read clip iterator
            m_readClipIter = m_clips.end();
            {
                auto iter = m_clips.begin();
                while (iter != m_clips.end())
                {
                    const AudioClipHolder& hClip = *iter;
                    int64_t clipPos = pos-hClip->Start();
                    hClip->SeekTo(clipPos);
                    if (m_readClipIter == m_clips.end() && clipPos < hClip->Duration())
                        m_readClipIter = iter;
                    iter++;
                }
            }
            // update read overlap iterator
            m_readOverlapIter = m_overlaps.end();
            {
                auto iter = m_overlaps.begin();
                while (iter != m_overlaps.end())
                {
                    const AudioOverlapHolder& hOverlap = *iter;
                    int64_t overlapPos = pos-hOverlap->Start();
                    if (m_readOverlapIter == m_overlaps.end() && overlapPos < hOverlap->Duration())
                    {
                        m_readOverlapIter = iter;
                        break;
                    }
                    iter++;
                }
            }
        }
        else
        {
            m_readClipIter = m_clips.end();
            {
                auto riter = m_clips.rbegin();
                while (riter != m_clips.rend())
                {
                    const AudioClipHolder& hClip = *riter;
                    int64_t clipPos = pos-hClip->Start();
                    hClip->SeekTo(clipPos);
                    if (m_readClipIter == m_clips.end() && clipPos >= 0)
                        m_readClipIter = riter.base();
                    riter++;
                }
            }
            m_readOverlapIter = m_overlaps.end();
            {
                auto riter = m_overlaps.rbegin();
                while (riter != m_overlaps.rend())
                {
                    const AudioOverlapHolder& hOverlap = *riter;
                    int64_t overlapPos = pos-hOverlap->Start();
                    if (m_readOverlapIter == m_overlaps.end() && overlapPos >= 0)
                        m_readOverlapIter = riter.base();
                    riter++;
                }
            }
        }

        m_readSamples = pos*m_outSampleRate/1000;
    }

    void AudioTrack::ReadAudioSamples(uint8_t* buf, uint32_t& size)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        uint32_t readSize = 0;
        if (m_overlaps.empty())
        {
            readSize = ReadClipData(buf, size);
            size = readSize;
            return;
        }

        if (m_readForward)
        {
            int64_t readPosBegin = m_readSamples*1000/m_outSampleRate;
            int64_t readPosEnd = (m_readSamples+size/m_frameSize)*1000/m_outSampleRate;
            uint8_t* readbufptr = buf;
            uint32_t toRead, readBytes;
            while (readSize < size && m_readOverlapIter != m_overlaps.end() && (*m_readOverlapIter)->Start() < readPosEnd)
            {
                auto& ovlp = *m_readOverlapIter;
                if (ovlp->Start() > readPosBegin)
                {
                    toRead = (ovlp->Start()-readPosBegin)*m_pcmSizePerSec/1000;
                    toRead -= toRead%m_frameSize;
                    if (toRead > size)
                        toRead = size;
                    readBytes = ReadClipData(readbufptr, toRead);
                    if (readBytes < toRead)
                    {
                        memset(readbufptr+readBytes, 0, toRead-readBytes);
                        readBytes = toRead;
                    }
                    readbufptr += readBytes;
                    m_readSamples += readBytes/m_frameSize;
                    readSize += readBytes;
                }
                if (readSize >= size)
                    break;

                bool eof = false;
                toRead = size-readSize;
                readBytes = ovlp->ReadAudioSamples(readbufptr, toRead, eof);
                if (eof)
                    m_readOverlapIter++;
                readbufptr += readBytes;
                m_readSamples += readBytes/m_frameSize;
                readSize += readBytes;
            }
            if (readSize < size)
            {
                toRead = size-readSize;
                readBytes = ReadClipData(readbufptr, toRead);
                readbufptr += readBytes;
                m_readSamples += readBytes/m_frameSize;
                readSize += readBytes;
            }
        }
        else
        {
            if (m_readOverlapIter == m_overlaps.end()) m_readOverlapIter--;
            int64_t readPosBegin = m_readSamples*1000/m_outSampleRate;
            int64_t readPosEnd = (m_readSamples-size/m_frameSize)*1000/m_outSampleRate;
            uint8_t* readbufptr = buf;
            uint32_t toRead, readBytes;
            while (readSize < size && (m_readOverlapIter != m_overlaps.begin() || readPosBegin > (*m_readOverlapIter)->Start()))
            {
                auto& ovlp = *m_readOverlapIter;
                if (readPosBegin > ovlp->End())
                {
                    toRead = (readPosBegin-ovlp->End())*m_pcmSizePerSec/1000;
                    toRead -= toRead%m_frameSize;
                    if (toRead > size)
                        toRead = size;
                    readBytes = ReadClipData(readbufptr, toRead);
                    if (readBytes < toRead)
                    {
                        memset(readbufptr+readBytes, 0, toRead-readBytes);
                        readBytes = toRead;
                    }
                    readbufptr += readBytes;
                    m_readSamples += readBytes/m_frameSize;
                    readSize += readBytes;
                }
                if (readSize >= size)
                    break;

                bool eof = false;
                toRead = size-readSize;
                readBytes = ovlp->ReadAudioSamples(readbufptr, toRead, eof);
                readbufptr += readBytes;
                m_readSamples -= readBytes/m_frameSize;
                readSize += readBytes;
                if (eof && m_readOverlapIter != m_overlaps.begin())
                    m_readOverlapIter--;
            }
            if (readSize < size)
            {
                toRead = size-readSize;
                readBytes = ReadClipData(readbufptr, toRead);
                readbufptr += readBytes;
                m_readSamples -= readBytes/m_frameSize;
                readSize += readBytes;
            }
        }
        size = readSize;
    }

    uint32_t AudioTrack::ReadClipData(uint8_t* buf, uint32_t size)
    {
        uint32_t readSize = 0;
        if (m_readForward)
        {
            if (m_readClipIter == m_clips.end())
                return 0;
            do {
                int64_t readPos = m_readSamples*1000/m_outSampleRate;
                if (readPos < (*m_readClipIter)->Start())
                {
                    int64_t skipSamples = (*m_readClipIter)->Start()*m_outSampleRate/1000-m_readSamples;
                    if (skipSamples > 0)
                    {
                        if (skipSamples*m_frameSize > size-readSize)
                            skipSamples = (size-readSize)/m_frameSize;
                        uint32_t skipSize = (uint32_t)(skipSamples*m_frameSize);
                        memset(buf+readSize, 0, skipSize);
                        readSize += skipSize;
                        m_readSamples += skipSamples;
                    }
                }
                if (readSize >= size)
                    break;

                uint8_t* readPtr = buf+readSize;
                uint32_t toReadSize = size-readSize;
                bool eof = false;
                (*m_readClipIter)->ReadAudioSamples(readPtr, toReadSize, eof);
                readSize += toReadSize;
                m_readSamples += toReadSize/m_frameSize;
                if (eof)
                    m_readClipIter++;
            } while (readSize < size && m_readClipIter != m_clips.end());
        }
        else
        {
            if (m_readSamples <= 0 || m_clips.empty())
                return 0;
            if (m_readClipIter == m_clips.end())
                m_readClipIter--;
            do
            {
                int64_t readPos = m_readSamples*1000/m_outSampleRate;
                if (readPos > (*m_readClipIter)->End())
                {
                    int64_t skipSamples = m_readSamples-(*m_readClipIter)->End()*m_outSampleRate/1000;
                    if (skipSamples > 0)
                    {
                        if (skipSamples*m_frameSize > size-readSize)
                            skipSamples = (size-readSize)/m_frameSize;
                        uint32_t skipSize = (uint32_t)(skipSamples*m_frameSize);
                        memset(buf+readSize, 0, skipSize);
                        readSize += skipSize;
                        m_readSamples -= skipSamples;
                    }
                }
                if (readSize >= size || m_readSamples <= 0)
                    break;

                uint8_t* readPtr = buf+readSize;
                uint32_t toRead = size-readSize;
                bool eof = false;
                (*m_readClipIter)->ReadAudioSamples(readPtr, toRead, eof);
                readSize += toRead;
                m_readSamples -= toRead/m_frameSize;
                if (eof)
                {
                    if (m_readClipIter != m_clips.begin())
                        m_readClipIter--;
                    else
                        break;
                }
            } while (readSize < size && m_readSamples > 0);
        }
        return readSize;
    }

    void AudioTrack::SetDirection(bool forward)
    {
        if (m_readForward == forward)
            return;
        m_readForward = forward;
        for (auto& clip : m_clips)
            clip->SetDirection(forward);
    }

    AudioClipHolder AudioTrack::GetClipByIndex(uint32_t index)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (index >= m_clips.size())
            return nullptr;
        auto iter = m_clips.begin();
        while (index > 0)
        {
            iter++; index--;
        }
        return *iter;
    }

    AudioClipHolder AudioTrack::GetClipById(int64_t id)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        auto iter = find_if(m_clips.begin(), m_clips.end(), [id] (const AudioClipHolder& clip) {
            return clip->Id() == id;
        });
        if (iter != m_clips.end())
            return *iter;
        return nullptr;
    }

    bool AudioTrack::CheckClipRangeValid(int64_t clipId, int64_t start, int64_t end)
    {
        for (auto& overlap : m_overlaps)
        {
            if (clipId == overlap->FrontClip()->Id() || clipId == overlap->RearClip()->Id())
                continue;
            if (start > overlap->Start() && start < overlap->End() ||
                end > overlap->Start() && end < overlap->End())
                return false;
        }
        return true;
    }

    void AudioTrack::UpdateClipOverlap(AudioClipHolder hUpdateClip, bool remove)
    {
        const int64_t id1 = hUpdateClip->Id();
        // remove invalid overlaps
        auto ovIter = m_overlaps.begin();
        while (ovIter != m_overlaps.end())
        {
            auto& hOverlap = *ovIter;
            if (hOverlap->FrontClip()->TrackId() != m_id || hOverlap->RearClip()->TrackId() != m_id)
            {
                ovIter = m_overlaps.erase(ovIter);
                continue;
            }
            if (hOverlap->FrontClip()->Id() == id1 || hOverlap->RearClip()->Id() == id1)
            {
                hOverlap->Update();
                if (hOverlap->Duration() <= 0)
                {
                    ovIter = m_overlaps.erase(ovIter);
                    continue;
                }
            }
            ovIter++;
        }
        if (!remove)
        {
            // add new overlaps
            for (auto& clip : m_clips)
            {
                if (hUpdateClip == clip)
                    continue;
                if (AudioOverlap::HasOverlap(hUpdateClip, clip))
                {
                    const int64_t id2 = clip->Id();
                    auto iter = find_if(m_overlaps.begin(), m_overlaps.end(), [id1, id2] (const AudioOverlapHolder& overlap) {
                        const int64_t idf = overlap->FrontClip()->Id();
                        const int64_t idr = overlap->RearClip()->Id();
                        return id1 == idf && id2 == idr || id1 == idr && id2 == idf;
                    });
                    if (iter == m_overlaps.end())
                    {
                        AudioOverlapHolder hOverlap(new AudioOverlap(0, hUpdateClip, clip));
                        m_overlaps.push_back(hOverlap);
                    }
                }
            }
        }

        // sort overlap by 'Start' time
        m_overlaps.sort(OVERLAP_SORT_CMP);
    }
}