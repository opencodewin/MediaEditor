#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "immat.h"

struct MediaSource
{
    virtual bool ConfigureSnapWindow(double windowSize, double frameCount) = 0;
    virtual bool ConfigureSnapshot(uint32_t width, uint32_t height) = 0;
    virtual bool Open(const std::string& url) = 0;
    virtual void Close() = 0;
    virtual bool GetSnapshots(std::vector<ImGui::ImMat>& snapshots, double t0, double t1, uint32_t count = 0) = 0;
    virtual std::string GetError() const = 0;

    virtual bool IsOpened() const = 0;
    virtual bool HasVideo() const = 0;
    virtual bool HasAudio() const = 0;
};