#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "immat.h"

namespace ImGui 
{
class VKSHADER_API Crop_vulkan
{
public:
    Crop_vulkan(int gpu = -1);
    ~Crop_vulkan();

    double crop(const ImMat& src, ImMat& dst, int _x, int _y, int _w, int _h) const;
    double cropto(const ImMat& src, ImMat& dst, int _x, int _y, int _w, int _h, int _xd, int _yd) const;

public:
    const VulkanDevice* vkdev {nullptr};
    Pipeline * pipe_crop      {nullptr};
    Pipeline * pipe_cropto    {nullptr};
    VkCompute * cmd           {nullptr};
    Option opt;

private:
    std::vector<uint32_t> spirv_data;
    void upload_param(const VkMat& src, VkMat& dst, int _x, int _y, int _w, int _h) const;
    void upload_param(const VkMat& src, VkMat& dst, int _x, int _y, int _w, int _h, int _xd, int _yd, bool fill) const;
};
} // namespace ImGui 