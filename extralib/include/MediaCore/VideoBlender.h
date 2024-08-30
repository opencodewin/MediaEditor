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
#include <immat.h>
#include "MediaCore.h"

namespace MediaCore
{
struct VideoBlender
{
    using Holder = std::shared_ptr<VideoBlender>;
    static MEDIACORE_API Holder CreateInstance();

    virtual ImGui::ImMat Blend(ImGui::ImMat& baseImage, ImGui::ImMat& overlayImage, int32_t x, int32_t y, float fOpacity = 1.f) = 0;
    virtual ImGui::ImMat Blend(ImGui::ImMat& baseImage, ImGui::ImMat& overlayImage, float fOpacity = 1.f) = 0;
    virtual ImGui::ImMat Blend(const ImGui::ImMat& baseImage, const ImGui::ImMat& overlayImage, const ImGui::ImMat& alphaMat) = 0;

    virtual bool EnableUseVulkan(bool enable) = 0;
    virtual std::string GetError() const = 0;
};
}