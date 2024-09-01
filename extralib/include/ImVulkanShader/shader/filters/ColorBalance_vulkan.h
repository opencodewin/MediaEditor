#pragma once
#include <imvk_gpu.h>
#include <imvk_pipeline.h>
#include <immat.h>
#define NO_STB_IMAGE
#include <imgui.h>

namespace ImGui 
{
class VKSHADER_API ColorBalance_vulkan
{
public:
    ColorBalance_vulkan(int gpu = -1);
    ~ColorBalance_vulkan();

    double filter(const ImMat& src, ImMat& dst, ImVec4 shadows, ImVec4 midtones, ImVec4 highlights, bool preserve_lightness = false) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe           {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& src, VkMat& dst, ImVec4 shadows, ImVec4 midtones, ImVec4 highlights, bool preserve_lightness) const;
};
} // namespace ImGui 