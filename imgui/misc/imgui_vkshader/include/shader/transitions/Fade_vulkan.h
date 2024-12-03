#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Fade_vulkan
{
public:
    Fade_vulkan(int gpu = -1);
    ~Fade_vulkan();

    double transition(const ImMat& src1, const ImMat& src2, ImMat& dst, float progress, int type, ImPixel& color) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src1, const VkMat& src2, VkMat& dst, float progress, int type, ImPixel& color) const;
};
} // namespace ImGui 