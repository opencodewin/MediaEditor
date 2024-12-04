#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API warpPerspective_vulkan
{
public:
    warpPerspective_vulkan(int gpu = -1);
    ~warpPerspective_vulkan();

    double warp(const ImMat& src, ImMat& dst, const ImMat& M, ImInterpolateMode type = IM_INTERPOLATE_NEAREST, ImPixel border_col = {0, 0, 0, 0}, ImPixel crop = {0, 0, 0, 0}) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, const VkMat& M, ImInterpolateMode type, ImPixel border_col, ImPixel crop) const;
};
} // namespace ImGui 