#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

#define CONCAT_HORIZONTAL   0
#define CONCAT_VERTICAL     1

namespace ImGui 
{
class VKSHADER_API Concat_vulkan
{
public:
    Concat_vulkan(int gpu = -1);
    ~Concat_vulkan();
    // direction = 0 means horizontal
    // direction = 1 means vertical
    double concat(const ImMat& src0, const ImMat& src1, ImMat& dst, int direction) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    std::vector<uint32_t> spirv_data;
    void upload_param(const VkMat& src0, const VkMat& src1, VkMat& dst, int direction) const;
};
} // namespace ImGui 