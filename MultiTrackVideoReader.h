#pragma once
#include <string>
#include "immat.h"
#include "VideoTrack.h"
#include "Logger.h"

struct MultiTrackVideoReader
{
    virtual bool Configure(uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate) = 0;
    virtual bool Start() = 0;
    virtual void Close() = 0;
    virtual bool AddTrack(int64_t trackId) = 0;
    virtual bool RemoveTrack(uint32_t index) = 0;
    virtual bool SetDirection(bool forward) = 0;
    virtual bool SeekTo(double pos) = 0;
    virtual bool ReadVideoFrame(double pos, ImGui::ImMat& vmat) = 0;
    virtual bool ReadNextVideoFrame(ImGui::ImMat& vmat) = 0;

    virtual uint32_t TrackCount() const = 0;
    virtual std::list<DataLayer::VideoTrackHolder>::iterator TrackListBegin() = 0;
    virtual std::list<DataLayer::VideoTrackHolder>::iterator TrackListEnd() = 0;
    virtual DataLayer::VideoTrackHolder GetTrack(uint32_t idx) = 0;
    virtual double Duration() = 0;

    virtual std::string GetError() const = 0;
};

MultiTrackVideoReader* CreateMultiTrackVideoReader();
void ReleaseMultiTrackVideoReader(MultiTrackVideoReader** mreader);

Logger::ALogger* GetMultiTrackVideoReaderLogger();