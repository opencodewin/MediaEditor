#include <imconfig.h>
#if IMGUI_VULKAN_SHADER
#include <ColorConvert_vulkan.h>
#include <CopyTo_vulkan.h>
#endif
#include "VideoClip.h"

using namespace std;

namespace DataLayer
{
    ///////////////////////////////////////////////////////////////////////////////////////////
    // VideoClip
    ///////////////////////////////////////////////////////////////////////////////////////////
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

    void VideoClip::SetDirection(bool forward)
    {
        m_srcReader->SetDirection(forward);
    }

    std::ostream& operator<<(std::ostream& os, VideoClip& clip)
    {
        os << "{'id':" << clip.Id() << ", 'start':" << clip.Start()
            << ", 'soff':" << clip.StartOffset() << ", 'eoff':" << clip.EndOffset() << "}";
        return os;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // DefaultVideoTransition_Impl
    ///////////////////////////////////////////////////////////////////////////////////////////
    class DefaultVideoTransition_Impl : public VideoTransition
    {
    public:
        void ApplyTo(VideoOverlap* overlap) override
        {
            m_overlapPtr = overlap;
        }

        ImGui::ImMat MixTwoImages(const ImGui::ImMat& vmat1, const ImGui::ImMat& vmat2, double pos) override
        {
#if IMGUI_VULKAN_SHADER
            ImGui::ImMat dst;
            dst.type = IM_DT_INT8;
            if (vmat2.device == IM_DD_VULKAN)
            {
                const ImGui::VkMat vmat2vk = vmat2;
                m_clrcvt.Conv(vmat2vk, dst);
            }
            else
            {
                dst = vmat2.clone();
            }
            double alpha = (1-pos/m_overlapPtr->Duration())/2;
            if (vmat1.device == IM_DD_VULKAN)
            {
                const ImGui::VkMat vmat1vk = vmat1;
                m_alphaBlender.copyTo(vmat1vk, dst, 0, 0, alpha);
            }
            else
            {
                m_alphaBlender.copyTo(vmat1, dst, 0, 0, alpha);
            }
            return dst;
#else
            ImGui::ImMat nullMat;
            nullMat.time_stamp = pos;
            return nullMat;
#endif
        }

    private:
        VideoOverlap* m_overlapPtr{nullptr};
#if IMGUI_VULKAN_SHADER
        ImGui::ColorConvert_vulkan m_clrcvt;
        ImGui::CopyTo_vulkan m_alphaBlender;
#endif
    };

    ///////////////////////////////////////////////////////////////////////////////////////////
    // VideoOverlap
    ///////////////////////////////////////////////////////////////////////////////////////////
    VideoOverlap::VideoOverlap(int64_t id, VideoClipHolder hClip1, VideoClipHolder hClip2)
        : m_id(id), m_frontClip(hClip1), m_rearClip(hClip2), m_transition(new DefaultVideoTransition_Impl())
    {
        Update();
        m_transition->ApplyTo(this);
    }

    void VideoOverlap::Update()
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

    void VideoOverlap::SeekTo(double pos)
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

    void VideoOverlap::ReadVideoFrame(double pos, ImGui::ImMat& vmat, bool& eof)
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

        vmat = m_transition->MixTwoImages(vmat1, vmat2, pos);

        eof = eof1 || eof2;
        if (pos == Duration())
            eof = true;
    }

    std::ostream& operator<<(std::ostream& os, VideoOverlap& overlap)
    {
        os << "{'id':" << overlap.Id() << ", 'start':" << overlap.Start()
            << ", 'fcId':" << overlap.FrontClip()->Id() << ", 'rcId':" << overlap.RearClip()->Id() << "}";
        return os;
    }
}