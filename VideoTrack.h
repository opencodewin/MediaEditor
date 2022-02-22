#pragma
#include <mutex>
#include <list>
#include "VideoClip.h"

struct VideoTrack
{
public:
    VideoTrack(uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate);
    VideoTrack(const VideoTrack&) = delete;
    VideoTrack(VideoTrack&&) = delete;
    VideoTrack& operator=(const VideoTrack&) = delete;

    uint32_t AddNewClip(const std::string& url, double timeLineOffset, double startOffset, double endOffset);
    uint32_t AddNewClip(MediaParserHolder hParser, double timeLineOffset, double startOffset, double endOffset);
    void InsertClip(VideoClipHolder hClip, double timeLineOffset);
    VideoClipHolder RemoveClipById(uint32_t clipId);
    VideoClipHolder RemoveClipByIndex(uint32_t index);

    uint32_t ClipCount() const { return m_clips.size(); }
    std::list<VideoClipHolder>::iterator ClipListBegin() { return m_clips.begin(); }
    std::list<VideoClipHolder>::iterator ClipListEnd() { return m_clips.end(); }
    VideoClipHolder GetClipByIndex(uint32_t index);
    void ChangeClip(uint32_t id, double timeLineOffset, double startOffset, double endOffset);

    void SeekTo(double pos);
    void ReadVideoFrame(ImGui::ImMat& vmat);

    double Duration() const { return m_duration; }

private:
    std::recursive_mutex m_apiLock;
    uint32_t m_outWidth;
    uint32_t m_outHeight;
    MediaInfo::Ratio m_frameRate;
    std::list<VideoClipHolder> m_clips;
    std::list<VideoClipHolder>::iterator m_iterRead;
    int64_t m_readFrames{0};
    double m_duration{0};
};

using VideoTrackHolder = std::shared_ptr<VideoTrack>;