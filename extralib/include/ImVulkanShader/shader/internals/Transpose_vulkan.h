#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Transpose_vulkan
{
public:
    Transpose_vulkan(int gpu = -1);
    ~Transpose_vulkan();

    double transpose(const ImMat& src, ImMat& dst, bool bFlipX, bool bFlipY) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, bool bFlipX, bool bFlipY) const;
};
} // namespace ImGui 