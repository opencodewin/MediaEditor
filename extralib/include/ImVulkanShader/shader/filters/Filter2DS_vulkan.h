#pragma once
#include "immat.h"
#include "imvk_gpu.h"
#include "imvk_pipeline.h"

namespace ImGui 
{
class VKSHADER_API Filter2DS_vulkan
{
public:
    Filter2DS_vulkan(int gpu = -1);
    ~Filter2DS_vulkan();

    double filter(const ImMat& src, ImMat& dst) const;

public:
    ImMat kernel;
    VkMat vk_kernel;
    int xksize;
    int yksize;
    int xanchor;
    int yanchor;

public:
    const VulkanDevice* vkdev   {nullptr};
    Pipeline * pipe_column      {nullptr};
    Pipeline * pipe_row         {nullptr};
    Pipeline * pipe_column_mono {nullptr};
    Pipeline * pipe_row_mono    {nullptr};
    VkCompute * cmd             {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst) const;
};
} // namespace ImGui 
