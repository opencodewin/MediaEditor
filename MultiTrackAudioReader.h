#pragma once
#include <list>
#include "immat.h"
#include "AudioTrack.h"
#include "Logger.h"

struct MultiTrackAudioReader
{
    virtual bool Configure(uint32_t outChannels, uint32_t outSampleRate, uint32_t outSamplesPerFrame = 1024) = 0;
    virtual bool Start() = 0;
    virtual void Close() = 0;
    virtual bool AddTrack() = 0;
    virtual bool SetDirection(bool forward) = 0;
    virtual bool SeekTo(double pos) = 0;
    virtual bool ReadAudioSamples(ImGui::ImMat& amat) = 0;

    virtual uint32_t TrackCount() const = 0;
    virtual std::list<AudioTrackHolder>::iterator TrackListBegin() = 0;
    virtual std::list<AudioTrackHolder>::iterator TrackListEnd() = 0;
    virtual AudioTrackHolder GetTrack(uint32_t idx) = 0;
    virtual double Duration() = 0;

    virtual std::string GetError() const = 0;
};

MultiTrackAudioReader* CreateMultiTrackAudioReader();
void ReleaseMultiTrackAudioReader(MultiTrackAudioReader** mreader);

Logger::ALogger* GetMultiTrackAudioReaderLogger();