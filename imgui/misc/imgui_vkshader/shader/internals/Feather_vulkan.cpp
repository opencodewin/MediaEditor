#include "Feather_vulkan.h"

namespace ImGui 
{
Feather_vulkan::Feather_vulkan(int gpu)
{
#ifdef BLUR_BOX
    blur = new BoxBlur_vulkan(gpu);
#else
    blur = new ImGui::GaussianBlur_vulkan(gpu);
#endif
    erosion = new ImGui::Erosion_vulkan(gpu);
}

Feather_vulkan::~Feather_vulkan()
{
    if (blur) { delete blur; blur = nullptr; }
    if (erosion) { delete erosion; erosion = nullptr; }
}

double Feather_vulkan::forward(const ImMat& src, ImMat& dst, float feather)
{
    double ret = 0.0;
    if (!blur || !erosion)
        return ret;

    VkMat vk_erosion;
    ret += erosion->filter(src, vk_erosion, feather);
#ifdef BLUR_BOX
    blur->SetParam(feather, feather);
#else
    blur->SetParam(feather, 0);
#endif
    ret += blur->filter(vk_erosion, dst);

    return ret;
}

double Feather_vulkan::forward(const VkMat& src, VkMat& dst, float feather)
{
    double ret = 0.0;
    if (!blur || !erosion)
        return ret;

    VkMat vk_erosion;
    ret += erosion->filter(src, vk_erosion, feather);
#ifdef BLUR_BOX
    blur->SetParam(feather, feather);
#else
    blur->SetParam(feather, 0);
#endif
    ret += blur->filter(vk_erosion, dst);

    return ret;
}

} // namespace ImGui 
