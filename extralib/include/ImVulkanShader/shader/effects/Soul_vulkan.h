#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API Soul_vulkan
{
public:
    Soul_vulkan(int gpu = 0);
    ~Soul_vulkan();

    double effect(const ImMat& src, ImMat& dst, float progress, int count, float max_scale, float max_alpha, bool shrink);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float progress, int count, float max_scale, float max_alpha, bool shrink);
};
} // namespace ImGui
