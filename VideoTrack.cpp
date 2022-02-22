#include <sstream>
#include <algorithm>
#include "VideoTrack.h"

using namespace std;

VideoTrack::VideoTrack(uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate)
    : m_outWidth(outWidth), m_outHeight(outHeight), m_frameRate(frameRate)
{
    m_iterRead = m_clips.begin();
}

uint32_t VideoTrack::AddNewClip(const std::string& url, double timeLineOffset, double startOffset, double endOffset)
{
    lock_guard<recursive_mutex> lk(m_apiLock);

    MediaParserHolder hParser = CreateMediaParser();
    if (!hParser->Open(url))
        throw runtime_error(hParser->GetError());

    return AddNewClip(hParser, timeLineOffset, startOffset, endOffset);
}

uint32_t VideoTrack::AddNewClip(MediaParserHolder hParser, double timeLineOffset, double startOffset, double endOffset)
{
    lock_guard<recursive_mutex> lk(m_apiLock);

    VideoClipHolder hClip(new VideoClip(hParser, m_outWidth, m_outHeight, m_frameRate, timeLineOffset, startOffset, endOffset));
    const uint32_t clipId = hClip->Id();
    InsertClip(hClip, timeLineOffset);
    SeekTo((double)m_readFrames*m_frameRate.den/m_frameRate.num);

    return clipId;
}

void VideoTrack::InsertClip(VideoClipHolder hClip, double timeLineOffset)
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
            VideoClipHolder h2ndClip(new VideoClip(
                (*iter)->GetMediaParser(), m_outWidth, m_outHeight, m_frameRate, clipEnd, (*iter)->StartOffset()+moveDistance, (*iter)->EndOffset()));
            m_clips.push_back(h2ndClip);
            moveDistance = iterEnd-clipStart;
            (*iter)->ChangeEndOffset((*iter)->EndOffset()-moveDistance);
            break;
        }
        else
            iter++;
    }
    m_clips.push_back(hClip);

    m_clips.sort([](const VideoClipHolder& a, const VideoClipHolder& b) {
        return a->TimeLineOffset() < b->TimeLineOffset();
    });

    VideoClipHolder lastClip = m_clips.back();
    m_duration = lastClip->TimeLineOffset()+lastClip->ClipDuration();
}

VideoClipHolder VideoTrack::RemoveClipById(uint32_t clipId)
{
    lock_guard<recursive_mutex> lk(m_apiLock);

    auto iter = find_if(m_clips.begin(), m_clips.end(), [clipId](const VideoClipHolder& clip) {
        return clip->Id() == clipId;
    });
    if (iter == m_clips.end())
        return nullptr;

    VideoClipHolder hClip = (*iter);
    m_clips.erase(iter);

    const double clipStart = (*iter)->TimeLineOffset();
    const double clipEnd = clipStart+(*iter)->ClipDuration();
    double readPos = (double)m_readFrames*m_frameRate.den/m_frameRate.num;
    if (readPos >= clipStart && readPos < clipEnd)
        SeekTo(readPos);

    if (m_clips.empty())
        m_duration = 0;
    else
    {
        VideoClipHolder lastClip = m_clips.back();
        m_duration = lastClip->TimeLineOffset()+lastClip->ClipDuration();
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

    const double clipStart = (*iter)->TimeLineOffset();
    const double clipEnd = clipStart+(*iter)->ClipDuration();
    double readPos = (double)m_readFrames*m_frameRate.den/m_frameRate.num;
    if (readPos >= clipStart && readPos < clipEnd)
        SeekTo(readPos);

    if (m_clips.empty())
        m_duration = 0;
    else
    {
        VideoClipHolder lastClip = m_clips.back();
        m_duration = lastClip->TimeLineOffset()+lastClip->ClipDuration();
    }
    return hClip;
}

void VideoTrack::SeekTo(double pos)
{
    lock_guard<recursive_mutex> lk(m_apiLock);
    if (pos < 0)
        throw invalid_argument("Argument 'pos' can NOT be NEGATIVE!");

    m_iterRead = m_clips.end();
    auto iter = m_clips.begin();
    while (iter != m_clips.end())
    {
        const VideoClipHolder& item = *iter;
        if (pos >= item->TimeLineOffset() && pos < item->TimeLineOffset()+item->ClipDuration())
        {
            (*iter)->SeekTo(pos-(*iter)->TimeLineOffset());
            m_iterRead = iter++;
            break;
        }
        else if (pos < item->TimeLineOffset())
        {
            (*iter)->SeekTo(0);
            m_iterRead = iter++;
            break;
        }
        iter++;
    }
    while (iter != m_clips.end())
    {
        (*iter)->SeekTo(0);
        iter++;
    }
    m_readFrames = (int64_t)(pos*m_frameRate.num/m_frameRate.den);
}

void VideoTrack::ReadVideoFrame(ImGui::ImMat& vmat)
{
    lock_guard<recursive_mutex> lk(m_apiLock);
    vmat.release();
    double readPos = (double)m_readFrames*m_frameRate.den/m_frameRate.num;
    while (m_iterRead != m_clips.end() && readPos >= (*m_iterRead)->TimeLineOffset())
    {
        auto& hClip = *m_iterRead;
        bool eof = false;
        if (readPos < hClip->TimeLineOffset()+hClip->ClipDuration())
        {
            hClip->ReadVideoFrame(readPos-hClip->TimeLineOffset(), vmat, eof);
            break;
        }
        else
            m_iterRead++;
    }
    vmat.time_stamp = readPos;
    m_readFrames++;
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

void VideoTrack::ChangeClip(uint32_t id, double timeLineOffset, double startOffset, double endOffset)
{
    lock_guard<recursive_mutex> lk(m_apiLock);
    VideoClipHolder hClip = RemoveClipById(id);
    if (!hClip)
        throw invalid_argument("Invalid value for argument 'id'!");
    if (timeLineOffset >= 0)
        hClip->SetTimeLineOffset(timeLineOffset);
    else
        timeLineOffset = hClip->TimeLineOffset();
    if (startOffset >= 0)
        hClip->ChangeStartOffset(startOffset);
    if (endOffset <= 0)
        hClip->ChangeEndOffset(endOffset);
    InsertClip(hClip, timeLineOffset);
    SeekTo((double)m_readFrames*m_frameRate.den/m_frameRate.num);
}