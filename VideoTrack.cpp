#include <sstream>
#include <algorithm>
#include "VideoTrack.h"

using namespace std;

namespace DataLayer
{
    std::function<bool(const VideoClipHolder&, const VideoClipHolder&)> VideoTrack::CLIP_SORT_CMP =
        [](const VideoClipHolder& a, const VideoClipHolder& b)
        {
            return a->Start() < b->Start();
        };

    std::function<bool(const VideoOverlapHolder&, const VideoOverlapHolder&)> VideoTrack::OVERLAP_SORT_CMP =
        [](const VideoOverlapHolder& a, const VideoOverlapHolder& b)
        {
            return a->Start() < b->Start();
        };

    VideoTrack::VideoTrack(int64_t id, uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate)
        : m_id(id), m_outWidth(outWidth), m_outHeight(outHeight), m_frameRate(frameRate)
    {
        m_readClipIter = m_clips.begin();
    }

    VideoTrackHolder VideoTrack::Clone(uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        VideoTrackHolder newInstance = VideoTrackHolder(new VideoTrack(m_id, outWidth, outHeight, frameRate));
        // duplicate the clips
        for (auto clip : m_clips)
        {
            auto newClip = clip->Clone(outWidth, outHeight, frameRate);
            newInstance->m_clips.push_back(newClip);
            newClip->SetTrackId(m_id);
            auto filter = clip->GetFilter();
            if (filter)
                newClip->SetFilter(filter->Clone());
            VideoClipHolder lastClip = newInstance->m_clips.back();
            newInstance->m_duration = lastClip->Start()+lastClip->Duration();
            newInstance->UpdateClipOverlap(newClip);
        }
        // clone the transitions on the overlaps
        for (auto overlap : m_overlaps)
        {
            auto iter = find_if(newInstance->m_overlaps.begin(), newInstance->m_overlaps.end(), [overlap] (auto& ovlp) {
                return overlap->FrontClip()->Id() == ovlp->FrontClip()->Id() && overlap->RearClip()->Id() == ovlp->RearClip()->Id();
            });
            if (iter != newInstance->m_overlaps.end())
            {
                auto trans = overlap->GetTransition();
                if (trans)
                    (*iter)->SetTransition(trans->Clone());
            }
        }
        return newInstance;
    }

    VideoClipHolder VideoTrack::AddNewClip(int64_t clipId, MediaParserHolder hParser, int64_t start, int64_t startOffset, int64_t endOffset, int64_t readPos)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        VideoClipHolder hClip = VideoClip::CreateVideoInstance(clipId, hParser, m_outWidth, m_outHeight, m_frameRate, start, startOffset, endOffset, readPos-start);
        InsertClip(hClip);
        return hClip;
    }

    void VideoTrack::InsertClip(VideoClipHolder hClip)
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
        VideoClipHolder lastClip = m_clips.back();
        m_duration = lastClip->Start()+lastClip->Duration();
        // update overlap
        UpdateClipOverlap(hClip);
        // call 'SeekTo()' to update iterators
        const int64_t readPos = (int64_t)((double)m_readFrames*1000*m_frameRate.den/m_frameRate.num);
        SeekTo(readPos);
    }

    void VideoTrack::MoveClip(int64_t id, int64_t start)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        VideoClipHolder hClip = GetClipById(id);
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
        VideoClipHolder lastClip = m_clips.back();
        m_duration = lastClip->Start()+lastClip->Duration();
        // update overlap
        UpdateClipOverlap(hClip);
        // call 'SeekTo()' to update iterators
        const int64_t readPos = (int64_t)((double)m_readFrames*1000*m_frameRate.den/m_frameRate.num);
        SeekTo(readPos);
    }

    void VideoTrack::ChangeClipRange(int64_t id, int64_t startOffset, int64_t endOffset)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        VideoClipHolder hClip = GetClipById(id);
        if (!hClip)
            throw invalid_argument("Invalid value for argument 'id'!");

        bool rangeChanged = false;
        if (startOffset != hClip->StartOffset())
        {
            int64_t bias = startOffset-hClip->StartOffset();
            hClip->ChangeStartOffset(startOffset);
            hClip->SetStart(hClip->Start()+bias);
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
        VideoClipHolder lastClip = m_clips.back();
        m_duration = lastClip->Start()+lastClip->Duration();
        // update overlap
        UpdateClipOverlap(hClip);
        // call 'SeekTo()' to update iterators
        const int64_t readPos = (int64_t)((double)m_readFrames*1000*m_frameRate.den/m_frameRate.num);
        SeekTo(readPos);
    }

    VideoClipHolder VideoTrack::RemoveClipById(int64_t clipId)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        auto iter = find_if(m_clips.begin(), m_clips.end(), [clipId](const VideoClipHolder& clip) {
            return clip->Id() == clipId;
        });
        if (iter == m_clips.end())
            return nullptr;

        VideoClipHolder hClip = (*iter);
        m_clips.erase(iter);
        hClip->SetTrackId(-1);
        UpdateClipOverlap(hClip, true);
        // call 'SeekTo()' to update iterators
        const int64_t readPos = (int64_t)((double)m_readFrames*1000*m_frameRate.den/m_frameRate.num);
        SeekTo(readPos);

        if (m_clips.empty())
            m_duration = 0;
        else
        {
            VideoClipHolder lastClip = m_clips.back();
            m_duration = lastClip->Start()+lastClip->Duration();
        }
        return hClip;
    }

    VideoClipHolder VideoTrack::RemoveClipByIndex(uint32_t index)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        if (index >= m_clips.size())
            throw invalid_argument("Argument 'index' exceeds the count of clips!");

        auto iter = m_clips.begin();
        while (index > 0)
        {
            iter++; index--;
        }

        VideoClipHolder hClip = (*iter);
        m_clips.erase(iter);
        hClip->SetTrackId(-1);
        UpdateClipOverlap(hClip, true);
        // call 'SeekTo()' to update iterators
        const int64_t readPos = (int64_t)((double)m_readFrames*1000*m_frameRate.den/m_frameRate.num);
        SeekTo(readPos);

        if (m_clips.empty())
            m_duration = 0;
        else
        {
            VideoClipHolder lastClip = m_clips.back();
            m_duration = lastClip->Start()+lastClip->Duration();
        }
        return hClip;
    }

    void VideoTrack::SeekTo(int64_t pos)
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
                    const VideoClipHolder& hClip = *iter;
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
                    const VideoOverlapHolder& hOverlap = *iter;
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
                    const VideoClipHolder& hClip = *riter;
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
                    const VideoOverlapHolder& hOverlap = *riter;
                    int64_t overlapPos = pos-hOverlap->Start();
                    if (m_readOverlapIter == m_overlaps.end() && overlapPos >= 0)
                        m_readOverlapIter = riter.base();
                    riter++;
                }
            }
        }

        m_readFrames = (int64_t)(pos*m_frameRate.num/(m_frameRate.den*1000));
    }

    void VideoTrack::ReadVideoFrame(vector<CorrelativeFrame>& frames, ImGui::ImMat& out)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);

        const int64_t readPos = (int64_t)((double)m_readFrames*1000*m_frameRate.den/m_frameRate.num);
        for (auto& clip : m_clips)
            clip->NotifyReadPos(readPos-clip->Start());

        if (m_readForward)
        {
            // first, find the image from a overlap
            bool readFromeOverlay = false;
            while (m_readOverlapIter != m_overlaps.end() && readPos >= (*m_readOverlapIter)->Start())
            {
                auto& hOverlap = *m_readOverlapIter;
                bool eof = false;
                if (readPos < hOverlap->End())
                {
                    hOverlap->ReadVideoFrame(readPos-hOverlap->Start(), frames, out, eof);
                    readFromeOverlay = true;
                    break;
                }
                else
                    m_readOverlapIter++;
            }

            if (!readFromeOverlay)
            {
                // then try to read the image from a clip
                while (m_readClipIter != m_clips.end() && readPos >= (*m_readClipIter)->Start())
                {
                    auto& hClip = *m_readClipIter;
                    bool eof = false;
                    if (readPos < hClip->End())
                    {
                        hClip->ReadVideoFrame(readPos-hClip->Start(), frames, out, eof);
                        break;
                    }
                    else
                        m_readClipIter++;
                }
            }

            out.time_stamp = (double)readPos/1000;
            m_readFrames++;
        }
        else
        {
            if (!m_overlaps.empty())
            {
                if (m_readOverlapIter == m_overlaps.end()) m_readOverlapIter--;
                while (m_readOverlapIter != m_overlaps.begin() && readPos < (*m_readOverlapIter)->Start())
                    m_readOverlapIter--;
                auto& hOverlap = *m_readOverlapIter;
                bool eof = false;
                if (readPos >= hOverlap->Start() && readPos < hOverlap->End())
                    hOverlap->ReadVideoFrame(readPos-hOverlap->Start(), frames, out, eof);
            }

            if (out.empty() && !m_clips.empty())
            {
                if (m_readClipIter == m_clips.end()) m_readClipIter--;
                while (m_readClipIter != m_clips.begin() && readPos < (*m_readClipIter)->Start())
                    m_readClipIter--;
                auto& hClip = *m_readClipIter;
                bool eof = false;
                if (readPos >= hClip->Start() && readPos < hClip->End())
                    hClip->ReadVideoFrame(readPos-hClip->Start(), frames, out, eof);
            }

            out.time_stamp = (double)readPos/1000;
            m_readFrames--;
        }
    }

    void VideoTrack::SetDirection(bool forward)
    {
        if (m_readForward == forward)
            return;
        m_readForward = forward;
        for (auto& clip : m_clips)
            clip->SetDirection(forward);
    }

    VideoClipHolder VideoTrack::GetClipByIndex(uint32_t index)
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

    VideoClipHolder VideoTrack::GetClipById(int64_t id)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        auto iter = find_if(m_clips.begin(), m_clips.end(), [id] (const VideoClipHolder& clip) {
            return clip->Id() == id;
        });
        if (iter != m_clips.end())
            return *iter;
        return nullptr;
    }

    VideoOverlapHolder VideoTrack::GetOverlapById(int64_t id)
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        auto iter = find_if(m_overlaps.begin(), m_overlaps.end(), [id] (const VideoOverlapHolder& ovlp) {
            return ovlp->Id() == id;
        });
        if (iter != m_overlaps.end())
            return *iter;
        return nullptr;
    }

    bool VideoTrack::CheckClipRangeValid(int64_t clipId, int64_t start, int64_t end)
    {
        for (auto& overlap : m_overlaps)
        {
            if (clipId == overlap->FrontClip()->Id() || clipId == overlap->RearClip()->Id())
                continue;
            if ((start > overlap->Start() && start < overlap->End()) ||
                (end > overlap->Start() && end < overlap->End()))
                return false;
        }
        return true;
    }

    void VideoTrack::UpdateClipOverlap(VideoClipHolder hUpdateClip, bool remove)
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
                if (VideoOverlap::HasOverlap(hUpdateClip, clip))
                {
                    const int64_t id2 = clip->Id();
                    auto iter = find_if(m_overlaps.begin(), m_overlaps.end(), [id1, id2] (const VideoOverlapHolder& overlap) {
                        const int64_t idf = overlap->FrontClip()->Id();
                        const int64_t idr = overlap->RearClip()->Id();
                        return (id1 == idf && id2 == idr) || (id1 == idr && id2 == idf);
                    });
                    if (iter == m_overlaps.end())
                    {
                        VideoOverlapHolder hOverlap(new VideoOverlap(0, hUpdateClip, clip));
                        m_overlaps.push_back(hOverlap);
                    }
                }
            }
        }

        // sort overlap by 'Start' time
        m_overlaps.sort(OVERLAP_SORT_CMP);
    }

    std::ostream& operator<<(std::ostream& os, VideoTrack& track)
    {
        os << "{ clips(" << track.m_clips.size() << "): [";
        auto clipIter = track.m_clips.begin();
        while (clipIter != track.m_clips.end())
        {
            os << *((*clipIter).get());
            clipIter++;
            if (clipIter != track.m_clips.end())
                os << ", ";
            else
                break;
        }
        os << "], overlaps(" << track.m_overlaps.size() << "): [";
        auto ovlpIter = track.m_overlaps.begin();
        while (ovlpIter != track.m_overlaps.end())
        {
            os << *((*ovlpIter).get());
            ovlpIter++;
            if (ovlpIter != track.m_overlaps.end())
                os << ", ";
            else
                break;
        }
        os << "] }";
        return os;
    }
}
