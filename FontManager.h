#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

namespace FM
{
    enum FontWeight
    {
        FontWeightUndefined = 0,
        FontWeightThin = 100,
        FontWeightUltraLight = 200,
        FontWeightLight = 300,
        FontWeightNormal = 400,
        FontWeightMedium = 500,
        FontWeightSemiBold = 600,
        FontWeightBold = 700,
        FontWeightUltraBold = 800,
        FontWeightHeavy = 900
    };

    enum FontWidth
    {
        FontWidthUndefined = 0,
        FontWidthUltraCondensed = 1,
        FontWidthExtraCondensed = 2,
        FontWidthCondensed = 3,
        FontWidthSemiCondensed = 4,
        FontWidthNormal = 5,
        FontWidthSemiExpanded = 6,
        FontWidthExpanded = 7,
        FontWidthExtraExpanded = 8,
        FontWidthUltraExpanded = 9
    };

    struct FontDescriptor
    {
        virtual const std::string& Path() const = 0;
        virtual const std::string& PostscriptName() const = 0;
        virtual const std::string& Family() const = 0;
        virtual const std::string& Style() const = 0;
        virtual FontWeight Weight() const = 0;
        virtual FontWidth Width() const = 0;
        virtual bool Italic() const = 0;
        virtual bool Monospace() const = 0;
    };

    using FontDescriptorHolder = std::shared_ptr<FontDescriptor>;

    FontDescriptorHolder NewFontDescriptor(
        const char *path, const char *postscriptName, const char *family, const char *style,
        FontWeight weight, FontWidth width, bool italic, bool monospace);

    std::vector<FontDescriptorHolder> GetAvailableFonts();

    std::unordered_map<std::string, std::vector<FontDescriptorHolder>> GroupFontsByFamily(const std::vector<FontDescriptorHolder>& fonts);
}