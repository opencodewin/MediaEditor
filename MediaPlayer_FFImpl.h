#pragma once
#include <cstdint>
#include <string>
#include <immat.h>
#include "AudioRender.hpp"

struct MediaPlayer
{
    virtual bool SetAudioRender(AudioRender* audrnd) = 0;
    virtual bool Open(const std::string& url) = 0;
    virtual bool Close() = 0;
    virtual bool Play() = 0;
    virtual bool Pause() = 0;
    virtual bool Reset() = 0;
    virtual bool Seek(int64_t pos, bool seekToI = false) = 0;
    virtual bool SeekAsync(int64_t pos) = 0;
    virtual bool QuitSeekAsync() = 0;

    virtual bool IsOpened() const = 0;
    virtual bool IsPlaying() const = 0;
    virtual bool IsSeeking() const = 0;

    virtual bool HasVideo() const = 0;
    virtual bool HasAudio() const = 0;
    virtual float GetPlaySpeed() const = 0;
    virtual bool SetPlaySpeed(float speed) = 0;
    virtual bool SetPreferHwDecoder(bool prefer) = 0;
    virtual uint64_t GetDuration() const = 0;
    virtual int64_t GetPlayPos() const = 0; 
    virtual ImGui::ImMat GetVideo() const = 0;

    enum class PlayMode
    {
        NORMAL = 0,
        VIDEO_ONLY,
        AUDIO_ONLY
    };
    virtual bool SetPlayMode(PlayMode mode) = 0;

    virtual std::string GetError() const = 0;
};

MediaPlayer* CreateMediaPlayer();
void ReleaseMediaPlayer(MediaPlayer** player);
