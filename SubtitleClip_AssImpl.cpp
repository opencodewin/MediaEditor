#include <sstream>
#include "SubtitleClip_AssImpl.h"

using namespace std;
using namespace DataLayer;

SubtitleClip_AssImpl::SubtitleClip_AssImpl(
        int layer, int readOrder, const string& trackStyle,
        int64_t startTime, int64_t duration,
        const std::string& text,
        AssRenderCallback renderCb)
    : m_type(DataLayer::ASS)
    , m_layer(layer), m_readOrder(readOrder), m_trackStyle(trackStyle)
    , m_startTime(startTime), m_duration(duration)
    , m_text(text)
    , m_renderCb(renderCb)
{}

SubtitleClip_AssImpl::SubtitleClip_AssImpl(ASS_Event* assEvent, const string& trackStyle, AssRenderCallback renderCb)
    : m_type(DataLayer::ASS), m_assEvent(assEvent), m_renderCb(renderCb)
    , m_layer(assEvent->Layer), m_readOrder(assEvent->ReadOrder), m_trackStyle(trackStyle)
    , m_startTime(assEvent->Start), m_duration(assEvent->Duration)
    , m_text(string(assEvent->Text))
{}

SubtitleImage SubtitleClip_AssImpl::Image()
{
    if (!m_image.Valid() && m_renderCb)
    {
        if (m_assTextChanged)
        {
            if (m_assEvent)
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
            }
            m_assTextChanged = false;
        }
        m_image = m_renderCb(this);
    }
    return m_image;
}

void SubtitleClip_AssImpl::EnableUsingTrackStyle(bool enable)
{
    m_useTrackStyle = enable;
    m_image.Invalidate();
}

void SubtitleClip_AssImpl::SetTrackStyle(const std::string& name)
{
    m_trackStyle = name;
    if (!m_useTrackStyle)
        m_image.Invalidate();
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
    SetAlignment(style.Alignment());
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetFont(const std::string& font)
{
    if (m_font == font)
        return;
    m_font = font;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
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
    if (m_scaleX == value)
        return;
    m_scaleX = value;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetScaleY(double value)
{
    if (m_scaleY == value)
        return;
    m_scaleY = value;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetSpacing(double value)
{
    if (m_spacing == value)
        return;
    m_spacing = value > 10 ? 10 : value;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetPrimaryColor(const SubtitleColor& color)
{
    if (m_primaryColor == color)
        return;
    m_primaryColor = color;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetSecondaryColor(const SubtitleColor& color)
{
    if (m_secondaryColor == color)
        return;
    m_secondaryColor = color;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetOutlineColor(const SubtitleColor& color)
{
    if (m_outlineColor == color)
        return;
    m_outlineColor = color;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetBackgroundColor(const SubtitleColor& color)
{
    m_bgColor = color;
}

void SubtitleClip_AssImpl::SetBold(bool enable)
{
    if (m_bold == enable)
        return;
    m_bold = enable;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetItalic(bool enable)
{
    if (m_italic == enable)
        return;
    m_italic = enable;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetUnderline(bool enable)
{
    if (m_underline == enable)
        return;
    m_underline = enable;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetStrikeOut(bool enable)
{
    if (m_strikeout == enable)
        return;
    m_strikeout = enable;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetBorderWidth(uint32_t value)
{
    if (m_borderWidth == value)
        return;
    m_borderWidth = value > 5 ? 5 : value;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

// void SubtitleClip_AssImpl::SetShadowDepth(uint32_t value)
// {
//     m_shadowDepth = value > 5 ? 5 : value;
//     if (!m_useTrackStyle)
//         m_image.Invalidate();
// }

void SubtitleClip_AssImpl::SetBlurEdge(bool enable) 
{
    if (m_blurEdge == enable)
        return;
    m_blurEdge = enable;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetRotationX(double value)
{
    if (m_rotationX == value)
        return;
    value -= (int64_t)(value/360)*360;
    m_rotationX = value;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetRotationY(double value)
{
    if (m_rotationY == value)
        return;
    value -= (int64_t)(value/360)*360;
    m_rotationY = value;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetRotationZ(double value)
{
    if (m_rotationZ == value)
        return;
    value -= (int64_t)(value/360)*360;
    m_rotationZ = value;
    if (!m_useTrackStyle)
    {
        m_assTextChanged = true;
        m_image.Invalidate();
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
        m_assTextChanged = true;
        m_image.Invalidate();
    }
}

void SubtitleClip_AssImpl::SetText(const std::string& text)
{
    if (m_text == text)
        return;
    m_text = text;
    m_assTextChanged = true;
    m_image.Invalidate();
}

void SubtitleClip_AssImpl::InvalidateImage()
{
    m_image.Invalidate();
}

void SubtitleClip_AssImpl::SetReadOrder(int readOrder)
{
    if (m_readOrder == readOrder)
        return;
    m_readOrder = readOrder;
    if (m_assEvent)
        m_assEvent->ReadOrder = readOrder;
    m_image.Invalidate();
}

void SubtitleClip_AssImpl::SetLayer(int layer)
{
    if (m_layer == layer)
        return;
    m_layer = layer;
    if (m_assEvent)
        m_assEvent->Layer = layer;
    m_image.Invalidate();
}

void SubtitleClip_AssImpl::SetStartTime(int64_t startTime)
{
    if (m_startTime == startTime)
        return;
    m_startTime = startTime;
    if (m_assEvent)
        m_assEvent->Start = startTime;
    m_image.Invalidate();
}

void SubtitleClip_AssImpl::SetDuration(int64_t duration)
{
    if (m_duration == duration)
        return;
    m_duration = duration;
    if (m_assEvent)
        m_assEvent->Duration = duration;
    m_image.Invalidate();
}

string SubtitleClip_AssImpl::GenerateAssChunk()
{
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
    return (uint32_t)(color.a*255)&0xff;
}

string SubtitleClip_AssImpl::GenerateStyledText()
{
    if (!m_assTextChanged)
        return m_styledText;

    ostringstream oss;
    oss << "{";
    oss << "\\fn" << m_font;
    // oss << "\\fs" << m_fontSize;
    oss << "\\fscx" << m_scaleX*100;
    oss << "\\fscy" << m_scaleY*100;
    oss << "\\fsp" << m_spacing;
    oss << "\\1c" << hex << ToAssColor(m_primaryColor) << "\1a" << ToAssAlpha(m_primaryColor);
    oss << "\\2c" << hex << ToAssColor(m_secondaryColor) << "\2a" << ToAssAlpha(m_secondaryColor);
    oss << "\\3c" << hex << ToAssColor(m_outlineColor) << "\3a" << ToAssAlpha(m_outlineColor);
    oss << "\\b" << (m_bold?1:0);
    oss << "\\i" << (m_italic?1:0);
    oss << "\\u" << (m_underline?1:0);
    oss << "\\s" << (m_strikeout?1:0);
    oss << "\\bord" << m_borderWidth;
    // oss << "\\shad" << m_shadowDepth;
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
