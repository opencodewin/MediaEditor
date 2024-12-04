#include "Gaussian_vulkan.h"
#include "ImVulkanShader.h"

namespace ImGui
{
GaussianBlur_vulkan::GaussianBlur_vulkan(int gpu)
    : Filter2DS_vulkan(gpu)
{
    prepare_kernel();
}

GaussianBlur_vulkan::~GaussianBlur_vulkan()
{
}

void GaussianBlur_vulkan::prepare_kernel()
{
    int ksize = blurRadius * 2 + 1;
    float _sigma = sigma;
    if (sigma <= 0.0f) 
    {
        _sigma = ((ksize - 1) * 0.5 - 1) * 0.3 + 0.8;
    }
    double scale = 1.0f / (_sigma * _sigma * 2.0);
    double sum = 0.0;

#if 1
    kernel.create(ksize, size_t(4u), 1);
    for (int i = 0; i < ksize; i++) 
    {
        int x = i - (ksize - 1) / 2;
        kernel.at<float>(i) = exp(-scale * (x * x));
        sum += kernel.at<float>(i);
    }
#else
    double cons = scale / M_PI;
    kernel.create(ksize, ksize, size_t(4u), 1);
    for (int i = 0; i < ksize; i++)
    {
        for (int j = 0; j < ksize; j++)
        {
            int x = i - (ksize - 1) / 2;
            int y = j - (ksize - 1) / 2;
            kernel.at<float>(i, j) = cons * exp(-scale * (x * x + y * y));
            sum += kernel.at<float>(i, j);
        }
    }
#endif
    sum = 1.0 / sum;
    kernel *= (float)(sum);
    VkTransfer tran(vkdev);
    tran.record_upload(kernel, vk_kernel, opt, false);
    tran.submit_and_wait();

    xksize = yksize = ksize;
    xanchor = yanchor = blurRadius;
}

void GaussianBlur_vulkan::SetParam(int _blurRadius, float _sigma)
{
    if (blurRadius != _blurRadius || sigma != _sigma)
    {
        blurRadius = _blurRadius;
        sigma = _sigma;
        prepare_kernel();
    }
}
} // namespace ImGui

