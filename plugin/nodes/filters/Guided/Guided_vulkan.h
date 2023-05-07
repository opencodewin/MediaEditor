#pragma once
#include <imvk_gpu.h>
#include <imvk_pipeline.h>
#include <Box.h>

namespace ImGui
{
class VKSHADER_API Guided_vulkan
{
public:
    Guided_vulkan(int gpu = 0);
    ~Guided_vulkan();
    double filter(const ImMat& src, ImMat& dst, int r, float eps);

private:
    VulkanDevice* vkdev {nullptr};
    Option opt;
    Pipeline* pipe {nullptr};
    Pipeline* pipe_to_matting {nullptr};
    Pipeline* pipe_matting {nullptr};
    VkCompute * cmd {nullptr};
    BoxBlur_vulkan * box1 {nullptr};
    BoxBlur_vulkan * box2 {nullptr};
    BoxBlur_vulkan * box3 {nullptr};
    BoxBlur_vulkan * box4 {nullptr};
    BoxBlur_vulkan * box5 {nullptr};

private:
    void upload_param(const VkMat& src, VkMat& dst, int r, float eps);
};
} // namespace ImGui
