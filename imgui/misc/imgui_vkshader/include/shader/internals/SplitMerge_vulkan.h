#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"

namespace ImGui 
{
class VKSHADER_API SplitMerge_vulkan
{
public:
    SplitMerge_vulkan(int gpu = -1);
    ~SplitMerge_vulkan();

    double split(const ImMat& src, std::vector<ImMat>& dst) const;
    double merge(const std::vector<ImMat>& src, ImMat& dst) const;

public:
    const VulkanDevice* vkdev;
    Pipeline * pipe_split = nullptr;
    Pipeline * pipe_merge = nullptr;
    VkCompute * cmd = nullptr;
    Option opt;

private:
    std::vector<uint32_t> spirv_data;
    void upload_param(const VkMat& src, std::vector<VkMat>& dst) const;
    void upload_param(const std::vector<VkMat>& src, VkMat& dst) const;
};
} // namespace ImGui 