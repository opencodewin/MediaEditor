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
        virtual ~SubtitleTrack() {}

        virtual int64_t Id() const = 0;
        virtual uint32_t ClipCount() const = 0;
        virtual int64_t Duration() const = 0;
        virtual const SubtitleStyle& DefaultStyle() const = 0;

        virtual bool SetFrameSize(uint32_t width, uint32_t height) = 0;
        virtual bool IsFullSizeOutput() const = 0;
        virtual bool EnableFullSizeOutput(bool enable) = 0;
        virtual bool SetFont(const std::string& font) = 0;
        virtual bool SetScaleX(double value) = 0;
        virtual bool SetScaleY(double value) = 0;
        virtual bool SetSpacing(double value) = 0;
        virtual bool SetAngle(double value) = 0;
        virtual bool SetOutlineWidth(double value) = 0;
        virtual bool SetShadowDepth(double value) = 0;
        virtual bool SetBorderStyle(int value) = 0;
        virtual bool SetAlignment(int value) = 0;
        virtual bool SetOffsetH(int32_t value) = 0;
        virtual bool SetOffsetV(int32_t value) = 0;
        virtual bool SetOffsetCompensationV(int32_t value) = 0;
        virtual int32_t GetOffsetCompensationV() const = 0;
        virtual bool SetItalic(int value) = 0;
        virtual bool SetBold(int value) = 0;
        virtual bool SetUnderLine(bool enable) = 0;
        virtual bool SetStrikeOut(bool enable) = 0;
        virtual bool SetPrimaryColor(const SubtitleColor& color) = 0;
        virtual bool SetSecondaryColor(const SubtitleColor& color) = 0;
        virtual bool SetOutlineColor(const SubtitleColor& color) = 0;
        virtual bool SetBackColor(const SubtitleColor& color) = 0;
        virtual bool SetBackgroundColor(const SubtitleColor& color) = 0;
        virtual bool SetPrimaryColor(const ImVec4& color) = 0;
        virtual bool SetSecondaryColor(const ImVec4& color) = 0;
        virtual bool SetOutlineColor(const ImVec4& color) = 0;
        virtual bool SetBackColor(const ImVec4& color) = 0;
        virtual bool SetBackgroundColor(const ImVec4& color) = 0;
        virtual void Refresh() = 0;

        // currently supported key-name: Scale, ScaleX, ScaleY, Spacing, Angle, OutlineWidth, ShadowDepth, OffsetH, OffsetV
        virtual bool SetKeyPoints(const ImGui::KeyPointEditor& keyPoints) = 0;
        virtual ImGui::KeyPointEditor* GetKeyPoints() = 0;

        virtual SubtitleClipHolder NewClip(int64_t startTime, int64_t duration) = 0;
        virtual bool DeleteClip(SubtitleClipHolder hClip) = 0;
        virtual bool ChangeClipTime(SubtitleClipHolder clip, int64_t startTime, int64_t duration) = 0;
        virtual SubtitleClipHolder GetClipByTime(int64_t ms) = 0;
        virtual SubtitleClipHolder GetCurrClip() = 0;
        virtual SubtitleClipHolder GetPrevClip() = 0;
        virtual SubtitleClipHolder GetNextClip() = 0;
        virtual int32_t GetClipIndex(SubtitleClipHolder clip) const = 0;
        virtual uint32_t GetCurrIndex() const = 0;
        virtual bool SeekToTime(int64_t ms) = 0;
        virtual bool SeekToIndex(uint32_t index) = 0;

        virtual bool IsVisible() const = 0;
        virtual void SetVisible(bool enable) = 0;
        virtual bool SaveAs(const std::string& subFilePath) = 0;

        virtual SubtitleTrackHolder Clone(uint32_t frmW, uint32_t frmH) = 0;
        virtual std::string GetError() const = 0;

        static SubtitleTrackHolder BuildFromFile(int64_t id, const std::string& url);
        static SubtitleTrackHolder NewEmptyTrack(int64_t id);
    };

    bool InitializeSubtitleLibrary();
    void ReleaseSubtitleLibrary();
    bool SetFontDir(const std::string& path);
}

Logger::ALogger* GetSubtitleTrackLogger();