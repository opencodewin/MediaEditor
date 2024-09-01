#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Polar_vulkan
{
public:
    Polar_vulkan(int gpu = -1);
    ~Polar_vulkan();

    double transition(const ImMat& src1, const ImMat& src2, ImMat& dst, float progress, int segments) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src1, const VkMat& src2, VkMat& dst, float progress, int segments) const;
};
} // namespace ImGui 