#pragma
#include <functional>
#include <mutex>
#include <list>
#include "VideoClip.h"

namespace DataLayer
{
    struct VideoTrack
    {
    public:
        VideoTrack(int64_t id, uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate);
        VideoTrack(const VideoTrack&) = delete;
        VideoTrack(VideoTrack&&) = delete;
        VideoTrack& operator=(const VideoTrack&) = delete;

        // uint32_t AddNewClip(const std::string& url, double start, double startOffset, double endOffset);
        // uint32_t AddNewClip(MediaParserHolder hParser, double start, double startOffset, double endOffset);
        void InsertClip(VideoClipHolder hClip);
        void MoveClip(int64_t id, double start);
        void ChangeClipRange(int64_t id, double startOffset, double endOffset);
        VideoClipHolder RemoveClipById(int64_t clipId);
        VideoClipHolder RemoveClipByIndex(uint32_t index);

        VideoClipHolder GetClipByIndex(uint32_t index);
        VideoClipHolder GetClipById(int64_t id);
        uint32_t ClipCount() const { return m_clips.size(); }
        std::list<VideoClipHolder>::iterator ClipListBegin() { return m_clips.begin(); }
        std::list<VideoClipHolder>::iterator ClipListEnd() { return m_clips.end(); }
        uint32_t OverlapCount() const { return m_overlaps.size(); }
        std::list<VideoClipOverlapHolder>::iterator OverlapListBegin() { return m_overlaps.begin(); }
        std::list<VideoClipOverlapHolder>::iterator OverlapListEnd() { return m_overlaps.end(); }

        void SeekTo(double pos);
        void ReadVideoFrame(ImGui::ImMat& vmat);

        int64_t Id() const { return m_id; }
        uint32_t OutWidth() const { return m_outWidth; }
        uint32_t OutHeight() const { return m_outHeight; }
        MediaInfo::Ratio FrameRate() const { return m_frameRate; }
        double Duration() const { return m_duration; }

    private:
        static std::function<bool(const VideoClipHolder&, const VideoClipHolder&)> CLIP_SORT_CMP;
        static std::function<bool(const VideoClipOverlapHolder&, const VideoClipOverlapHolder&)> OVERLAP_SORT_CMP;
        bool CheckClipRangeValid(int64_t clipId, double start, double end);
        void UpdateClipOverlap(VideoClipHolder hClip);

    private:
        std::recursive_mutex m_apiLock;
        int64_t m_id;
        uint32_t m_outWidth;
        uint32_t m_outHeight;
        MediaInfo::Ratio m_frameRate;
        std::list<VideoClipHolder> m_clips;
        std::list<VideoClipHolder>::iterator m_readClipIter;
        std::list<VideoClipOverlapHolder> m_overlaps;
        std::list<VideoClipOverlapHolder>::iterator m_readOverlapIter;
        int64_t m_readFrames{0};
        double m_duration{0};
    };

    using VideoTrackHolder = std::shared_ptr<VideoTrack>;
}