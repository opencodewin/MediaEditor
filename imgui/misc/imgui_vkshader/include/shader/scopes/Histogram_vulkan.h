#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui
{
class VKSHADER_API Histogram_vulkan
{
public:
    Histogram_vulkan(int gpu = 0);
    ~Histogram_vulkan();

    double scope(const ImGui::ImMat& src, ImGui::ImMat& dst, int level = 256, float scale = 1.0, bool log_view = false);

private:
    ImGui::VulkanDevice* vkdev      {nullptr};
    ImGui::Option opt;
    ImGui::VkCompute * cmd          {nullptr};
    ImGui::Pipeline* pipe           {nullptr};
    ImGui::Pipeline* pipe_zero      {nullptr};
    ImGui::Pipeline* pipe_conv      {nullptr};

private:
    void upload_param(const ImGui::VkMat& src, ImGui::VkMat& dst, float scale, bool log_view);
};
} // namespace ImGui