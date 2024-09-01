#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API SmartDenoise_vulkan
{
public:
    SmartDenoise_vulkan(int gpu = 0);
    ~SmartDenoise_vulkan();

    double filter(const ImMat& src, ImMat& dst, float sigma, float kSigma, float threshold);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float sigma, float kSigma, float threshold);
};
} // namespace ImGui
