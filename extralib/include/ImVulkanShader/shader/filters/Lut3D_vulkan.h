#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

typedef struct _tag_rgbvec 
{
    float r, g, b, a;
} rgbvec;

namespace ImGui 
{
class VKSHADER_API LUT3D_vulkan
{
public:
    LUT3D_vulkan(void * table, int size, float r_scale, float g_scale, float b_scale, float a_scale,
                int interpolation = IM_INTERPOLATE_TRILINEAR, int gpu = 0);
    LUT3D_vulkan(std::string lut_path, int interpolation = IM_INTERPOLATE_TRILINEAR, int gpu = 0);
    ~LUT3D_vulkan();

    double filter(const ImMat& src, ImMat& dst);

    void write_header_file(std::string filename);
    
public:
    const VulkanDevice* vkdev;
    Pipeline * pipeline_lut3d = nullptr;
    VkCompute * cmd = nullptr;
    Option opt;
    VkMat lut_gpu;

private:
    void *lut {nullptr};
    int lutsize {0};
    rgbvec scale {1.0, 1.0, 1.0, 0.0};
    int rgba_map[4];
    int interpolation_mode {IM_INTERPOLATE_TRILINEAR};
    bool from_file {false};

private:
    int init(int interpolation, int gpu);
    int allocate_3dlut(int size);
    int parse_cube(std::string lut_file);
    void upload_param(const VkMat& src, VkMat& dst);
};
} // namespace ImGui 