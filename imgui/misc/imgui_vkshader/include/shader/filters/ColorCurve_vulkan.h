#pragma once
#include <imvk_gpu.h>
#include <imvk_pipeline.h>
#include <immat.h>

namespace ImGui 
{
class VKSHADER_API ColorCurve_vulkan
{
public:
    ColorCurve_vulkan(int gpu = -1);
    ~ColorCurve_vulkan();

    double filter(const ImMat& src, ImMat& dst, const ImMat& curve) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, const VkMat& curve) const;
};
} // namespace ImGui 