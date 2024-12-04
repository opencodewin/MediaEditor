#include "Laplacian.h"
#include "ImVulkanShader.h"

namespace ImGui
{
Laplacian_vulkan::Laplacian_vulkan(int gpu)
    : Filter2D_vulkan(gpu)
{
    prepare_kernel();
}

Laplacian_vulkan::~Laplacian_vulkan()
{
}

void Laplacian_vulkan::prepare_kernel()
{
    kernel.create(3, 3, size_t(4u), 1);
    float side_power = (float)Strength / 4.0f;
    kernel.at<float>(1, 1) = -(float)Strength;
    kernel.at<float>(0, 1) =
    kernel.at<float>(1, 0) = 
    kernel.at<float>(1, 2) = 
    kernel.at<float>(2, 1) = side_power;
    VkTransfer tran(vkdev);
    tran.record_upload(kernel, vk_kernel, opt, false);
    tran.submit_and_wait();
    xksize = yksize = 3;
    xanchor = yanchor = 1;
}

void Laplacian_vulkan::SetParam(int _Strength)
{
    if (Strength != _Strength)
    {
        Strength = _Strength;
        prepare_kernel();
    }
}
} // namespace ImGui
