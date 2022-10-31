#pragma once
#include <ostream>
#include <string>
#include "immat.h"
#include "VideoTrack.h"
#include "SubtitleTrack.h"
#include "Logger.h"

struct MultiTrackVideoReader
{
    virtual bool Configure(uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate) = 0;
    virtual MultiTrackVideoReader* CloneAndConfigure(uint32_t outWidth, uint32_t outHeight, const MediaInfo::Ratio& frameRate) = 0;
    virtual bool Start() = 0;
    virtual void Close() = 0;
    virtual DataLayer::VideoTrackHolder AddTrack(int64_t trackId, int64_t insertAfterId = INT64_MAX) = 0;
    virtual DataLayer::VideoTrackHolder RemoveTrackByIndex(uint32_t index) = 0;
    virtual DataLayer::VideoTrackHolder RemoveTrackById(int64_t trackId) = 0;
    virtual bool ChangeTrackViewOrder(int64_t targetId, int64_t insertAfterId) = 0;
    virtual bool SetDirection(bool forward) = 0;
    virtual bool SeekTo(int64_t pos, bool async = false) = 0;
    virtual bool ReadVideoFrameEx(int64_t pos, std::vector<DataLayer::CorrelativeFrame>& frames, bool nonblocking = false, bool precise = true) = 0;
    virtual bool ReadVideoFrame(int64_t pos, ImGui::ImMat& vmat, bool nonblocking = false) = 0;
    virtual bool ReadNextVideoFrameEx(std::vector<DataLayer::CorrelativeFrame>& frames) = 0;
    virtual bool ReadNextVideoFrame(ImGui::ImMat& vmat) = 0;
    virtual bool Refresh() = 0;

    virtual int64_t Duration() const = 0;
    virtual int64_t ReadPos() const = 0;

    virtual uint32_t TrackCount() const = 0;
    virtual std::list<DataLayer::VideoTrackHolder>::iterator TrackListBegin() = 0;
    virtual std::list<DataLayer::VideoTrackHolder>::iterator TrackListEnd() = 0;
    virtual DataLayer::VideoTrackHolder GetTrackByIndex(uint32_t idx) = 0;
    virtual DataLayer::VideoTrackHolder GetTrackById(int64_t trackId, bool createIfNotExists = false) = 0;
    virtual DataLayer::VideoClipHolder GetClipById(int64_t clipId) = 0;
    virtual DataLayer::VideoOverlapHolder GetOverlapById(int64_t ovlpId) = 0;

    virtual DataLayer::SubtitleTrackHolder BuildSubtitleTrackFromFile(int64_t id, const std::string& url, int64_t insertAfterId = INT64_MAX) = 0;
    virtual DataLayer::SubtitleTrackHolder NewEmptySubtitleTrack(int64_t id, int64_t insertAfterId = INT64_MAX) = 0;
    virtual DataLayer::SubtitleTrackHolder GetSubtitleTrackById(int64_t trackId) = 0;
    virtual DataLayer::SubtitleTrackHolder RemoveSubtitleTrackById(int64_t trackId) = 0;
    virtual bool ChangeSubtitleTrackViewOrder(int64_t targetId, int64_t insertAfterId) = 0;

    virtual std::string GetError() const = 0;

    friend std::ostream& operator<<(std::ostream& os, MultiTrackVideoReader& mtvReader)
    {
        os << ">>> MultiTrackVideoReader :" << std::endl;
        auto trackIter = mtvReader.TrackListBegin();
        while (trackIter != mtvReader.TrackListEnd())
        {
            DataLayer::VideoTrackHolder& track = *trackIter;
            os << "\t Track#" << track->Id() << " : " << *(track.get()) << std::endl;
            trackIter++;
        }
        os << "<<< [END]MultiTrackVideoReader";
        return os;
    }
};

MultiTrackVideoReader* CreateMultiTrackVideoReader();
void ReleaseMultiTrackVideoReader(MultiTrackVideoReader** mreader);

Logger::ALogger* GetMultiTrackVideoReaderLogger();