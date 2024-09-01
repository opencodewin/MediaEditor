#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API Hatch_vulkan
{
public:
    Hatch_vulkan(int gpu = 0);
    ~Hatch_vulkan();

    double filter(const ImMat& src, ImMat& dst, float spacing, float width);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float spacing, float width);
};
} // namespace ImGui
