#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <functional>
#include "immat.h"
#include "SubtitleStyle.h"

namespace DataLayer
{
    enum SubtitleType
    {
        UNKNOWN = 0,
        TEXT,
        BITMAP,
        ASS,
    };

    class SubtitleImage
    {
    public:
        struct Rect
        {
            int32_t x;
            int32_t y;
            int32_t w;
            int32_t h;
        };

        SubtitleImage() = default;
        SubtitleImage(ImGui::ImMat& image, const Rect& area);

        ImGui::ImMat Vmat() const { return m_image; }
        Rect Area() const { return m_area; }
        bool Valid() const { return !m_image.empty(); }
        void Invalidate() { m_image.release(); }
        void UpdateArea(const Rect& r) { m_area = r; }

    private:
        ImGui::ImMat m_image;
        Rect m_area;
    };

    struct SubtitleClip;
    using SubtitleClipHolder = std::shared_ptr<SubtitleClip>;

    struct SubtitleClip
    {
        virtual SubtitleType Type() const = 0;
        virtual bool IsUsingTrackStyle() const = 0;
        virtual std::string TrackStyle() const = 0;
        virtual std::string Font() const = 0;
        // virtual uint32_t FontSize() const = 0;
        virtual double ScaleX() const = 0;
        virtual double ScaleY() const = 0;
        virtual double Spacing() const = 0;
        virtual SubtitleColor PrimaryColor() const = 0;
        virtual SubtitleColor SecondaryColor() const = 0;
        virtual SubtitleColor OutlineColor() const = 0;
        virtual SubtitleColor BackgroundColor() const = 0;
        virtual bool Bold() const = 0;
        virtual bool Italic() const = 0;
        virtual bool UnderLine() const = 0;
        virtual bool StrikeOut() const = 0;
        virtual uint32_t BorderWidth() const = 0;
        // virtual uint32_t ShadowDepth() const = 0;
        virtual bool BlurEdge() const = 0;
        virtual double RotationX() const = 0;
        virtual double RotationY() const = 0;
        virtual double RotationZ() const = 0;
        virtual int32_t OffsetH() const = 0;
        virtual int32_t OffsetV() const = 0;
        virtual uint32_t Alignment() const = 0;
        virtual int64_t StartTime() const = 0;
        virtual int64_t Duration() const = 0;
        virtual int64_t EndTime() const = 0;
        virtual std::string Text() const = 0;
        virtual SubtitleImage Image() = 0;

        virtual void EnableUsingTrackStyle(bool enable) = 0;
        virtual void SetTrackStyle(const std::string& name) = 0;
        virtual void SyncStyle(const SubtitleStyle& style) = 0;
        virtual void SetFont(const std::string& font) = 0;
        // virtual void SetFontSize(uint32_t value) = 0;
        virtual void SetScaleX(double value) = 0;
        virtual void SetScaleY(double value) = 0;
        virtual void SetSpacing(double value) = 0;
        virtual void SetPrimaryColor(const SubtitleColor& color) = 0;
        virtual void SetSecondaryColor(const SubtitleColor& color) = 0;
        virtual void SetOutlineColor(const SubtitleColor& color) = 0;
        virtual void SetBackgroundColor(const SubtitleColor& color) = 0;
        virtual void SetPrimaryColor(const ImVec4& color) = 0;
        virtual void SetSecondaryColor(const ImVec4& color) = 0;
        virtual void SetOutlineColor(const ImVec4& color) = 0;
        virtual void SetBackgroundColor(const ImVec4& color) = 0;
        virtual void SetBold(bool enable) = 0;
        virtual void SetItalic(bool enable) = 0;
        virtual void SetUnderLine(bool enable) = 0;
        virtual void SetStrikeOut(bool enable) = 0;
        virtual void SetBorderWidth(uint32_t value) = 0;
        // virtual void SetShadowDepth(uint32_t value) = 0;
        virtual void SetBlurEdge(bool enable) = 0;
        virtual void SetRotationX(double value) = 0;
        virtual void SetRotationY(double value) = 0;
        virtual void SetRotationZ(double value) = 0;
        virtual void SetOffsetH(int32_t value) = 0;
        virtual void SetOffsetV(int32_t value) = 0;
        virtual void SetAlignment(uint32_t value) = 0;
        virtual void SetText(const std::string& text) = 0;
        virtual void InvalidateImage() = 0;
    };
}