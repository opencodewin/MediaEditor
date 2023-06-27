#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API Glass_vulkan
{
public:
    Glass_vulkan(int gpu = 0);
    ~Glass_vulkan();
    double effect(const ImMat& src, ImMat& dst, float scale);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float scale);
};
} // namespace ImGui
