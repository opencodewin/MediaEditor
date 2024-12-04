/*
    Copyright (c) 2023-2024 CodeWin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <functional>
#include "immat.h"
#include "SubtitleStyle.h"

namespace MediaCore
{
enum class SubtitleType
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
        int32_t x{0};
        int32_t y{0};
        int32_t w{0};
        int32_t h{0};
    };

    SubtitleImage() = default;
    SubtitleImage(ImGui::ImMat& image, const Rect& area)
        : m_image(image), m_area(area) {}

    ImGui::ImMat Vmat() { return m_image; }
    Rect Area() const { return m_area; }
    bool Valid() const { return !m_image.empty(); }
    void Invalidate() { m_image.release(); m_area = Rect(); }
    void UpdateArea(const Rect& r) { m_area = r; }

private:
    ImGui::ImMat m_image;
    Rect m_area;
};

struct SubtitleClip;
using SubtitleClipHolder = std::shared_ptr<SubtitleClip>;

struct SubtitleClip
{
    virtual ~SubtitleClip() {}

    virtual SubtitleType Type() const = 0;
    virtual bool IsUsingTrackStyle() const = 0;
    virtual std::string TrackStyle() const = 0;
    virtual std::string Font() const = 0;
    virtual double ScaleX() const = 0;
    virtual double ScaleY() const = 0;
    virtual double Spacing() const = 0;
    virtual SubtitleColor PrimaryColor() const = 0;
    virtual SubtitleColor SecondaryColor() const = 0;
    virtual SubtitleColor OutlineColor() const = 0;
    virtual SubtitleColor BackColor() const = 0;
    virtual SubtitleColor BackgroundColor() const = 0;
    virtual bool Bold() const = 0;
    virtual bool Italic() const = 0;
    virtual bool UnderLine() const = 0;
    virtual bool StrikeOut() const = 0;
    virtual double BorderWidth() const = 0;
    virtual double ShadowDepth() const = 0;
    virtual bool BlurEdge() const = 0;
    virtual double RotationX() const = 0;
    virtual double RotationY() const = 0;
    virtual double RotationZ() const = 0;
    virtual int32_t OffsetH() const = 0;
    virtual int32_t OffsetV() const = 0;
    virtual float OffsetHScale() const = 0;
    virtual float OffsetVScale() const = 0;
    virtual uint32_t Alignment() const = 0;
    virtual ImGui::KeyPointEditor* GetKeyPoints() = 0;
    virtual int64_t StartTime() const = 0;
    virtual int64_t Duration() const = 0;
    virtual int64_t EndTime() const = 0;
    virtual std::string Text() const = 0;
    virtual SubtitleImage Image(int64_t timeOffset = 0) = 0;

    virtual void EnableUsingTrackStyle(bool enable) = 0;
    virtual void SetTrackStyle(const std::string& name) = 0;
    virtual void SyncStyle(const SubtitleStyle& style) = 0;
    virtual void SetFont(const std::string& font) = 0;
    virtual void SetScaleX(double value) = 0;
    virtual void SetScaleY(double value) = 0;
    virtual void SetSpacing(double value) = 0;
    virtual void SetPrimaryColor(const SubtitleColor& color) = 0;
    virtual void SetSecondaryColor(const SubtitleColor& color) = 0;
    virtual void SetOutlineColor(const SubtitleColor& color) = 0;
    virtual void SetBackColor(const SubtitleColor& color) = 0;
    virtual void SetBackgroundColor(const SubtitleColor& color) = 0;
    virtual void SetPrimaryColor(const ImVec4& color) = 0;
    virtual void SetSecondaryColor(const ImVec4& color) = 0;
    virtual void SetOutlineColor(const ImVec4& color) = 0;
    virtual void SetBackColor(const ImVec4& color) = 0;
    virtual void SetBackgroundColor(const ImVec4& color) = 0;
    virtual void SetBold(bool enable) = 0;
    virtual void SetItalic(bool enable) = 0;
    virtual void SetUnderLine(bool enable) = 0;
    virtual void SetStrikeOut(bool enable) = 0;
    virtual void SetBorderWidth(double value) = 0;
    virtual void SetShadowDepth(double value) = 0;
    virtual void SetBlurEdge(bool enable) = 0;
    virtual void SetRotationX(double value) = 0;
    virtual void SetRotationY(double value) = 0;
    virtual void SetRotationZ(double value) = 0;
    virtual void SetOffsetH(int32_t value) = 0;
    virtual void SetOffsetV(int32_t value) = 0;
    virtual void SetOffsetH(float value) = 0;
    virtual void SetOffsetV(float value) = 0;
    virtual void SetAlignment(uint32_t value) = 0;
    virtual void SetKeyPoints(const ImGui::KeyPointEditor& keyPoints) = 0;
    virtual void SetText(const std::string& text) = 0;

    virtual void CloneStyle(SubtitleClipHolder from, double wRatio = 1, double hRatio = 1) = 0;
    virtual void InvalidateImage() = 0;
};
}