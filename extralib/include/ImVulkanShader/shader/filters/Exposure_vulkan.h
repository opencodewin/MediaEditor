#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Exposure_vulkan
{
public:
    Exposure_vulkan(int gpu = -1);
    ~Exposure_vulkan();

    double filter(const ImMat& src, ImMat& dst, float exposure) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, float exposure) const;
};
} // namespace ImGui 