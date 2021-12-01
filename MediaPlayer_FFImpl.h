#pragma once
#include <cstdint>
#include <string>
#include <immat.h>

struct MediaPlayer
{
    virtual bool Open(const std::string& url) = 0;
    virtual bool Close() = 0;
    virtual bool Play() = 0;
    virtual bool Pause() = 0;
    virtual bool Reset() = 0;
    virtual bool Seek(uint64_t pos) = 0;
    virtual bool SeekAsync(uint64_t pos) = 0;

    virtual bool IsOpened() const = 0;
    virtual bool IsPlaying() const = 0;

    virtual bool HasVideo() const = 0;
    virtual bool HasAudio() const = 0;
    virtual float GetPlaySpeed() const = 0;
    virtual bool SetPlaySpeed(float speed) = 0;
    virtual bool SetPreferHwDecoder(bool prefer) = 0;
    virtual uint64_t GetDuration() const = 0;
    virtual uint64_t GetPlayPos() const = 0; 
    virtual ImGui::ImMat GetVideo() const = 0;

    virtual std::string GetError() const = 0;
};

MediaPlayer* CreateMediaPlayer();
void DestroyMediaPlayer(MediaPlayer** player);