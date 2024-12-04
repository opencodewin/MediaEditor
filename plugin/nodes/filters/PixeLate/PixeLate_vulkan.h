#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API PixeLate_vulkan
{
public:
    PixeLate_vulkan(int gpu = 0);
    ~PixeLate_vulkan();

    double effect(const ImMat& src, ImMat& dst, float radius);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float radius);
};
} // namespace ImGui
