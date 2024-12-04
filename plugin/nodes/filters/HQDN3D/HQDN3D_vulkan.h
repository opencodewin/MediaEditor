#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui
{
class VKSHADER_API HQDN3D_vulkan
{
public:
    HQDN3D_vulkan(int width, int height, int channels, int gpu = 0);
    ~HQDN3D_vulkan();
    void SetParam(float lum_spac, float chrom_spac, float lum_tmp, float chrom_tmp);
    
    double filter(const ImMat& src, ImMat& dst);

private:
    VulkanDevice* vkdev {nullptr};
    Option opt;
    Pipeline* pipe {nullptr};
    VkCompute * cmd {nullptr};

public:
    int in_width {0};
    int in_height {0};
    int in_channels {0};

private:
    float strength[4] {0, 0, 0, 0};
    ImMat coef_cpu[4];
    ImMat frame_spatial_cpu;
    ImMat frame_temporal_cpu;
    mutable Mutex param_lock;
    VkMat coefs[4];
    VkMat frame_spatial;
    VkMat frame_temporal;
private:
    void precalc_coefs(float dist25, int coef_index);
    void prealloc_frames(void);
    void upload_param(const VkMat& src, VkMat& dst);
};
} // namespace ImGui