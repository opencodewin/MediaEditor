#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API CopyTo_vulkan
{
public:
    CopyTo_vulkan(int gpu = -1);
    ~CopyTo_vulkan();

    double copyTo(const ImMat& src, ImMat& dst, int x, int y, float alpha = 1.f) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, int x, int y, float alpha) const;
};
} // namespace ImGui 