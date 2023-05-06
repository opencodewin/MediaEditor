#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API BurnOut_vulkan
{
public:
    BurnOut_vulkan(int gpu = -1);
    ~BurnOut_vulkan();

    double transition(const ImMat& src1, const ImMat& src2, ImMat& dst, float progress, ImPixel& shadow_colour, float smoothness) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src1, const VkMat& src2, VkMat& dst, float progress, ImPixel& shadow_colour, float smoothness) const;
};
} // namespace ImGui 