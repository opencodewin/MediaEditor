#pragma once
#include <atomic>
#include <memory>
#include <functional>
#include "immat.h"
#include "MediaReader.h"

namespace DataLayer
{
    struct VideoFilter;
    using VideoFilterHolder = std::shared_ptr<VideoFilter>;

    class VideoClip;
    using VideoClipHolder = std::shared_ptr<VideoClip>;

    // forward declaration
    struct VideoTransformFilter;

    class VideoClip
    {
    public:
        VideoClip(
            int64_t id, MediaParserHolder hParser,
            uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate,
            int64_t start, int64_t startOffset, int64_t endOffset, int64_t readpos);
        VideoClip(const VideoClip&) = delete;
        VideoClip(VideoClip&&) = delete;
        VideoClip& operator=(const VideoClip&) = delete;
        ~VideoClip();
        VideoClipHolder Clone( uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate) const;

        int64_t Id() const { return m_id; }
        int64_t TrackId() const { return m_trackId; }
        void SetTrackId(int64_t trackId) { m_trackId = trackId; }
        MediaParserHolder GetMediaParser() const { return m_srcReader->GetMediaParser(); }
        int64_t Duration() const { return m_srcDuration-m_startOffset-m_endOffset; }
        void SetStart(double start) { m_start = start; }
        int64_t Start() const { return m_start; }
        int64_t End() const { return m_start+Duration(); }
        int64_t StartOffset() const { return m_startOffset; }
        int64_t EndOffset() const { return m_endOffset; }
        uint32_t SrcWidth() const { return m_srcReader->GetVideoOutWidth(); }
        uint32_t SrcHeight() const { return m_srcReader->GetVideoOutHeight(); }
        uint32_t OutWidth() const;
        uint32_t OutHeight() const;
        
        bool IsStartOffsetValid(int64_t startOffset);
        void ChangeStartOffset(int64_t startOffset);
        bool IsEndOffsetValid(int64_t endOffset);
        void ChangeEndOffset(int64_t endOffset);
        VideoFilterHolder GetFilter() const { return m_filter; }
        void SetFilter(VideoFilterHolder filter);
        VideoTransformFilter* GetTransformFilterPtr();

        void SeekTo(int64_t pos);
        void ReadVideoFrame(int64_t pos, ImGui::ImMat& vmat, bool& eof);
        void NotifyReadPos(int64_t pos);
        void SetDirection(bool forward);

        friend std::ostream& operator<<(std::ostream& os, VideoClip& clip);

    private:
        int64_t m_id;
        int64_t m_trackId{-1};
        MediaInfo::InfoHolder m_hInfo;
        MediaReader* m_srcReader;
        int64_t m_srcDuration;
        int64_t m_start;
        int64_t m_startOffset;
        int64_t m_endOffset;
        bool m_eof{false};
        MediaInfo::Ratio m_frameRate;
        uint32_t m_frameIndex{0};
        VideoFilterHolder m_filter;
        VideoTransformFilter* m_transFilter{nullptr};
        int64_t m_wakeupRange{1000};
    };

    struct VideoFilter
    {
        virtual ~VideoFilter() {}
        virtual const std::string GetFilterName() const = 0;
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
        void ReadVideoFrame(int64_t pos, ImGui::ImMat& vmat, bool& eof);

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
