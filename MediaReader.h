#pragma once
#include "MediaParser.h"
#include "immat.h"

struct MediaReader
{
    virtual bool Open(const std::string& url) = 0;
    virtual bool Open(MediaParserHolder hParser) = 0;
    virtual MediaParserHolder GetMediaParser() const = 0;
    virtual void Close() = 0;
    virtual void SeekTo(double ts) = 0;
    virtual void SetDirection(bool forward) = 0;
    virtual bool IsDirectionForward() const = 0;
    virtual bool ReadFrame(double ts, ImGui::ImMat& m, bool wait = true) = 0;

    virtual bool IsOpened() const = 0;
    virtual bool HasVideo() const = 0;
    virtual bool HasAudio() const = 0;

    virtual bool SetSnapshotSize(uint32_t width, uint32_t height) = 0;
    virtual bool SetSnapshotResizeFactor(float widthFactor, float heightFactor) = 0;
    virtual bool SetOutColorFormat(ImColorFormat clrfmt) = 0;
    virtual bool SetResizeInterpolateMode(ImInterpolateMode interp) = 0;

    virtual MediaInfo::InfoHolder GetMediaInfo() const = 0;
    virtual const MediaInfo::VideoStream* GetVideoStream() const = 0;
    virtual const MediaInfo::AudioStream* GetAudioStream() const = 0;
};

MediaReader* CreateMediaReader();
void ReleaseMediaReader(MediaReader** mreader);