#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"

namespace ImGui 
{
class VKSHADER_API Resize_vulkan
{
public:
    Resize_vulkan(int gpu = -1);
    ~Resize_vulkan();

    double Resize(const ImMat& src, ImMat& dst, float fx, float fy = 0.f, ImInterpolateMode type = IM_INTERPOLATE_BICUBIC) const;

public:
    const VulkanDevice* vkdev;
    Pipeline * pipe = nullptr;
    VkCompute * cmd = nullptr;
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, ImInterpolateMode type) const;
};
} // namespace ImGui 