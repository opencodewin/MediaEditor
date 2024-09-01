#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API BarrelDistortion_vulkan
{
public:
    BarrelDistortion_vulkan(int gpu = 0);
    ~BarrelDistortion_vulkan();
    double effect(const ImMat& src, ImMat& dst, float scale, float pow);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float scale, float pow);
};
} // namespace ImGui
