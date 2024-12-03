#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Door_vulkan
{
public:
    Door_vulkan(int gpu = -1);
    ~Door_vulkan();

    double transition(const ImMat& src1, const ImMat& src2, ImMat& dst, float progress, bool open, bool horizon) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src1, const VkMat& src2, VkMat& dst, float progress, bool open, bool horizon) const;
};
} // namespace ImGui 