#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"
namespace ImGui
{
class VKSHADER_API Vector_vulkan
{
public:
    Vector_vulkan(int gpu = 0);
    ~Vector_vulkan();

    double scope(const ImGui::ImMat& src, ImGui::ImMat& dst, float intensity = 0.01);

private:
    ImGui::VulkanDevice* vkdev      {nullptr};
    ImGui::Option opt;
    ImGui::VkCompute * cmd          {nullptr};
    ImGui::Pipeline* pipe           {nullptr};
    ImGui::Pipeline* pipe_zero      {nullptr};
    ImGui::Pipeline* pipe_merge     {nullptr};

private:
    int size {512};

private:
    void upload_param(const ImGui::VkMat& src, ImGui::VkMat& dst, float intensity);
};
} // namespace ImGui
