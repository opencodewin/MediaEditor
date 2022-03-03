#include "VideoClip.h"

using namespace std;

namespace DataLayer
{
    ///////////////////////////////////////////////////////////////////////////////////////////
    // VideoClip
    ///////////////////////////////////////////////////////////////////////////////////////////
    atomic_uint32_t VideoClip::s_idCounter{1};

    VideoClip::VideoClip(
        int64_t id, MediaParserHolder hParser,
        uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate,
        double timeLineOffset, double startOffset, double endOffset)
        : m_id(id), m_timeLineOffset(timeLineOffset)
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
        if (endOffset < 0)
            throw invalid_argument("Argument 'endOffset' can NOT be NEGATIVE!");
        if (startOffset+endOffset >= m_srcDuration)
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
        if (startOffset < 0 || startOffset+m_endOffset >= m_srcDuration)
            return false;
        return true;
    }

    void VideoClip::ChangeStartOffset(double startOffset)
    {
        if (startOffset == m_startOffset)
            return;
        if (startOffset < 0)
            throw invalid_argument("Argument 'startOffset' can NOT be NEGATIVE!");
        if (startOffset+m_endOffset >= m_srcDuration)
            throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
        m_startOffset = startOffset;
    }

    void VideoClip::ChangeEndOffset(double endOffset)
    {
        if (endOffset == m_endOffset)
            return;
        if (endOffset < 0)
            throw invalid_argument("Argument 'endOffset' can NOT be NEGATIVE!");
        if (m_startOffset+endOffset >= m_srcDuration)
            throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
        m_endOffset = endOffset;
    }

    void VideoClip::SeekTo(double pos)
    {
        if (pos > Duration())
            return;
        if (pos < 0)
            pos = 0;
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


    ///////////////////////////////////////////////////////////////////////////////////////////
    // VideoClipOverlap
    ///////////////////////////////////////////////////////////////////////////////////////////
    VideoClipOverlap::VideoClipOverlap(int64_t id, VideoClipHolder hClip1, VideoClipHolder hClip2)
        : m_id(id), m_frontClip(hClip1), m_rearClip(hClip2), m_transFunc(DefaultTransition)
    {
        Update();
    }

    void VideoClipOverlap::Update()
    {
        VideoClipHolder hClip1 = m_frontClip;
        VideoClipHolder hClip2 = m_rearClip;
        if (hClip1->Start() <= hClip2->Start())
        {
            m_frontClip = hClip1;
            m_rearClip = hClip2;
        }
        else
        {
            m_frontClip = hClip2;
            m_rearClip = hClip1;
        }
        if (m_frontClip->End() <= m_rearClip->Start())
        {
            m_start = m_end = 0;
        }
        else
        {
            m_start = m_rearClip->Start();
            m_end = m_frontClip->End() <= m_rearClip->End() ? m_frontClip->End() : m_rearClip->End();
        }
    }

    void VideoClipOverlap::SeekTo(double pos)
    {
        if (pos > Duration())
            return;
        if (pos < 0)
            pos = 0;
        double pos1 = pos+(Start()-m_frontClip->Start());
        m_frontClip->SeekTo(pos1);
        double pos2 = pos+(Start()-m_rearClip->Start());
        m_rearClip->SeekTo(pos2);
    }

    void VideoClipOverlap::ReadVideoFrame(double pos, ImGui::ImMat& vmat, bool& eof)
    {
        if (pos < 0 || pos > Duration())
            throw invalid_argument("Argument 'pos' can NOT be NEGATIVE or larger than overlap duration!");

        bool eof1{false};
        ImGui::ImMat vmat1;
        double pos1 = pos+(Start()-m_frontClip->Start());
        m_frontClip->ReadVideoFrame(pos1, vmat1, eof1);

        bool eof2{false};
        ImGui::ImMat vmat2;
        double pos2 = pos+(Start()-m_rearClip->Start());
        m_rearClip->ReadVideoFrame(pos2, vmat2, eof2);

        m_transFunc(vmat1, vmat2, pos);

        eof = eof1 || eof2;
        if (pos == Duration())
            eof = true;
    }
}