#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API SmudgeBlur_vulkan
{
public:
    SmudgeBlur_vulkan(int gpu = 0);
    ~SmudgeBlur_vulkan();

    double effect(const ImMat& src, ImMat& dst, float radius, float iterations);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float radius, float iterations);
};
} // namespace ImGui
