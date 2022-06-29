#pragma once
#include <string>
#include <list>
#include "immat.h"
#include "AudioTrack.h"
#include "Logger.h"

struct MultiTrackAudioReader
{
    virtual bool Configure(uint32_t outChannels, uint32_t outSampleRate, uint32_t outSamplesPerFrame = 1024) = 0;
    virtual bool Start() = 0;
    virtual void Close() = 0;
    virtual DataLayer::AudioTrackHolder AddTrack(int64_t trackId) = 0;
    virtual DataLayer::AudioTrackHolder RemoveTrackByIndex(uint32_t index) = 0;
    virtual DataLayer::AudioTrackHolder RemoveTrackById(int64_t trackId) = 0;
    virtual bool SetDirection(bool forward) = 0;
    virtual bool SeekTo(int64_t pos) = 0;
    virtual bool ReadAudioSamples(ImGui::ImMat& amat) = 0;
    virtual bool Refresh() = 0;
    virtual int64_t SizeToDuration(uint32_t sizeInByte) = 0;

    virtual int64_t Duration() const = 0;
    virtual int64_t ReadPos() const = 0;

    virtual uint32_t TrackCount() const = 0;
    virtual std::list<DataLayer::AudioTrackHolder>::iterator TrackListBegin() = 0;
    virtual std::list<DataLayer::AudioTrackHolder>::iterator TrackListEnd() = 0;
    virtual DataLayer::AudioTrackHolder GetTrackByIndex(uint32_t idx) = 0;
    virtual DataLayer::AudioTrackHolder GetTrackById(int64_t trackId, bool createIfNotExists = false) = 0;
    virtual DataLayer::AudioClipHolder GetClipById(int64_t clipId) = 0;

    virtual std::string GetError() const = 0;
};

MultiTrackAudioReader* CreateMultiTrackAudioReader();
void ReleaseMultiTrackAudioReader(MultiTrackAudioReader** mreader);

Logger::ALogger* GetMultiTrackAudioReaderLogger();