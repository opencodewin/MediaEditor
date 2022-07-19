#include "FontManager.h"

using namespace std;
using namespace FM;

class FontDescriptor_Impl : public FontDescriptor
{
public:
    FontDescriptor_Impl() = default;
    FontDescriptor_Impl(const FontDescriptor_Impl&) = default;
    FontDescriptor_Impl(FontDescriptor_Impl&&) = default;
    FontDescriptor_Impl& operator=(const FontDescriptor_Impl&) = default;

    FontDescriptor_Impl(const char *path, const char *postscriptName, const char *family, const char *style,
                        FontWeight weight, FontWidth width, bool italic, bool monospace)
        : m_path(path), m_postscriptName(postscriptName), m_family(family), m_style(style)
        , m_weight(weight), m_width(width), m_italic(italic), m_monospace(monospace)
    {}

    FontDescriptor_Impl(const FontDescriptor& a)
        : m_path(a.Path()), m_postscriptName(a.PostscriptName()), m_family(a.Family()), m_style(a.Style())
        , m_weight(a.Weight()), m_width(a.Width()), m_italic(a.Italic()), m_monospace(a.Monospace())
    {}

    const std::string& Path() const override
    {
        return m_path;
    }

    const std::string& PostscriptName() const override
    {
        return m_postscriptName;
    }

    const std::string& Family() const override
    {
        return m_family;
    }

    const std::string& Style() const override
    {
        return m_style;
    }

    FontWeight Weight() const override
    {
        return m_weight;
    }

    FontWidth Width() const override
    {
        return m_width;
    }

    bool Italic() const override
    {
        return m_italic;
    }

    bool Monospace() const override
    {
        return m_monospace;
    }

private:
    string m_path;
    string m_postscriptName;
    string m_family;
    string m_style;
    FontWeight m_weight{FontWeightUndefined};
    FontWidth m_width{FontWidthUndefined};
    bool m_italic{false};
    bool m_monospace{false};
};

FontDescriptorHolder FM::NewFontDescriptor(
        const char *path, const char *postscriptName, const char *family, const char *style,
        FontWeight weight, FontWidth width, bool italic, bool monospace)
{
    return FontDescriptorHolder(new FontDescriptor_Impl(path, postscriptName, family, style, weight, width, italic, monospace));
}
