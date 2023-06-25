#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API RadicalBlur_vulkan
{
public:
    RadicalBlur_vulkan(int gpu = 0);
    ~RadicalBlur_vulkan();
    double effect(const ImMat& src, ImMat& dst, float radius, float dist, float intensity, float count);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float radius, float dist, float intensity, float count);
};
} // namespace ImGui
