#pragma once
#include "imvk_platform.h"

namespace ImGui 
{
class VkAllocator;
class PipelineCache;
class Allocator;
class VKSHADER_API Option
{
public:
    // default option
    Option();

public:
    // blob memory allocator
    Allocator* blob_allocator;

    // workspace memory allocator
    Allocator* workspace_allocator;

    // blob memory allocator
    VkAllocator* blob_vkallocator;

    // workspace memory allocator
    VkAllocator* workspace_vkallocator;

    // staging memory allocator
    VkAllocator* staging_vkallocator;

    // pipeline cache
    PipelineCache* pipeline_cache;

    // enable bf16 data type for storage
    // improve most operator performance on all arm devices, may consume more memory
    bool use_bf16_storage;

    // enable options for gpu inference
    bool use_fp16_packed;
    bool use_fp16_storage;
    bool use_fp16_arithmetic;
    bool use_int8_packed;
    bool use_int8_storage;
    bool use_int8_arithmetic;

    // enable simd-friendly packed memory layout
    // improve all operator performance on all arm devices, will consume more memory
    // changes should be applied before loading network structure and weight
    // enabled by default
    bool use_packing_layout;

    bool use_shader_pack8;

    // subgroup option
    bool use_subgroup_basic;
    bool use_subgroup_vote;
    bool use_subgroup_ballot;
    bool use_subgroup_shuffle;

    // turn on for adreno
    bool use_image_storage;
    bool use_tensor_storage;

    bool use_reserved_0;

    bool use_local_pool_allocator;

    // enable local memory optimization for gpu inference
    bool use_shader_local_memory;

    // enable cooperative matrix optimization for gpu inference
    bool use_cooperative_matrix;

    // enable options for shared variables in gpu shader
    bool use_fp16_uniform;
    bool use_int8_uniform;

    bool use_reserved_5;
    bool use_reserved_6;
    bool use_reserved_7;
    bool use_reserved_8;
    bool use_reserved_9;
    bool use_reserved_10;
    bool use_reserved_11;
};

} // namespace ImGui

