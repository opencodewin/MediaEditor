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

        VideoClipHolder AddNewClip(int64_t clipId, MediaParserHolder hParser, int64_t start, int64_t startOffset, int64_t endOffset, int64_t readPos);
        void InsertClip(VideoClipHolder hClip);
        void MoveClip(int64_t id, int64_t start);
        void ChangeClipRange(int64_t id, int64_t startOffset, int64_t endOffset);
        VideoClipHolder RemoveClipById(int64_t clipId);
        VideoClipHolder RemoveClipByIndex(uint32_t index);

        VideoClipHolder GetClipByIndex(uint32_t index);
        VideoClipHolder GetClipById(int64_t id);
        VideoOverlapHolder GetOverlapById(int64_t id);
        uint32_t ClipCount() const { return m_clips.size(); }
        std::list<VideoClipHolder>::iterator ClipListBegin() { return m_clips.begin(); }
        std::list<VideoClipHolder>::iterator ClipListEnd() { return m_clips.end(); }
        uint32_t OverlapCount() const { return m_overlaps.size(); }
        std::list<VideoOverlapHolder>::iterator OverlapListBegin() { return m_overlaps.begin(); }
        std::list<VideoOverlapHolder>::iterator OverlapListEnd() { return m_overlaps.end(); }

        void SeekTo(int64_t pos);
        void ReadVideoFrame(ImGui::ImMat& vmat);
        void SetDirection(bool forward);

        int64_t Id() const { return m_id; }
        uint32_t OutWidth() const { return m_outWidth; }
        uint32_t OutHeight() const { return m_outHeight; }
        MediaInfo::Ratio FrameRate() const { return m_frameRate; }
        int64_t Duration() const { return m_duration; }
        int64_t ReadPos() const { return (int64_t)((double)m_readFrames*1000*m_frameRate.den/m_frameRate.num); }

        friend std::ostream& operator<<(std::ostream& os, VideoTrack& track);

    private:
        static std::function<bool(const VideoClipHolder&, const VideoClipHolder&)> CLIP_SORT_CMP;
        static std::function<bool(const VideoOverlapHolder&, const VideoOverlapHolder&)> OVERLAP_SORT_CMP;
        bool CheckClipRangeValid(int64_t clipId, int64_t start, int64_t end);
        void UpdateClipOverlap(VideoClipHolder hClip, bool remove = false);

    private:
        std::recursive_mutex m_apiLock;
        int64_t m_id;
        uint32_t m_outWidth;
        uint32_t m_outHeight;
        MediaInfo::Ratio m_frameRate;
        std::list<VideoClipHolder> m_clips;
        std::list<VideoClipHolder>::iterator m_readClipIter;
        std::list<VideoOverlapHolder> m_overlaps;
        std::list<VideoOverlapHolder>::iterator m_readOverlapIter;
        int64_t m_readFrames{0};
        int64_t m_duration{0};
        bool m_readForward{true};
    };

    using VideoTrackHolder = std::shared_ptr<VideoTrack>;
}