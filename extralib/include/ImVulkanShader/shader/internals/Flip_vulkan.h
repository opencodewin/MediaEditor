#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Flip_vulkan
{
public:
    Flip_vulkan(int gpu = -1);
    ~Flip_vulkan();

    double flip(const ImMat& src, ImMat& dst, bool bFlipX, bool bFlipY) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, bool bFlipX, bool bFlipY) const;
};
} // namespace ImGui 