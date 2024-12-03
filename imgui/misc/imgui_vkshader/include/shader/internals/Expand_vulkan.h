#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Expand_vulkan
{
public:
    Expand_vulkan(int gpu = -1);
    ~Expand_vulkan();

    double expand(const ImMat& src, ImMat& dst, int t, int b, int l, int r) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    std::vector<uint32_t> spirv_data;
    void upload_param(const VkMat& src, VkMat& dst, int t, int b, int l, int r) const;
};
} // namespace ImGui 