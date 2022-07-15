#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include "Logger.h"
#include "SubtitleClip.h"

namespace DataLayer
{
    struct SubtitleTrack;
    using SubtitleTrackHolder = std::shared_ptr<SubtitleTrack>;

    struct SubtitleTrack
    {
        virtual bool SetFrameSize(uint32_t width, uint32_t height) = 0;
        virtual bool SetFont(const std::string& font) = 0;
        virtual bool SetFontScale(double scale) = 0;

        virtual SubtitleClipHolder GetClip(int64_t ms) = 0;
        virtual SubtitleClipHolder GetCurrClip() = 0;
        virtual SubtitleClipHolder GetNextClip() = 0;
        virtual bool SeekToTime(int64_t ms) = 0;
        virtual bool SeekToIndex(uint32_t index) = 0;
        virtual uint32_t ClipCount() const = 0;

        virtual std::string GetError() const = 0;

        static SubtitleTrackHolder BuildFromFile(int64_t id, const std::string& url);
    };

    bool InitializeSubtitleLibrary();
    void ReleaseSubtitleLibrary();
    bool SetFontDir(const std::string& path);
}

Logger::ALogger* GetSubtitleTrackLogger();