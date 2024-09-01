#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API OpacityFilter_vulkan
{
public:
    OpacityFilter_vulkan(int gpuIdx = -1);
    ~OpacityFilter_vulkan();

    ImMat maskOpacity(const ImMat& src, const ImMat& mask, float opacity, bool inverse, bool inplace) const;

private:
    const VulkanDevice* m_pVkDev{nullptr};
    Pipeline* m_pPipeMaskOpacity{nullptr};
    Pipeline* m_pPipeMaskOpacityInplace{nullptr};
    VkCompute* m_pVkCpt{nullptr};
    Option m_tShdOpt;
};
}