#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Luma_vulkan
{
public:
    Luma_vulkan(int gpu = -1);
    ~Luma_vulkan();

    double transition(const ImMat& src1, const ImMat& src2, const ImMat& mask, ImMat& dst, float progress) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src1, const VkMat& src2, const VkMat& mask, VkMat& dst, float progress) const;
};
} // namespace ImGui 