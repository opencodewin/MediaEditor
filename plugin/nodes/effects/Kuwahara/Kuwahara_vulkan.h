#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API Kuwahara_vulkan
{
public:
    Kuwahara_vulkan(int gpu = 0);
    ~Kuwahara_vulkan();
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
