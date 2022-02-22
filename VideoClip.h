#pragma once
#include <memory>
#include <atomic>
#include "immat.h"
#include "MediaReader.h"

class VideoClip
{
public:
    VideoClip(MediaParserHolder hParser, uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate, double timeLineOffset, double startOffset, double endOffset);
    VideoClip(const VideoClip&) = delete;
    VideoClip(VideoClip&&) = delete;
    VideoClip& operator=(const VideoClip&) = delete;
    ~VideoClip();

    uint32_t Id() const { return m_id; }
    MediaParserHolder GetMediaParser() const { return m_srcReader->GetMediaParser(); }
    double ClipDuration() const { return m_srcDuration+m_endOffset-m_startOffset; }
    double TimeLineOffset() const { return m_timeLineOffset; }
    void SetTimeLineOffset(double timeLineOffset) { m_timeLineOffset = timeLineOffset; }
    double StartOffset() const { return m_startOffset; }
    double EndOffset() const { return m_endOffset; }
    
    bool IsStartOffsetValid(double startOffset);
    void ChangeStartOffset(double startOffset);
    bool IsEndOffsetValid(double endOffset);
    void ChangeEndOffset(double endOffset);

    void SeekTo(double pos);
    void ReadVideoFrame(double pos, ImGui::ImMat& vmat, bool& eof);

private:
    static std::atomic_uint32_t s_idCounter;
    uint32_t m_id;
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