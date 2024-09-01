#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API Emboss_vulkan
{
public:
    Emboss_vulkan(int gpu = 0);
    ~Emboss_vulkan();

    double filter(const ImMat& src, ImMat& dst, float intensity, float angle, int stride);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float intensity, float angle, int stride);
};
} // namespace ImGui
