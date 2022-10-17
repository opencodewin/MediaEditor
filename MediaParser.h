#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "Logger.h"
#include "MediaInfo.h"

struct MediaParser
{
    virtual ~MediaParser() {};

    virtual bool Open(const std::string& url) = 0;
    virtual void Close() = 0;

    enum InfoType
    {
        MEDIA_INFO = 0,
        VIDEO_SEEK_POINTS,
    };
    virtual bool EnableParseInfo(InfoType infoType) = 0;
    virtual bool CheckInfoReady(InfoType infoType) = 0;

    virtual std::string GetUrl() const = 0;

    virtual MediaInfo::InfoHolder GetMediaInfo(bool wait = true) = 0;
    virtual bool HasVideo() = 0;
    virtual bool HasAudio() = 0;
    virtual int GetBestVideoStreamIndex() = 0;
    virtual int GetBestAudioStreamIndex() = 0;
    virtual MediaInfo::VideoStream* GetBestVideoStream() = 0;
    virtual MediaInfo::AudioStream* GetBestAudioStream() = 0;

    using SeekPointsHolder = std::shared_ptr<std::vector<int64_t>>;
    virtual SeekPointsHolder GetVideoSeekPoints(bool wait = true) = 0;

    virtual bool IsOpened() const = 0;

    virtual std::string GetError() const = 0;
};

using MediaParserHolder = std::shared_ptr<MediaParser>;

MediaParserHolder CreateMediaParser();

Logger::ALogger* GetMediaParserLogger();