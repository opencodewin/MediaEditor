#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Fish2Panorama_vulkan
{
public:
    Fish2Panorama_vulkan(int gpu = 0);
    ~Fish2Panorama_vulkan();

    double filter(const ImMat& src, ImMat& dst, float fov, float cx, float cy, ImInterpolateMode mode);

private:
    VulkanDevice* vkdev {nullptr};
    Option opt;
    Pipeline* pipe {nullptr};
    VkCompute * cmd {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, float fov, float cx, float cy, ImInterpolateMode mode);
};
} // namespace ImGui