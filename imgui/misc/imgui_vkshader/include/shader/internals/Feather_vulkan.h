#pragma once
#define BLUR_BOX
#include <imvk_gpu.h>
#include <imvk_pipeline.h>
#ifdef BLUR_BOX
#include <Box_vulkan.h>
#else
#include <Gaussian_vulkan.h>
#endif
#include <Erosion_vulkan.h>

namespace ImGui 
{
class VKSHADER_API Feather_vulkan
{
public:
    Feather_vulkan(int gpu = -1);
    ~Feather_vulkan();
    double forward(const ImMat& src, ImMat& dst, float feather);
    double forward(const VkMat& src, VkMat& dst, float feather);

public:
    #ifdef BLUR_BOX
    BoxBlur_vulkan* blur        {nullptr};
#else
    GaussianBlur_vulkan* blur   {nullptr};
#endif
    Erosion_vulkan* erosion     {nullptr};
};
} // namespace ImGui
