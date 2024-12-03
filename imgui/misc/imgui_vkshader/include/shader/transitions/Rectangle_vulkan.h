#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Rectangle_vulkan
{
public:
    Rectangle_vulkan(int gpu = -1);
    ~Rectangle_vulkan();

    double transition(const ImMat& src1, const ImMat& src2, ImMat& dst, float progress, ImPixel& bg_colour) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src1, const VkMat& src2, VkMat& dst, float progress, ImPixel& bg_colour) const;
};
} // namespace ImGui 