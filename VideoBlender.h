#pragma once
#include <string>
#include <memory>
#include <immat.h>

namespace DataLayer
{
    struct VideoBlender
    {
        virtual bool Init() = 0;
        virtual ImGui::ImMat Blend(ImGui::ImMat& baseImage, ImGui::ImMat& overlayImage, int32_t x, int32_t y) = 0;
        virtual bool Init(const std::string& inputFormat, uint32_t w1, uint32_t h1, uint32_t w2, uint32_t h2, int32_t x, int32_t y) = 0;
        virtual ImGui::ImMat Blend(ImGui::ImMat& baseImage, ImGui::ImMat& overlayImage) = 0;

        virtual std::string GetError() const = 0;
    };

    using VideoBlenderHolder = std::shared_ptr<VideoBlender>;

    VideoBlenderHolder CreateVideoBlender();
}