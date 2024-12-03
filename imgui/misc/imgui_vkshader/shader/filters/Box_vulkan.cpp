#include "Box_vulkan.h"
#include "ImVulkanShader.h"

namespace ImGui 
{
BoxBlur_vulkan::BoxBlur_vulkan(int gpu)
    : Filter2DS_vulkan(gpu)
{
    prepare_kernel();
}

BoxBlur_vulkan::~BoxBlur_vulkan()
{
}

void BoxBlur_vulkan::prepare_kernel()
{
    xksize = xSize;
    yksize = ySize;
    xanchor = xSize / 2;
    yanchor = ySize / 2;
#if 1
    float kvulve = 2.0f / (float)(xSize + ySize);
#else
    float kvulve = 1.0f / (float)(xSize * ySize);
#endif
    kernel.create(xSize, ySize, size_t(4u), 1);
    for (int x = 0; x < xSize; x++)
    {
        for (int y = 0; y < ySize; y++)
        {
            kernel.at<float>(x, y) = kvulve;
        }

    }
    VkTransfer tran(vkdev);
    tran.record_upload(kernel, vk_kernel, opt, false);
    tran.submit_and_wait();
}

void BoxBlur_vulkan::SetParam(int _xSize, int _ySize)
{
    if (xSize != _xSize || ySize != _ySize)
    {
        xSize = _xSize;
        ySize = _ySize;
        prepare_kernel();
    }
}
} // namespace ImGui