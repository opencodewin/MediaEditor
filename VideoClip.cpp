#include <imconfig.h>
#if IMGUI_VULKAN_SHADER
#include <ColorConvert_vulkan.h>
#include <AlphaBlending_vulkan.h>
#endif
#include "VideoClip.h"
#include "FFVideoFilter.h"
#include "Logger.h"

using namespace std;
using namespace Logger;

namespace DataLayer
{
    ///////////////////////////////////////////////////////////////////////////////////////////
    // VideoClip
    ///////////////////////////////////////////////////////////////////////////////////////////
    VideoClip::VideoClip(
        int64_t id, MediaParserHolder hParser,
        uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate,
        int64_t start, int64_t startOffset, int64_t endOffset, int64_t readpos)
        : m_id(id), m_start(start)
    {
        m_hInfo = hParser->GetMediaInfo();
        if (hParser->GetBestVideoStreamIndex() < 0)
            throw invalid_argument("Argument 'hParser' has NO VIDEO stream!");
        m_srcReader = CreateMediaReader();
        if (!m_srcReader->Open(hParser))
            throw runtime_error(m_srcReader->GetError());
        if (!m_srcReader->ConfigVideoReader(0u, 0u))
            throw runtime_error(m_srcReader->GetError());
        if (frameRate.num <= 0 || frameRate.den <= 0)
            throw invalid_argument("Invalid argument value for 'frameRate'!");
        m_frameRate = frameRate;
        m_srcDuration = static_cast<int64_t>(m_srcReader->GetVideoStream()->duration*1000);
        if (startOffset < 0)
            throw invalid_argument("Argument 'startOffset' can NOT be NEGATIVE!");
        if (endOffset < 0)
            throw invalid_argument("Argument 'endOffset' can NOT be NEGATIVE!");
        if (startOffset+endOffset >= m_srcDuration*1000)
            throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
        m_startOffset = startOffset;
        m_endOffset = endOffset;
        if (!m_srcReader->SeekTo((double)startOffset/1000))
            throw runtime_error(m_srcReader->GetError());
        bool suspend = readpos < -m_wakeupRange || readpos > Duration()+m_wakeupRange;
        if (!m_srcReader->Start(suspend))
            throw runtime_error(m_srcReader->GetError());
        m_fftransFilter = NewFFTransformVideoFilter();
        if (!m_fftransFilter->Initialize(outWidth, outHeight, "RGBA"))
            throw runtime_error(m_fftransFilter->GetError());
    }

    VideoClip::~VideoClip()
    {
        ReleaseMediaReader(&m_srcReader);
        delete m_fftransFilter;
        m_fftransFilter = nullptr;
    }

    VideoClipHolder VideoClip::Clone( uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate) const
    {
        VideoClipHolder newInstance = VideoClipHolder(new VideoClip(
            m_id, m_srcReader->GetMediaParser(), outWidth, outHeight, frameRate, m_start, m_startOffset, m_endOffset, 0));
        return newInstance;
    }

    uint32_t VideoClip::OutWidth() const
    {
        return m_fftransFilter->GetOutWidth();
    }

    uint32_t VideoClip::OutHeight() const
    {
        return m_fftransFilter->GetOutHeight();
    }

    bool VideoClip::IsStartOffsetValid(int64_t startOffset)
    {
        if (startOffset < 0 || startOffset+m_endOffset >= m_srcDuration)
            return false;
        return true;
    }

    void VideoClip::ChangeStartOffset(int64_t startOffset)
    {
        if (startOffset == m_startOffset)
            return;
        if (startOffset < 0)
            throw invalid_argument("Argument 'startOffset' can NOT be NEGATIVE!");
        if (startOffset+m_endOffset >= m_srcDuration)
            throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
        m_startOffset = startOffset;
    }

    void VideoClip::ChangeEndOffset(int64_t endOffset)
    {
        if (endOffset == m_endOffset)
            return;
        if (endOffset < 0)
            throw invalid_argument("Argument 'endOffset' can NOT be NEGATIVE!");
        if (m_startOffset+endOffset >= m_srcDuration)
            throw invalid_argument("Argument 'startOffset/endOffset', clip duration is NOT LARGER than 0!");
        m_endOffset = endOffset;
    }

    void VideoClip::SetFilter(VideoFilterHolder filter)
    {
        if (filter)
        {
            filter->ApplyTo(this);
            m_filter = filter;
        }
        else
        {
            m_filter = nullptr;
        }
    }

    FFTransformVideoFilter* VideoClip::GetTransformFilterPtr()
    {
        return m_fftransFilter;
    }

    void VideoClip::SeekTo(int64_t pos)
    {
        if (pos > Duration())
            return;
        if (pos < 0)
            pos = 0;
        if (!m_srcReader->SeekTo((double)(pos+m_startOffset)/1000))
            throw runtime_error(m_srcReader->GetError());
        m_eof = false;
    }

    void VideoClip::ReadVideoFrame(int64_t pos, ImGui::ImMat& vmat, bool& eof)
    {
        if (m_eof)
        {
            eof = true;
            return;
        }
        if (m_srcReader->IsSuspended())
        {
            m_srcReader->Wakeup();
            // Log(DEBUG) << ">>>> Clip#" << m_id <<" is WAKEUP." << endl;
        }
        ImGui::ImMat image;
        if (!m_srcReader->ReadVideoFrame((double)(pos+m_startOffset)/1000, image, eof))
            throw runtime_error(m_srcReader->GetError());
        VideoFilterHolder filter = m_filter;
        if (filter)
            image = filter->FilterImage(image, pos);
        // vmat = m_fftransFilter->FilterImage(image, pos);
        vmat = image;
    }

    void VideoClip::NotifyReadPos(int64_t pos)
    {
        if (pos < -m_wakeupRange || pos > Duration()+m_wakeupRange)
        {
            if (!m_srcReader->IsSuspended())
            {
                m_srcReader->Suspend();
                // Log(DEBUG) << ">>>> Clip#" << m_id <<" is SUSPENDED." << endl;
            }
        }
        else if (m_srcReader->IsSuspended())
        {
            m_srcReader->Wakeup();
            // Log(DEBUG) << ">>>> Clip#" << m_id <<" is WAKEUP." << endl;
        }
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
        VideoTransitionHolder Clone() override
        {
            return VideoTransitionHolder(new DefaultVideoTransition_Impl);
        }

        void ApplyTo(VideoOverlap* overlap) override
        {
            m_overlapPtr = overlap;
        }

        ImGui::ImMat MixTwoImages(const ImGui::ImMat& vmat1, const ImGui::ImMat& vmat2, int64_t pos, int64_t dur) override
        {
#if IMGUI_VULKAN_SHADER
            ImGui::ImMat dst;
            dst.type = IM_DT_INT8;
            double alpha = 1-(double)pos/m_overlapPtr->Duration();
            m_alphaBlender.blend(vmat1, vmat2, dst, (float)alpha);
            return dst;
#else
            return pos < m_overlapPtr->Duration()/2 ? vmat1 : vmat2;
#endif
        }

    private:
        VideoOverlap* m_overlapPtr{nullptr};
#if IMGUI_VULKAN_SHADER
        ImGui::ColorConvert_vulkan m_clrcvt;
        ImGui::AlphaBlending_vulkan m_alphaBlender;
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

    void VideoOverlap::SetTransition(VideoTransitionHolder transition)
    {
        if (transition)
        {
            transition->ApplyTo(this);
            m_transition = transition;
        }
        else
        {
            VideoTransitionHolder defaultTrans(new DefaultVideoTransition_Impl());
            defaultTrans->ApplyTo(this);
            m_transition = defaultTrans;
        }
    }

    void VideoOverlap::SeekTo(int64_t pos)
    {
        if (pos > Duration())
            return;
        if (pos < 0)
            pos = 0;
        int64_t pos1 = pos+(Start()-m_frontClip->Start());
        m_frontClip->SeekTo(pos1);
        int64_t pos2 = pos+(Start()-m_rearClip->Start());
        m_rearClip->SeekTo(pos2);
    }

    void VideoOverlap::ReadVideoFrame(int64_t pos, ImGui::ImMat& vmat, bool& eof)
    {
        if (pos < 0 || pos > Duration())
            throw invalid_argument("Argument 'pos' can NOT be NEGATIVE or larger than overlap duration!");

        bool eof1{false};
        ImGui::ImMat vmat1;
        int64_t pos1 = pos+(Start()-m_frontClip->Start());
        m_frontClip->ReadVideoFrame(pos1, vmat1, eof1);

        bool eof2{false};
        ImGui::ImMat vmat2;
        int64_t pos2 = pos+(Start()-m_rearClip->Start());
        m_rearClip->ReadVideoFrame(pos2, vmat2, eof2);

        VideoTransitionHolder transition = m_transition;
        vmat = transition->MixTwoImages(vmat1, vmat2, pos, Duration());

        eof = eof1 || eof2;
        if (pos == Duration())
            eof = true;
    }

    std::ostream& operator<<(std::ostream& os, VideoOverlap& overlap)
    {
        os << "{'id':" << overlap.Id() << ", 'start':" << overlap.Start() << ", 'dur':" << overlap.Duration() << "}";
        return os;
    }
}