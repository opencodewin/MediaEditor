#pragma once
#include <string>
#include <memory>
#include "immat.h"
#include "MediaParser.h"
#include "Logger.h"

struct MediaSnapshot
{
    struct Image
    {
        bool mTextureReady{false};
        ImTextureID mTid{0};
        ImVec2 mSize{0, 0};
        int64_t mTimestampMs{0};
        ImGui::ImMat mImgMat;
    };
    using ImageHolder = std::shared_ptr<Image>;

    virtual bool Open(const std::string& url) = 0;
    virtual bool Open(MediaParserHolder hParser) = 0;
    virtual MediaParserHolder GetMediaParser() const = 0;
    virtual void Close() = 0;
    virtual bool GetSnapshots(std::vector<ImageHolder>& snapshots, double startPos) = 0;
    virtual bool UpdateSnapshotTexture(std::vector<ImageHolder>& snapshots) = 0;

    virtual bool IsOpened() const = 0;
    virtual bool HasVideo() const = 0;
    virtual bool HasAudio() const = 0;
    virtual bool ConfigSnapWindow(double& windowSize, double frameCount) = 0;
    virtual bool SetCacheFactor(double cacheFactor) = 0;
    virtual double GetMinWindowSize() const = 0;
    virtual double GetMaxWindowSize() const = 0;

    virtual bool SetSnapshotSize(uint32_t width, uint32_t height) = 0;
    virtual bool SetSnapshotResizeFactor(float widthFactor, float heightFactor) = 0;
    virtual bool SetOutColorFormat(ImColorFormat clrfmt) = 0;
    virtual bool SetResizeInterpolateMode(ImInterpolateMode interp) = 0;

    virtual MediaInfo::InfoHolder GetMediaInfo() const = 0;
    virtual const MediaInfo::VideoStream* GetVideoStream() const = 0;
    virtual const MediaInfo::AudioStream* GetAudioStream() const = 0;

    virtual uint32_t GetVideoWidth() const = 0;
    virtual uint32_t GetVideoHeight() const = 0;
    virtual int64_t GetVideoMinPos() const = 0;
    virtual int64_t GetVideoDuration() const = 0;
    virtual int64_t GetVideoFrameCount() const = 0;

    virtual std::string GetError() const = 0;
};

MediaSnapshot* CreateMediaSnapshot();
void ReleaseMediaSnapshot(MediaSnapshot** msnapshot);

Logger::ALogger* GetMediaSnapshotLogger();
