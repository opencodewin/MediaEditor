#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
#include <string>

namespace ImGui
{
class VKSHADER_API Jitter_vulkan
{
public:
    Jitter_vulkan(int gpu = 0);
    ~Jitter_vulkan();
    void SetParam(float _edgeStrength);

    double effect(const ImMat& src, ImMat& dst, float playTime, float duration, float max_scale, float offset);

private:
    const VulkanDevice* vkdev   {nullptr};
    Option opt;
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float playTime, float duration, float max_scale, float offset);
};
} // namespace ImGui
