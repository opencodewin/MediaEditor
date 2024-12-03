#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API ColorInvert_vulkan
{
public:
    ColorInvert_vulkan(int gpu = -1);
    ~ColorInvert_vulkan();

    double filter(const ImMat& src, ImMat& dst) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst) const;
};
} // namespace ImGui 