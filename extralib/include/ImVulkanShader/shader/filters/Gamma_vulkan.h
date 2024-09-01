#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Gamma_vulkan
{
public:
    Gamma_vulkan(int gpu = -1);
    ~Gamma_vulkan();

    double filter(const ImMat& src, ImMat& dst, float gamma) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, float gamma) const;
};
} // namespace ImGui 