#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

#define FILTER_2DS_BLUR 0
#define CHROMAKEY_OUTPUT_NORMAL      0  // normal RGBA with masked alpha channel
#define CHROMAKEY_OUTPUT_ALPHA_ONLY  1  // Mono Channel with alpha only
#define CHROMAKEY_OUTPUT_ALPHA_RGBA  2  // RGBA output with all channels is alpha mask

namespace ImGui 
{
class VKSHADER_API ChromaKey_vulkan
{
public:
    ChromaKey_vulkan(int gpu = -1);
    ~ChromaKey_vulkan();
    // 比较差异,确定使用亮度与颜色比例,值需大于0,值越大,亮度所占比例越大
    // lumaMask  {1.0f}
    // 需要扣除的颜色
    // chromaColor {}
    // 比较差异相差的最少值(少于这值会放弃alpha)
    // alphaCutoffMin {0.2f}
    // 比较后的alpha系数增亮
    // alphaScale {10.0f}
    // 比较后的alpha指数增亮
    // alphaExponent {0.1f}

    double filter(const ImMat& src, ImMat& dst,
                float lumaMask, ImPixel chromaColor,
                float alphaCutoffMin, float alphaScale, float alphaExponent,
                int output_type);

public:
    const VulkanDevice* vkdev   {nullptr};
    Pipeline * pipe             {nullptr};
#if FILTER_2DS_BLUR
    Pipeline * pipe_blur_column {nullptr};
    Pipeline * pipe_blur_row    {nullptr};
#else
    Pipeline * pipe_blur        {nullptr};
#endif
    Pipeline * pipe_sharpen     {nullptr};
    Pipeline * pipe_despill     {nullptr};
    VkCompute * cmd             {nullptr};
    Option opt;

private:
    ImMat kernel;
    VkMat vk_kernel;
    int blurRadius      {1};
    int ksize           {3};
    float sigma         {0};
    void prepare_kernel();

private:
    
    void upload_param(const VkMat& src, VkMat& dst,
                    float lumaMask, ImPixel chromaColor, 
                    float alphaCutoffMin, float alphaScale, float alphaExponent,
                    int output_type);
};
} // namespace ImGui 