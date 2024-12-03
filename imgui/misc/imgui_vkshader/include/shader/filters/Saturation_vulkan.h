#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Saturation_vulkan
{
public:
    Saturation_vulkan(int gpu = -1);
    ~Saturation_vulkan();

    double filter(const ImMat& src, ImMat& dst, float saturation) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, float saturation) const;
};
} // namespace ImGui 