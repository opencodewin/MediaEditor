#include "VideoClip.h"

using namespace std;

atomic_uint32_t VideoClip::s_idCounter{1};

VideoClip::VideoClip(MediaParserHolder hParser, uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate, double timeLineOffset, double startOffset, double endOffset)
    : m_timeLineOffset(timeLineOffset)
{
    m_hInfo = hParser->GetMediaInfo();
    if (hParser->GetBestVideoStreamIndex() < 0)
        throw invalid_argument("Argument 'hParser' has NO VIDEO stream!");
    m_srcReader = CreateMediaReader();
    if (!m_srcReader->Open(hParser))
        throw runtime_error(m_srcReader->GetError());
    if (!m_srcReader->ConfigVideoReader(outWidth, outHeight))
        throw runtime_error(m_srcReader->GetError());
    if (frameRate.num <= 0 || frameRate.den <= 0)
        throw invalid_argument("Invalid argument value for 'frameRate'!");
    m_frameRate = frameRate;
    m_srcDuration = m_srcReader->GetVideoStream()->duration;
    if (startOffset < 0)
        throw invalid_argument("Argument 'startOffset' can NOT be NEGATIVE!");
    if (endOffset > 0)
        throw invalid_argument("Argument 'endOffset' can NOT be POSITIVE!");
    if (startOffset-endOffset >= m_srcDuration)
        throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
    m_startOffset = startOffset;
    m_endOffset = endOffset;
    if (!m_srcReader->SeekTo(startOffset))
        throw runtime_error(m_srcReader->GetError());
    if (!m_srcReader->Start())
        throw runtime_error(m_srcReader->GetError());

    m_timeLineOffset = timeLineOffset;
    m_id = s_idCounter++;
}

VideoClip::~VideoClip()
{
    ReleaseMediaReader(&m_srcReader);
}

bool VideoClip::IsStartOffsetValid(double startOffset)
{
    if (startOffset < 0 || startOffset-m_endOffset >= m_srcDuration)
        return false;
    return true;
}

void VideoClip::ChangeStartOffset(double startOffset)
{
    if (startOffset == m_startOffset)
        return;
    if (startOffset < 0)
        throw invalid_argument("Argument 'startOffset' can NOT be NEGATIVE!");
    if (startOffset-m_endOffset >= m_srcDuration)
        throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
    m_startOffset = startOffset;
}

void VideoClip::ChangeEndOffset(double endOffset)
{
    if (endOffset == m_endOffset)
        return;
    if (endOffset > 0)
        throw invalid_argument("Argument 'endOffset' can NOT be POSITIVE!");
    if (m_startOffset-endOffset >= m_srcDuration)
        throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
    m_endOffset = endOffset;
}

void VideoClip::SeekTo(double pos)
{
    if (pos < 0 || pos >= ClipDuration())
        throw invalid_argument("Argument 'pos' can NOT be NEGATIVE or larger than clip duration!");
    if (!m_srcReader->SeekTo(pos+m_startOffset))
        throw runtime_error(m_srcReader->GetError());
    m_eof = false;
}

void VideoClip::ReadVideoFrame(double pos, ImGui::ImMat& vmat, bool& eof)
{
    if (m_eof)
    {
        eof = true;
        return;
    }
    if (!m_srcReader->ReadVideoFrame(pos+m_startOffset, vmat, eof))
        throw runtime_error(m_srcReader->GetError());
}
