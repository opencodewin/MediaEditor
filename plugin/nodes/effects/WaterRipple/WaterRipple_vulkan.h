#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API WaterRipple_vulkan
{
public:
    WaterRipple_vulkan(int gpu = 0);
    ~WaterRipple_vulkan();
    double effect(const ImMat& src, ImMat& dst, float freq, float amount);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float freq, float amount);
};
} // namespace ImGui
