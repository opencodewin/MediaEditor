#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Brightness_vulkan
{
public:
    Brightness_vulkan(int gpu = -1);
    ~Brightness_vulkan();

    double filter(const ImMat& src, ImMat& dst, float brightness) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, float brightness) const;
};
} // namespace ImGui 