#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include <vector>

namespace ImGui 
{
class VKSHADER_API Binary_vulkan
{
public:
    Binary_vulkan(int gpu = -1);
    ~Binary_vulkan();

    double forward(const ImMat& src, ImMat& dst, float v_min, float v_max);

public:
    const VulkanDevice* vkdev   {nullptr};
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, float v_min, float v_max);
};
} // namespace ImGui
