#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Equidistance2Orthographic_vulkan
{
public:
    Equidistance2Orthographic_vulkan(int gpu = 0);
    ~Equidistance2Orthographic_vulkan();

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