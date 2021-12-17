#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "immat.h"

struct MediaSnapshot
{
    virtual bool Open(const std::string& url) = 0;
    virtual void Close() = 0;
    virtual bool GetSnapshots(std::vector<ImGui::ImMat>& snapshots, double startPos) = 0;

    virtual bool IsOpened() const = 0;
    virtual bool HasVideo() const = 0;
    virtual bool HasAudio() const = 0;
    virtual int64_t GetVidoeMinPos() const = 0;
    virtual int64_t GetVidoeDuration() const = 0;
    virtual int64_t GetVidoeFrameCount() const = 0;
    virtual bool ConfigSnapWindow(double& windowSize, double frameCount) = 0;
    virtual double GetMinWindowSize() const = 0;
    virtual double GetMaxWindowSize() const = 0;
    virtual uint32_t GetVideoWidth() const = 0;
    virtual uint32_t GetVideoHeight() const = 0;
    virtual bool SetSnapshotSize(uint32_t width, uint32_t height) = 0;
    virtual bool SetOutColorFormat(ImColorFormat clrfmt) = 0;
    virtual bool SetResizeInterpolateMode(ImInterpolateMode interp) = 0;

    virtual std::string GetError() const = 0;
};

MediaSnapshot* CreateMediaSnapshot();
void ReleaseMediaSnapshot(MediaSnapshot** msrc);