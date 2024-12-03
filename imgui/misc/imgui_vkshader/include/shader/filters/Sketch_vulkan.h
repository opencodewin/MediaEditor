#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API Sketch_vulkan
{
public:
    Sketch_vulkan(int gpu = 0);
    ~Sketch_vulkan();

    double filter(const ImMat& src, ImMat& dst, float intensity, int step);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float intensity, int step);
};
} // namespace ImGui
