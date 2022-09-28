#pragma once
#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <list>
#include "immat.h"
#include "MediaReader.h"

namespace DataLayer
{
    struct CorrelativeFrame
    {
        enum Phase
        {
            PHASE_SOURCE_FRAME = 0,
            PHASE_AFTER_FILTER,
            PHASE_AFTER_TRANSFORM,
            PHASE_AFTER_TRANSITION,
            PHASE_AFTER_MIXING,
        } phase;
        int64_t clipId;
        int64_t trackId;
        ImGui::ImMat frame;
    };

    struct VideoFilter;
    using VideoFilterHolder = std::shared_ptr<VideoFilter>;

    class VideoClip;
    using VideoClipHolder = std::shared_ptr<VideoClip>;

    // forward declaration
    struct VideoTransformFilter;

    struct VideoClip
    {
        ~VideoClip() {}
        static VideoClipHolder CreateVideoInstance(
            int64_t id, MediaParserHolder hParser,
            uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate,
            int64_t start, int64_t startOffset, int64_t endOffset, int64_t readpos);
        static VideoClipHolder CreateImageInstance(
            int64_t id, MediaParserHolder hParser,
            uint32_t outWidth, uint32_t outHeight, int64_t start, int64_t duration);

        virtual VideoClipHolder Clone(uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate) const = 0;
        virtual MediaParserHolder GetMediaParser() const = 0;
        virtual int64_t Id() const = 0;
        virtual int64_t TrackId() const = 0;
        virtual bool IsImage() const = 0;
        virtual int64_t Start() const = 0;
        virtual int64_t End() const = 0;
        virtual int64_t StartOffset() const = 0;
        virtual int64_t EndOffset() const = 0;
        virtual int64_t Duration() const = 0;
        virtual uint32_t SrcWidth() const = 0;
        virtual uint32_t SrcHeight() const = 0;
        virtual uint32_t OutWidth() const = 0;
        virtual uint32_t OutHeight() const = 0;

        virtual void SetTrackId(int64_t trackId) = 0;
        virtual void SetStart(int64_t start) = 0;
        virtual void ChangeStartOffset(int64_t startOffset) = 0;
        virtual void ChangeEndOffset(int64_t endOffset) = 0;
        virtual void SetDuration(int64_t duration) = 0;
        virtual void ReadVideoFrame(int64_t pos, std::vector<CorrelativeFrame>& frames, ImGui::ImMat& out, bool& eof) = 0;
        virtual void SeekTo(int64_t pos) = 0;
        virtual void NotifyReadPos(int64_t pos) = 0;
        virtual void SetDirection(bool forward) = 0;
        virtual void SetFilter(VideoFilterHolder filter) = 0;
        virtual VideoFilterHolder GetFilter() const = 0;
        virtual VideoTransformFilter* GetTransformFilterPtr() = 0;

        friend std::ostream& operator<<(std::ostream& os, VideoClip& clip);
    };

    struct VideoFilter
    {
        virtual ~VideoFilter() {}
        virtual const std::string GetFilterName() const = 0;
        virtual VideoFilterHolder Clone() = 0;
        virtual void ApplyTo(VideoClip* clip) = 0;
        virtual ImGui::ImMat FilterImage(const ImGui::ImMat& vmat, int64_t pos) = 0;
    };

    struct VideoTransition;
    using VideoTransitionHolder = std::shared_ptr<VideoTransition>;

    class VideoOverlap
    {
    public:
        static bool HasOverlap(VideoClipHolder hClip1, VideoClipHolder hClip2)
        {
            return (hClip1->Start() >= hClip2->Start() && hClip1->Start() < hClip2->End()) ||
                   (hClip1->End() > hClip2->Start() && hClip1->End() <= hClip2->End()) ||
                   (hClip1->Start() < hClip2->Start() && hClip1->End() > hClip2->End());
        }

        VideoOverlap(int64_t id, VideoClipHolder hClip1, VideoClipHolder hClip2);

        void Update();

        VideoTransitionHolder GetTransition() const { return m_transition; }
        void SetTransition(VideoTransitionHolder trans);

        int64_t Id() const { return m_id; }
        void SetId(int64_t id) { m_id = id; }
        int64_t Start() const { return m_start; }
        int64_t End() const { return m_end; }
        int64_t Duration() const { return m_end-m_start; }
        VideoClipHolder FrontClip() const { return m_frontClip; }
        VideoClipHolder RearClip() const { return m_rearClip; }

        void SeekTo(int64_t pos);
        void ReadVideoFrame(int64_t pos, std::vector<CorrelativeFrame>& frames, ImGui::ImMat& out, bool& eof);

        friend std::ostream& operator<<(std::ostream& os, VideoOverlap& overlap);

    private:
        int64_t m_id;
        VideoClipHolder m_frontClip;
        VideoClipHolder m_rearClip;
        int64_t m_start{0};
        int64_t m_end{0};
        VideoTransitionHolder m_transition;
    };

    using VideoOverlapHolder = std::shared_ptr<VideoOverlap>;

    struct VideoTransition
    {
        virtual ~VideoTransition() {}
        virtual VideoTransitionHolder Clone() = 0;
        virtual void ApplyTo(VideoOverlap* overlap) = 0;
        virtual ImGui::ImMat MixTwoImages(const ImGui::ImMat& vmat1, const ImGui::ImMat& vmat2, int64_t pos, int64_t dur) = 0;
    };
}
