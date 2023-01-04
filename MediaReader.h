#pragma once
#include "immat.h"
#include "MediaParser.h"
#include "Logger.h"

struct MediaReader
{
    virtual bool Open(const std::string& url) = 0;
    virtual bool Open(MediaParserHolder hParser) = 0;
    virtual bool ConfigVideoReader(
            uint32_t outWidth, uint32_t outHeight,
            ImColorFormat outClrfmt = IM_CF_RGBA, ImInterpolateMode rszInterp = IM_INTERPOLATE_BICUBIC) = 0;
    virtual bool ConfigVideoReader(
            float outWidthFactor, float outHeightFactor,
            ImColorFormat outClrfmt = IM_CF_RGBA, ImInterpolateMode rszInterp = IM_INTERPOLATE_BICUBIC) = 0;
    virtual bool ConfigAudioReader(uint32_t outChannels, uint32_t outSampleRate, const std::string& outPcmFormat = "fltp", uint32_t audioStreamIndex = 0) = 0;
    virtual bool Start(bool suspend = false) = 0;
    virtual bool Stop() = 0;
    virtual void Close() = 0;
    virtual bool SeekTo(double pos) = 0;
    virtual void SetDirection(bool forward) = 0;
    virtual void Suspend() = 0;
    virtual void Wakeup() = 0;

    virtual bool ReadVideoFrame(double pos, ImGui::ImMat& m, bool& eof, bool wait = true) = 0;
    virtual bool ReadAudioSamples(uint8_t* buf, uint32_t& size, double& pos, bool& eof, bool wait = true) = 0;

    virtual uint32_t Id() const = 0;
    virtual bool IsOpened() const = 0;
    virtual bool IsStarted() const = 0;
    virtual MediaParserHolder GetMediaParser() const = 0;
    virtual bool IsVideoReader() const = 0;
    virtual bool IsDirectionForward() const = 0;
    virtual bool IsSuspended() const = 0;
    virtual bool IsPlanar() const = 0;

    virtual bool SetCacheDuration(double forwardDur, double backwardDur) = 0;
    virtual std::pair<double, double> GetCacheDuration() const = 0;

    virtual MediaInfo::InfoHolder GetMediaInfo() const = 0;
    virtual const MediaInfo::VideoStream* GetVideoStream() const = 0;
    virtual const MediaInfo::AudioStream* GetAudioStream() const = 0;
    virtual uint32_t GetVideoOutWidth() const = 0;
    virtual uint32_t GetVideoOutHeight() const = 0;
    virtual uint32_t GetAudioOutChannels() const = 0;
    virtual uint32_t GetAudioOutSampleRate() const = 0;
    virtual uint32_t GetAudioOutFrameSize() const = 0;

    virtual bool IsHwAccelEnabled() const = 0;
    virtual void EnableHwAccel(bool enable) = 0;
    virtual std::string GetError() const = 0;
};

MediaReader* CreateMediaReader(const std::string& loggerName = "");
void ReleaseMediaReader(MediaReader** mreader);

Logger::ALogger* GetMediaReaderLogger();