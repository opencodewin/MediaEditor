#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API DeBand_vulkan
{
public:
    DeBand_vulkan(int width, int height, int channels, int gpu = 0);
    ~DeBand_vulkan();
    void SetParam(int _range, float _direction);
    
    double filter(const ImMat& src, ImMat& dst, float threshold, bool blur);

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
    int range {16};
    float direction {2*M_PI};
    ImMat xpos_cpu;
    ImMat ypos_cpu;
    mutable Mutex param_lock;
    VkMat xpos;
    VkMat ypos;
private:
    void precalc_pos(void);
    void upload_param(const VkMat& src, VkMat& dst, float threshold, bool blur);
};
} // namespace ImGui 