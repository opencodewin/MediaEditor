#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui
{
class VKSHADER_API Waveform_vulkan
{
public:
    Waveform_vulkan(int gpu = 0);
    ~Waveform_vulkan();

    double scope(const ImGui::ImMat& src, ImGui::ImMat& dst, int level = 256, float fintensity = 0.1, bool separate = false, bool show_y = false);

private:
    ImGui::VulkanDevice* vkdev      {nullptr};
    ImGui::Option opt;
    ImGui::VkCompute * cmd          {nullptr};
    ImGui::Pipeline* pipe           {nullptr};
    ImGui::Pipeline* pipe_zero      {nullptr};
    ImGui::Pipeline* pipe_conv      {nullptr};

private:
    void upload_param(const ImGui::VkMat& src, ImGui::VkMat& dst, float fintensity, bool separate, bool show_y);
};
} // namespace ImGui