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

#include <sstream>
#include "SubtitleClip_AssImpl.h"
#include "Logger.h"

using namespace std;
using namespace MediaCore;
using namespace Logger;

SubtitleClip_AssImpl::SubtitleClip_AssImpl(ASS_Event* assEvent, ASS_Track* assTrack, AssRenderCallback renderCb)
    : m_type(SubtitleType::ASS), m_assEvent(assEvent), m_assTrack(assTrack), m_renderCb(renderCb)
    , m_readOrder(assEvent->ReadOrder), m_trackStyle(assTrack->styles[assEvent->Style].Name)
    , m_text(string(assEvent->Text))
{}

SubtitleImage SubtitleClip_AssImpl::Image(int64_t timeOffset)
{
    if (!m_assEvent || !m_renderCb)
        return SubtitleImage();
    if (timeOffset < 0 || timeOffset >= Duration())
        return SubtitleImage();

    auto iter = m_renderedImages.find(timeOffset);
    if (iter == m_renderedImages.end())
    {
        bool x_absolute = false;
        bool y_absolute = false;
        int64_t pos = timeOffset;
        for (int i = 0; i < m_keyPoints.GetCurveCount(); i++)
        {
            auto name = m_keyPoints.GetCurveName(i);
            auto value = m_keyPoints.GetValueByDim(i, pos, ImGui::ImCurveEdit::DIM_X);
            if (name == "Scale")
            {
                _SetScaleX(value, false);
                _SetScaleY(value, false);
            }
            else if (name == "ScaleX")
                _SetScaleX(value, false);
            else if (name == "ScaleY")
                _SetScaleY(value, false);
            else if (name == "Spacing")
                _SetSpacing(value, false);
            else if (name == "OutlineWidth" || name == "BorderWidth")
                _SetBorderWidth(value, false);
            else if (name == "ShadowDepth")
                _SetShadowDepth(value, false);
            else if (name == "AngleX" || name == "RotationX")
                _SetRotationX(value, false);
            else if (name == "AngleY" || name == "RotationY")
                _SetRotationY(value, false);
            else if (name == "AngleZ" || name == "RotationZ")
                _SetRotationZ(value, false);
            else if (name == "OffsetH")
            {
                _SetOffsetH(value, false);
                x_absolute = true;
            }
            else if (name == "OffsetV")
            {
                _SetOffsetV(value, false);
                y_absolute = true;
            }
            else
                Log(WARN) << "[SubtitleClip_AssImpl] UNKNOWN curve name '" << name << "', value=" << value << "." << endl;
        }
        if (m_styledTextNeedUpdate)
        {
            if (m_assEvent->Text)
                free(m_assEvent->Text);
            if (!m_useTrackStyle)
                GenerateStyledText();
            string& assText = m_useTrackStyle ? m_text : m_styledText;
            int len = assText.size();
            m_assEvent->Text = (char*)malloc(len+1);
            memcpy(m_assEvent->Text, assText.c_str(), len);
            m_assEvent->Text[len] = 0;
            m_styledTextNeedUpdate = false;
        }
        m_renderedImages[timeOffset] = m_renderCb(this, timeOffset, x_absolute, y_absolute);
    }
    return m_renderedImages[timeOffset];
}

void SubtitleClip_AssImpl::EnableUsingTrackStyle(bool enable)
{
    if (m_useTrackStyle == enable)
        return;
    m_useTrackStyle = enable;
    m_styledTextNeedUpdate = true;
    m_renderedImages.clear();
}

void SubtitleClip_AssImpl::SetTrackStyle(const std::string& name)
{
    if (!m_assEvent || m_trackStyle == name)
        return;

    int targetIdx = -1, defaultIdx = -1;
    for (int i = 0; i < m_assTrack->n_styles; i++)
    {
        string styleName(m_assTrack->styles[i].Name);
        if (styleName == name)
            targetIdx = i;
        if (styleName == "Default")
            defaultIdx = i;
    }
    if (targetIdx >= 0)
        m_assEvent->Style = targetIdx;
    else if (defaultIdx >= 0)
        m_assEvent->Style = defaultIdx;
    else if (m_assTrack->n_styles > 0)
        m_assEvent->Style = m_assTrack->n_styles-1;
    else
        m_assEvent->Style = 0;

    if (m_assTrack->n_styles > 0)
        m_trackStyle = string(m_assTrack->styles[m_assEvent->Style].Name);
    else
        m_trackStyle = "Default";

    if (m_useTrackStyle)
        m_renderedImages.clear();
}

void SubtitleClip_AssImpl::SyncStyle(const SubtitleStyle& style)
{
    m_font = style.Font();
    m_scaleX = style.ScaleX();
    m_scaleY = style.ScaleY();
    m_spacing = style.Spacing();
    m_primaryColor = style.PrimaryColor();
    m_secondaryColor = style.SecondaryColor();
    m_outlineColor = style.OutlineColor();
    m_bgColor = style.BackgroundColor();
    m_bold = style.Bold() > 0;
    m_italic = style.Italic() > 0;
    m_underline = style.UnderLine();
    m_strikeout = style.StrikeOut();
    const double olw = style.OutlineWidth();
    m_borderWidth = olw>5 ? 5 : (olw<0 ? 0 : olw);
    m_blurEdge = false;
    m_rotationX = 0;
    m_rotationY = 0;
    m_rotationZ = style.Angle();
    m_offsetH = style.OffsetH();
    m_offsetV = style.OffsetV();
    m_foffsetH = style.OffsetHScale();
    m_foffsetV = style.OffsetVScale();
    SetAlignment(style.Alignment());

    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetFont(const std::string& font)
{
    if (m_font == font)
        return;
    m_font = font;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        m_renderedImages.clear();
    }
}

// void SubtitleClip_AssImpl::SetFontSize(uint32_t value)
// {
//     m_fontSize = value > 50 ? 50 : value;
//     if (!m_useTrackStyle)
//         m_image.Invalidate();
// }

void SubtitleClip_AssImpl::SetScaleX(double value)
{
    return _SetScaleX(value, true);
}

void SubtitleClip_AssImpl::_SetScaleX(double value, bool clearCache)
{
    if (m_scaleX == value)
        return;
    m_scaleX = value;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        if (clearCache)
            m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetScaleY(double value)
{
    return _SetScaleY(value, true);
}

void SubtitleClip_AssImpl::_SetScaleY(double value, bool clearCache)
{
    if (m_scaleY == value)
        return;
    m_scaleY = value;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        if (clearCache)
            m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetSpacing(double value)
{
    return _SetSpacing(value, true);
}

void SubtitleClip_AssImpl::_SetSpacing(double value, bool clearCache)
{
    if (m_spacing == value)
        return;
    m_spacing = value > 10 ? 10 : value;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        if (clearCache)
            m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetBorderWidth(double value)
{
    return _SetBorderWidth(value, true);
}

void SubtitleClip_AssImpl::_SetBorderWidth(double value, bool clearCache)
{
    if (m_borderWidth == value)
        return;
    m_borderWidth = value;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        if (clearCache)
            m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetShadowDepth(double value)
{
    return _SetShadowDepth(value, true);
}

void SubtitleClip_AssImpl::_SetShadowDepth(double value, bool clearCache)
{
    if (m_shadowDepth == value)
        return;
    m_shadowDepth = value;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        if (clearCache)
            m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetRotationX(double value)
{
    return _SetRotationX(value, true);
}

void SubtitleClip_AssImpl::_SetRotationX(double value, bool clearCache)
{
    if (m_rotationX == value)
        return;
    value -= (int64_t)(value/360)*360;
    m_rotationX = value;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        if (clearCache)
            m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetRotationY(double value)
{
    return _SetRotationY(value, true);
}

void SubtitleClip_AssImpl::_SetRotationY(double value, bool clearCache)
{
    if (m_rotationY == value)
        return;
    value -= (int64_t)(value/360)*360;
    m_rotationY = value;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        if (clearCache)
            m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetRotationZ(double value)
{
    return _SetRotationZ(value, true);
}

void SubtitleClip_AssImpl::_SetRotationZ(double value, bool clearCache)
{
    if (m_rotationZ == value)
        return;
    value -= (int64_t)(value/360)*360;
    m_rotationZ = value;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        if (clearCache)
            m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetOffsetH(int32_t value)
{
    return _SetOffsetH(value, true);
}

void SubtitleClip_AssImpl::_SetOffsetH(int32_t value, bool clearCache)
{
    if (m_offsetH == value)
        return;
    m_offsetH = value;
    if (!m_useTrackStyle && clearCache)
        m_renderedImages.clear();
}

void SubtitleClip_AssImpl::SetOffsetV(int32_t value)
{
    return _SetOffsetV(value, true);
}

void SubtitleClip_AssImpl::_SetOffsetV(int32_t value, bool clearCache)
{
    if (m_offsetV == value)
        return;
    m_offsetV = value;
    if (!m_useTrackStyle && clearCache)
        m_renderedImages.clear();
}

void SubtitleClip_AssImpl::SetOffsetH(float value)
{
    return _SetOffsetH(value, true);
}

void SubtitleClip_AssImpl::_SetOffsetH(float value, bool clearCache)
{
    if (m_foffsetH == value)
        return;
    m_foffsetH = value;
    if (!m_useTrackStyle && clearCache)
        m_renderedImages.clear();
}

void SubtitleClip_AssImpl::SetOffsetV(float value)
{
    return _SetOffsetV(value, true);
}

void SubtitleClip_AssImpl::_SetOffsetV(float value, bool clearCache)
{
    if (m_foffsetV == value)
        return;
    m_foffsetV = value;
    if (!m_useTrackStyle && clearCache)
        m_renderedImages.clear();
}

void SubtitleClip_AssImpl::SetPrimaryColor(const SubtitleColor& color)
{
    if (m_primaryColor == color)
        return;
    m_primaryColor = color;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetSecondaryColor(const SubtitleColor& color)
{
    if (m_secondaryColor == color)
        return;
    m_secondaryColor = color;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetOutlineColor(const SubtitleColor& color)
{
    if (m_outlineColor == color)
        return;
    m_outlineColor = color;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetBackColor(const SubtitleColor& color)
{
    if (m_backColor == color)
        return;
    m_backColor = color;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetBackgroundColor(const SubtitleColor& color)
{
    if (m_bgColor == color)
        return;
    m_bgColor = color;
    if (!m_useTrackStyle)
    {
        m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetPrimaryColor(const ImVec4& color)
{
    const SubtitleColor _color(color.x, color.y, color.z, color.w);
    SetPrimaryColor(_color);
}

void SubtitleClip_AssImpl::SetSecondaryColor(const ImVec4& color)
{
    const SubtitleColor _color(color.x, color.y, color.z, color.w);
    SetSecondaryColor(_color);
}

void SubtitleClip_AssImpl::SetOutlineColor(const ImVec4& color)
{
    const SubtitleColor _color(color.x, color.y, color.z, color.w);
    SetOutlineColor(_color);
}

void SubtitleClip_AssImpl::SetBackColor(const ImVec4& color)
{
    const SubtitleColor _color(color.x, color.y, color.z, color.w);
    SetBackColor(_color);
}

void SubtitleClip_AssImpl::SetBackgroundColor(const ImVec4& color)
{
    const SubtitleColor _color(color.x, color.y, color.z, color.w);
    SetBackgroundColor(_color);
}

void SubtitleClip_AssImpl::SetBold(bool enable)
{
    if (m_bold == enable)
        return;
    m_bold = enable;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetItalic(bool enable)
{
    if (m_italic == enable)
        return;
    m_italic = enable;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetUnderLine(bool enable)
{
    if (m_underline == enable)
        return;
    m_underline = enable;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetStrikeOut(bool enable)
{
    if (m_strikeout == enable)
        return;
    m_strikeout = enable;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetBlurEdge(bool enable) 
{
    if (m_blurEdge == enable)
        return;
    m_blurEdge = enable;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetAlignment(uint32_t value)
{
    if (m_alignment == value)
        return;
    value = value<1 ? 1 : (value>9 ? 9 : value);
    m_alignment = value;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetKeyPoints(const ImGui::KeyPointEditor& keyPoints)
{
    m_keyPoints = keyPoints;
    if (!m_useTrackStyle)
    {
        m_styledTextNeedUpdate = true;
        m_renderedImages.clear();
    }
}

void SubtitleClip_AssImpl::SetText(const std::string& text)
{
    if (m_text == text)
        return;
    m_text = text;
    m_styledTextNeedUpdate = true;
    m_renderedImages.clear();
}

void SubtitleClip_AssImpl::CloneStyle(SubtitleClipHolder from, double wRatio, double hRatio)
{
    m_useTrackStyle = from->IsUsingTrackStyle();
    SetTrackStyle(from->TrackStyle());
    SetFont(from->Font());
    SetScaleX(from->ScaleX());
    SetScaleY(from->ScaleY());
    SetSpacing(from->Spacing());
    SetPrimaryColor(from->PrimaryColor());
    SetSecondaryColor(from->SecondaryColor());
    SetOutlineColor(from->OutlineColor());
    SetBackgroundColor(from->BackgroundColor());
    SetBold(from->Bold());
    SetItalic(from->Italic());
    SetUnderLine(from->UnderLine());
    SetStrikeOut(from->StrikeOut());
    SetBorderWidth(from->BorderWidth());
    SetBlurEdge(from->BlurEdge());
    SetRotationX(from->RotationX());
    SetRotationY(from->RotationY());
    SetRotationZ(from->RotationZ());
    SetOffsetH((int32_t)(from->OffsetH()*wRatio));
    SetOffsetV((int32_t)(from->OffsetV()*hRatio));
    SetOffsetH(from->OffsetHScale());
    SetOffsetV(from->OffsetVScale());
    SetAlignment(from->Alignment());
    SetKeyPoints(*(from->GetKeyPoints()));
}

void SubtitleClip_AssImpl::InvalidateImage()
{
    m_renderedImages.clear();
}

void SubtitleClip_AssImpl::ResyncAssEventPtr(ASS_Event* assEvent)
{
    if (assEvent->ReadOrder != m_readOrder)
        throw runtime_error("Ass event readorder does NOT MATCH!");
    m_assEvent = assEvent;
}

void SubtitleClip_AssImpl::SetStartTime(int64_t startTime)
{
    if (!m_assEvent)
        return;
    m_assEvent->Start = startTime;
    m_renderedImages.clear();
}

void SubtitleClip_AssImpl::SetDuration(int64_t duration)
{
    if (!m_assEvent)
        return;
    m_assEvent->Duration = duration;
    m_renderedImages.clear();
}

string SubtitleClip_AssImpl::GenerateAssChunk()
{
    if (!m_assEvent)
        return string();
    ostringstream oss;
    oss << m_assEvent->ReadOrder << "," << m_assEvent->Layer << "," << m_trackStyle << ",,0,0,0,," << GetAssText();
    return oss.str();
}

static inline uint32_t ToAssColor(SubtitleColor color)
{
    return (((uint32_t)(color.b*255)&0xff)<<16)|(((uint32_t)(color.g*255)&0xff)<<8)|((uint32_t)(color.r*255)&0xff);
}

static inline uint32_t ToAssAlpha(SubtitleColor color)
{
    return (uint32_t)((1-color.a)*255)&0xff;
}

string SubtitleClip_AssImpl::GenerateStyledText()
{
    if (!m_styledTextNeedUpdate)
        return m_styledText;

    ostringstream oss;
    oss << "{";
    oss << "\\fn" << m_font;
    // oss << "\\fs" << m_fontSize;
    oss << "\\fscx" << m_scaleX*100;
    oss << "\\fscy" << m_scaleY*100;
    oss << "\\fsp" << m_spacing;
    oss << "\\1c&H" << hex << ToAssColor(m_primaryColor) << "&\\1a&H" << ToAssAlpha(m_primaryColor) << "&";
    oss << "\\2c&H" << hex << ToAssColor(m_secondaryColor) << "&\\2a&H" << ToAssAlpha(m_secondaryColor) << "&";
    oss << "\\3c&H" << hex << ToAssColor(m_outlineColor) << "&\\3a&H" << ToAssAlpha(m_outlineColor) << "&";
    oss << "\\4c&H" << hex << ToAssColor(m_backColor) << "&\\4a&H" << ToAssAlpha(m_backColor) << "&";
    oss << dec;
    oss << "\\b" << (m_bold?1:0);
    oss << "\\i" << (m_italic?1:0);
    oss << "\\u" << (m_underline?1:0);
    oss << "\\s" << (m_strikeout?1:0);
    oss << "\\bord" << m_borderWidth;
    oss << "\\shad" << m_shadowDepth;
    oss << "\\be" << (m_blurEdge?1:0);
    oss << "\\frx" << m_rotationX;
    oss << "\\fry" << m_rotationY;
    oss << "\\frz" << m_rotationZ;
    int a = m_alignment;
    if (a >= 4 && a <= 6)
        a += 5;
    else if (a >= 7 && a <= 9)
        a -= 2;
    else if (a < 1 || a > 3)
        a = 2;
    oss << "\\a" << a;
    oss << "}";
    oss << m_text;
    m_styledText = oss.str();
    return m_styledText;
}

string SubtitleClip_AssImpl::GetAssText()
{
    return m_useTrackStyle ? m_text : GenerateStyledText();
}

void SubtitleClip_AssImpl::UpdateImageAreaX(int32_t bias)
{
    for (auto& elem : m_renderedImages)
    {
        auto& image = elem.second;
        SubtitleImage::Rect r{image.Area()};
        r.x += bias;
        image.UpdateArea(r);
    }
}

void SubtitleClip_AssImpl::UpdateImageAreaY(int32_t bias)
{
    for (auto& elem : m_renderedImages)
    {
        auto& image = elem.second;
        SubtitleImage::Rect r{image.Area()};
        r.y += bias;
        image.UpdateArea(r);
    }
}

void SubtitleClip_AssImpl::InvalidateClip()
{
    m_assTrack = nullptr;
    m_assEvent = nullptr;
    m_renderedImages.clear();
}
