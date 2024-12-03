#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Filter2D_vulkan
{
public:
    Filter2D_vulkan(int gpu = -1);
    ~Filter2D_vulkan();

    double filter(const ImMat& src, ImMat& dst) const;

public:
    ImMat kernel;
    VkMat vk_kernel;
    int xksize;
    int yksize;
    int xanchor;
    int yanchor;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    std::vector<uint32_t> spirv_data;
    void upload_param(const VkMat& src, VkMat& dst) const;
};
} // namespace ImGui 