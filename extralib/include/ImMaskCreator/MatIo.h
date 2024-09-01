#pragma once
#include <string>
#include "immat.h"

namespace MatUtils
{
    IMGUI_API bool SaveAsPng(const ImGui::ImMat& m, const std::string& savePath);
}