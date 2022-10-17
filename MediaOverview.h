#pragma once
#include "immat.h"
#include "MediaParser.h"
#include "Logger.h"

struct MediaOverview
{
    virtual bool Open(const std::string& url, uint32_t snapshotCount = 20) = 0;
    virtual bool Open(MediaParserHolder hParser, uint32_t snapshotCount = 20) = 0;
    virtual MediaParserHolder GetMediaParser() const = 0;
    virtual void Close() = 0;
    virtual bool GetSnapshots(std::vector<ImGui::ImMat>& snapshots) = 0;

    struct Waveform
    {
        double aggregateSamples;
        double aggregateDuration;
        float minSample{0}, maxSample{0};
        std::vector<std::vector<float>> pcm;
    };
    using WaveformHolder = std::shared_ptr<Waveform>;
    virtual WaveformHolder GetWaveform() const = 0;
    virtual bool SetSingleFramePixels(uint32_t pixels) = 0;
    virtual bool SetFixedAggregateSamples(double aggregateSamples) = 0;

    virtual bool IsOpened() const = 0;
    virtual bool IsDone() const = 0;
    virtual bool HasVideo() const = 0;
    virtual bool HasAudio() const = 0;
    virtual uint32_t GetSnapshotCount() const = 0;

    virtual bool SetSnapshotSize(uint32_t width, uint32_t height) = 0;
    virtual bool SetSnapshotResizeFactor(float widthFactor, float heightFactor) = 0;
    virtual bool SetOutColorFormat(ImColorFormat clrfmt) = 0;
    virtual bool SetResizeInterpolateMode(ImInterpolateMode interp) = 0;

    virtual MediaInfo::InfoHolder GetMediaInfo() const = 0;
    virtual const MediaInfo::VideoStream* GetVideoStream() const = 0;
    virtual const MediaInfo::AudioStream* GetAudioStream() const = 0;

    virtual uint32_t GetVideoWidth() const = 0;
    virtual uint32_t GetVideoHeight() const = 0;
    virtual int64_t GetVideoDuration() const = 0;
    virtual int64_t GetVideoFrameCount() const = 0;
    virtual uint32_t GetAudioChannel() const = 0;
    virtual uint32_t GetAudioSampleRate() const = 0;

    virtual bool IsHwAccelEnabled() const = 0;
    virtual void EnableHwAccel(bool enable) = 0;
    virtual std::string GetError() const = 0;
};

MediaOverview* CreateMediaOverview();
void ReleaseMediaOverview(MediaOverview** msrc);

Logger::ALogger* GetMediaOverviewLogger();