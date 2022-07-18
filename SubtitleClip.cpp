#include "SubtitleClip.h"

using namespace std;
using namespace DataLayer;

SubtitleImage::SubtitleImage(ImGui::ImMat& image, const Rect& area)
    : m_image(image), m_area(area)
{}

SubtitleClip::SubtitleClip(SubtitleType type, int64_t startTime, int64_t duration, const char* text)
    : m_type(type), m_startTime(startTime), m_duration(duration), m_text(text)
{
}

SubtitleClip::SubtitleClip(SubtitleType type, int64_t startTime, int64_t duration, SubtitleImage& image)
    : m_type(type), m_startTime(startTime), m_duration(duration), m_image(image)
{}

bool SubtitleClip::SetFont(const string& font)
{
    return false;
}

bool SubtitleClip::SetFontScale(double scale)
{
    return false;
}

void SubtitleClip::SetTextColor(const Color& color)
{
    m_textColor = color;
}

void SubtitleClip::SetBackgroundColor(const Color& color)
{
    m_bgColor = color;
}

SubtitleImage SubtitleClip::Image()
{
    if (!m_image.Valid() && m_renderCb)
        m_image = m_renderCb(this);
    return m_image;
}