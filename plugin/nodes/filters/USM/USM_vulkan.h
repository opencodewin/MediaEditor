#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui
{
class VKSHADER_API USM_vulkan
{
public:
    USM_vulkan(int gpu = 0);
    ~USM_vulkan();

    double filter(const ImMat& src, ImMat& dst, float _sigma, float amount, float threshold);

private:
    VulkanDevice* vkdev      {nullptr};
    Option opt;
    Pipeline* pipe           {nullptr};
    Pipeline * pipe_column   {nullptr};
    Pipeline * pipe_row      {nullptr};
    VkCompute * cmd          {nullptr};

private:
    ImMat kernel;
    VkMat vk_kernel;
    int blurRadius  {2};
    int xksize;
    int yksize;
    int xanchor;
    int yanchor;
    float sigma;

private:
    void upload_param(const VkMat& src, VkMat& dst, float _sigma, float amount, float threshold);
    void prepare_kernel();
};
} // namespace ImGui
