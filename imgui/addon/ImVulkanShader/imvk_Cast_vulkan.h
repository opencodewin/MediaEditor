#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"

namespace ImGui 
{
class VKSHADER_API Cast_vulkan
{
public:
    Cast_vulkan();

    virtual int create_pipeline(const Option& opt);
    virtual int destroy_pipeline(const Option& opt);

    virtual int forward(const ImMat& bottom_blob, ImMat& top_blob, const Option& opt) const;
    virtual int forward(const VkMat& bottom_blob, VkMat& top_blob, VkCompute& cmd, const Option& opt) const;
    virtual int forward(const VkImageMat& bottom_blob, VkImageMat& top_blob, VkCompute& cmd, const Option& opt) const;

public:
    Pipeline* pipeline_cast_fp32_to_fp16;
    Pipeline* pipeline_cast_fp32_to_fp16_pack4;
    Pipeline* pipeline_cast_fp32_to_fp16_pack8;
    Pipeline* pipeline_cast_fp16_to_fp32;
    Pipeline* pipeline_cast_fp16_to_fp32_pack4;
    Pipeline* pipeline_cast_fp16_to_fp32_pack8;

public:
    // element type
    // 0 = auto
    // 1 = float32
    // 2 = float16
    // 3 = int8
    // 4 = bfloat16
    int type_from;
    int type_to;

public:
    const VulkanDevice* vkdev;
    std::vector<ImMat> bottom_shapes;
    std::vector<ImMat> top_shapes;

public:
    std::vector<uint32_t> spirv_cast_fp16_to_fp32_pack4;
    std::vector<uint32_t> spirv_cast_fp16_to_fp32_pack8;
    std::vector<uint32_t> spirv_cast_fp16_to_fp32;
    std::vector<uint32_t> spirv_cast_fp32_to_fp16_pack4;
    std::vector<uint32_t> spirv_cast_fp32_to_fp16_pack8;
    std::vector<uint32_t> spirv_cast_fp32_to_fp16;
};
} // namespace ImGui 