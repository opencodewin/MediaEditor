#pragma once
#include <imvk_gpu.h>
#include <imvk_pipeline.h>
#include <Feather_vulkan.h>

enum ImBlendingMode : int32_t {
    IM_BL_NONE = 0,
    IM_BL_ADD,
    IM_BL_AVERAGE,
    IM_BL_COLOR,
    IM_BL_COLORBURN,
    IM_BL_COLORDODGE,
    IM_BL_DARKEN,
    IM_BL_DIFFERENCE,
    IM_BL_EXCLUSION,
    IM_BL_GLOW,
    IM_BL_HARDLIGHT,
    IM_BL_HARDMIX,
    IM_BL_HUE,
    IM_BL_LIGHTEN,
    IM_BL_LINEARBURN,
    IM_BL_LINEARDODGE,
    IM_BL_LINEARLIGHT,
    IM_BL_LUMINOSITY,
    IM_BL_MULTIPLY,
    IM_BL_NEGATION,
    IM_BL_OVERLAY,
    IM_BL_PHOENIX,
    IM_BL_PINLIGHT,
    IM_BL_REFLECT,
    IM_BL_SATURATION,
    IM_BL_SCREEN,
    IM_BL_SOFTLIGHT,
    IM_BL_SUBTRACT,
    IM_BL_VIVIDLIGHT,
    IM_BL_COUNT,
};

namespace ImGui 
{
class VKSHADER_API Blend_vulkan
{
public:
    Blend_vulkan(int gpu = -1);
    ~Blend_vulkan();

    double forward(const ImMat& blend, ImMat& base, ImBlendingMode mode, float opacity, ImPoint offset = ImPoint(0,0));
    double forward(const ImMat& blend, const ImMat& mask, ImMat& base, ImBlendingMode mode, float opacity, float feather, ImPoint offset = ImPoint(0,0));

public:
    const VulkanDevice* vkdev   {nullptr};
    Pipeline* pipe              {nullptr};
    Pipeline* pipe_mask_merge   {nullptr};
    VkCompute* cmd              {nullptr};
    Feather_vulkan* fea         {nullptr};
    Option opt;

private:
    void upload_param(const VkMat& blend, VkMat& base, ImBlendingMode mode, float opacity, ImPoint offset = ImPoint(0,0));
    void upload_param(const VkMat& blend, const VkMat& mask, VkMat& base, ImBlendingMode mode, float opacity, float feather, ImPoint offset = ImPoint(0,0));
};
} // namespace ImGui
