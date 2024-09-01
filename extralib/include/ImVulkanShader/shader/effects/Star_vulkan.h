#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API Star_vulkan
{
public:
    Star_vulkan(int gpu = 0);
    ~Star_vulkan();

    double effect(const ImMat& src, ImMat& dst, float progress, float speed, int layers, ImPixel& colour);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float progress, float speed, int layers, ImPixel& colour);
};
} // namespace ImGui
