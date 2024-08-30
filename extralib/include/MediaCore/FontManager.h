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
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include "MediaCore.h"

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

    MEDIACORE_API FontDescriptorHolder NewFontDescriptor(
        const char *path, const char *postscriptName, const char *family, const char *style,
        FontWeight weight, FontWidth width, bool italic, bool monospace);

    MEDIACORE_API std::vector<FontDescriptorHolder> GetAvailableFonts();

    MEDIACORE_API std::unordered_map<std::string, std::vector<FontDescriptorHolder>> GroupFontsByFamily(const std::vector<FontDescriptorHolder>& fonts);
}