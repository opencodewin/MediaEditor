#pragma once
#include <atomic>
#include <memory>
#include <functional>
#include "immat.h"
#include "MediaReader.h"

namespace DataLayer
{
    class VideoClip
    {
    public:
        VideoClip(
            int64_t id, MediaParserHolder hParser,
            uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate,
            double timeLineOffset, double startOffset, double endOffset);
        VideoClip(const VideoClip&) = delete;
        VideoClip(VideoClip&&) = delete;
        VideoClip& operator=(const VideoClip&) = delete;
        ~VideoClip();

        int64_t Id() const { return m_id; }
        MediaParserHolder GetMediaParser() const { return m_srcReader->GetMediaParser(); }
        double Duration() const { return m_srcDuration-m_startOffset-m_endOffset; }
        void SetTimeLineOffset(double timeLineOffset) { m_timeLineOffset = timeLineOffset; }
        double Start() const { return m_timeLineOffset; }
        double End() const { return m_timeLineOffset+Duration(); }
        double StartOffset() const { return m_startOffset; }
        double EndOffset() const { return m_endOffset; }
        
        bool IsStartOffsetValid(double startOffset);
        void ChangeStartOffset(double startOffset);
        bool IsEndOffsetValid(double endOffset);
        void ChangeEndOffset(double endOffset);

        void SeekTo(double pos);
        void ReadVideoFrame(double pos, ImGui::ImMat& vmat, bool& eof);
        void SetDirection(bool forward);

    private:
        static std::atomic_uint32_t s_idCounter;
        int64_t m_id;
        MediaInfo::InfoHolder m_hInfo;
        MediaReader* m_srcReader;
        double m_timeLineOffset;
        double m_srcDuration;
        double m_startOffset;
        double m_endOffset;
        bool m_eof{false};
        MediaInfo::Ratio m_frameRate;
        uint32_t m_frameIndex{0};
    };

    using VideoClipHolder = std::shared_ptr<VideoClip>;

    struct VideoTransition;
    using VideoTransitionHolder = std::shared_ptr<VideoTransition>;

    class VideoOverlap
    {
    public:
        static bool HasOverlap(VideoClipHolder hClip1, VideoClipHolder hClip2)
        {
            return hClip1->Start() >= hClip2->Start() && hClip1->Start() < hClip2->End() ||
                   hClip1->End() > hClip2->Start() && hClip1->End() <= hClip2->End() ||
                   hClip1->Start() < hClip2->Start() && hClip1->End() > hClip2->End();
        }

        VideoOverlap(int64_t id, VideoClipHolder hClip1, VideoClipHolder hClip2);

        void Update();
        void SetTransition(VideoTransitionHolder trans);

        int64_t Id() const { return m_id; }
        double Start() const { return m_start; }
        double End() const { return m_end; }
        double Duration() const { return m_end-m_start; }
        VideoClipHolder FrontClip() const { return m_frontClip; }
        VideoClipHolder RearClip() const { return m_rearClip; }

        void SeekTo(double pos);
        void ReadVideoFrame(double pos, ImGui::ImMat& vmat, bool& eof);

    private:
        int64_t m_id;
        VideoClipHolder m_frontClip;
        VideoClipHolder m_rearClip;
        double m_start{0};
        double m_end{0};
        VideoTransitionHolder m_transition;
    };

    using VideoOverlapHolder = std::shared_ptr<VideoOverlap>;

    struct VideoTransition
    {
        virtual ~VideoTransition() {}
        virtual void ApplyTo(VideoOverlap* overlap) = 0;
        virtual ImGui::ImMat MixTwoImages(const ImGui::ImMat& vmat1, const ImGui::ImMat& vmat2, double pos) = 0;
    };
}