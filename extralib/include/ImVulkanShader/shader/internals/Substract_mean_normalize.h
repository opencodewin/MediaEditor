#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include <vector>

namespace ImGui 
{
class VKSHADER_API Substract_Mean_Normalize_vulkan
{
public:
    Substract_Mean_Normalize_vulkan(int gpu = -1);
    ~Substract_Mean_Normalize_vulkan();

    double forward(const ImMat& bottom_blob, ImMat& top_blob, std::vector<float> mean_vals, std::vector<float> norm_vals);

public:
    const VulkanDevice* vkdev   {nullptr};
    Pipeline* pipe              {nullptr};
    VkCompute * cmd             {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, std::vector<float> mean_vals, std::vector<float> norm_vals);
};
} // namespace ImGui
