#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API Sway_vulkan
{
public:
    Sway_vulkan(int gpu = 0);
    ~Sway_vulkan();

    double effect(const ImMat& src, ImMat& dst, float speed, float strength, float density, bool horizontal);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float speed, float strength, float density, bool horizontal);
};
} // namespace ImGui
