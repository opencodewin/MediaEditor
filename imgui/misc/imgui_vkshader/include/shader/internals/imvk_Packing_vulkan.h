#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"

namespace ImGui 
{
class VKSHADER_API Packing_vulkan
{
public:
    Packing_vulkan();

    virtual int create_pipeline(const Option& opt);
    virtual int destroy_pipeline(const Option& opt);

    virtual int forward(const VkMat& bottom_blob, VkMat& top_blob, VkCompute& cmd, const Option& opt) const;
    virtual int forward(const VkImageMat& bottom_blob, VkImageMat& top_blob, VkCompute& cmd, const Option& opt) const;
    virtual int forward(const VkMat& bottom_blob, VkImageMat& top_blob, VkCompute& cmd, const Option& opt) const;
    virtual int forward(const VkImageMat& bottom_blob, VkMat& top_blob, VkCompute& cmd, const Option& opt) const;

public:
    Pipeline* pipeline_packing;
    Pipeline* pipeline_packing_pack4;
    Pipeline* pipeline_packing_pack8;
    Pipeline* pipeline_packing_pack1to4;
    Pipeline* pipeline_packing_pack4to1;
    Pipeline* pipeline_packing_pack1to8;
    Pipeline* pipeline_packing_pack4to8;
    Pipeline* pipeline_packing_pack8to4;
    Pipeline* pipeline_packing_pack8to1;

public:
    int out_elempack;
    int use_padding;

    // element type
    // 0 = auto
    // 1 = fp32
    // 2 = fp16p
    // 3 = fp16s
    int cast_type_from;
    int cast_type_to;

    // storage type
    // 0 = buffer
    // 1 = image
    int storage_type_from;
    int storage_type_to;

public:
    const VulkanDevice* vkdev;
    std::vector<ImMat> bottom_shapes;
    std::vector<ImMat> top_shapes;

public:
    std::vector<uint32_t> spirv_packing_fp16_to_fp32;
    std::vector<uint32_t> spirv_packing_fp32_to_fp16;
    std::vector<uint32_t> spirv_packing_pack1to4_fp16_to_fp32;
    std::vector<uint32_t> spirv_packing_pack1to4_fp32_to_fp16;
    std::vector<uint32_t> spirv_packing_pack1to4;
    std::vector<uint32_t> spirv_packing_pack1to8_fp16_to_fp32;
    std::vector<uint32_t> spirv_packing_pack1to8_fp32_to_fp16;
    std::vector<uint32_t> spirv_packing_pack1to8;
    std::vector<uint32_t> spirv_packing_pack4_fp16_to_fp32;
    std::vector<uint32_t> spirv_packing_pack4_fp32_to_fp16;
    std::vector<uint32_t> spirv_packing_pack4;
    std::vector<uint32_t> spirv_packing_pack4to1_fp16_to_fp32;
    std::vector<uint32_t> spirv_packing_pack4to1_fp32_to_fp16;
    std::vector<uint32_t> spirv_packing_pack4to1;
    std::vector<uint32_t> spirv_packing_pack4to8_fp16_to_fp32;
    std::vector<uint32_t> spirv_packing_pack4to8_fp32_to_fp16;
    std::vector<uint32_t> spirv_packing_pack4to8;
    std::vector<uint32_t> spirv_packing_pack8_fp16_to_fp32;
    std::vector<uint32_t> spirv_packing_pack8_fp32_to_fp16;
    std::vector<uint32_t> spirv_packing_pack8;
    std::vector<uint32_t> spirv_packing_pack8to1_fp16_to_fp32;
    std::vector<uint32_t> spirv_packing_pack8to1_fp32_to_fp16;
    std::vector<uint32_t> spirv_packing_pack8to1;
    std::vector<uint32_t> spirv_packing_pack8to4_fp16_to_fp32;
    std::vector<uint32_t> spirv_packing_pack8to4_fp32_to_fp16;
    std::vector<uint32_t> spirv_packing_pack8to4;
    std::vector<uint32_t> spirv_packing;
};
} // namespace ImGui 