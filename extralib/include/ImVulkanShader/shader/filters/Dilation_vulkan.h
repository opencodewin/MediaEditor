#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Dilation_vulkan
{
public:
    Dilation_vulkan(int gpu = -1);
    ~Dilation_vulkan();

    double filter(const ImMat& src, ImMat& dst, int ksz);

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, int ksz);
};
} // namespace ImGui 