#include "imvk_Packing_vulkan.h"
#include "imvk_Packing_shader.h"
#include "imvk_command.h"

namespace ImGui 
{
Packing_vulkan::Packing_vulkan()
{
    // support_vulkan = true;
    // support_image_storage = true;
    pipeline_packing = 0;
    pipeline_packing_pack4 = 0;
    pipeline_packing_pack8 = 0;
    pipeline_packing_pack1to4 = 0;
    pipeline_packing_pack4to1 = 0;
    pipeline_packing_pack1to8 = 0;
    pipeline_packing_pack4to8 = 0;
    pipeline_packing_pack8to4 = 0;
    pipeline_packing_pack8to1 = 0;
}

int Packing_vulkan::create_pipeline(const Option& _opt)
{
    Option opt = _opt;
    const ImMat& out_shape = top_shapes.empty() ? ImMat() : top_shapes[0];

    size_t out_elemsize = out_elempack;
    if (cast_type_to == 0)
    {
        if (opt.use_fp16_storage)
        {
            out_elemsize = out_elempack * 2u;
        }
        else if (opt.use_fp16_packed)
        {
            if (out_elempack == 8) out_elemsize = 8 * 2u;
            if (out_elempack == 4) out_elemsize = 4 * 2u;
            if (out_elempack == 1) out_elemsize = 4u;
        }
        else
        {
            out_elemsize = out_elempack * 4u;
        }
    }
    else if (cast_type_to == 1)
    {
        out_elemsize = out_elempack * 4u;
    }
    else if (cast_type_to == 2)
    {
        if (out_elempack == 8) out_elemsize = 8 * 2u;
        if (out_elempack == 4) out_elemsize = 4 * 2u;
        if (out_elempack == 1) out_elemsize = 4u;
    }
    else // if (cast_type_to == 3)
    {
        out_elemsize = out_elempack * 2u;
    }

    ImMat out_shape_packed;
    if (out_shape.dims == 1) out_shape_packed = ImMat(out_shape.w / out_elempack, (void*)0, out_elemsize, out_elempack);
    if (out_shape.dims == 2) out_shape_packed = ImMat(out_shape.w, out_shape.h / out_elempack, (void*)0, out_elemsize, out_elempack);
    if (out_shape.dims == 3) out_shape_packed = ImMat(out_shape.w, out_shape.h, out_shape.c / out_elempack, (void*)0, out_elemsize, out_elempack);

    // check blob shape
    if (!vkdev->shape_support_image_storage(out_shape_packed))
    {
        //support_image_storage = false;
        opt.use_image_storage = false;
    }

    std::vector<vk_specialization_type> specializations(2 + 10);
    specializations[0].i = storage_type_from;
    specializations[1].i = storage_type_to;
    specializations[2 + 0].i = 0; // FIXME shape elempack may be dynamic
    specializations[2 + 1].i = 0;
    specializations[2 + 2].i = 0;
    specializations[2 + 3].i = 0;
    specializations[2 + 4].i = 0;
    specializations[2 + 5].i = out_shape_packed.dims;
    specializations[2 + 6].i = out_shape_packed.w;
    specializations[2 + 7].i = out_shape_packed.h;
    specializations[2 + 8].i = out_shape_packed.c;
    specializations[2 + 9].i = out_shape_packed.cstep;

    ImMat local_size_xyz; // TODO more precise group size guessed from out_shape_packed
    if (out_shape_packed.dims == 1)
    {
        local_size_xyz.w = 64;
        local_size_xyz.h = 1;
        local_size_xyz.c = 1;
    }
    if (out_shape_packed.dims == 2)
    {
        local_size_xyz.w = 8;
        local_size_xyz.h = 8;
        local_size_xyz.c = 1;
    }
    if (out_shape_packed.dims == 3)
    {
        local_size_xyz.w = 4;
        local_size_xyz.h = 4;
        local_size_xyz.c = 4;
    }

    if (out_elempack == 8)
    {
        pipeline_packing_pack8 = new Pipeline(vkdev);
        pipeline_packing_pack8->set_optimal_local_size_xyz(local_size_xyz);

        pipeline_packing_pack1to8 = new Pipeline(vkdev);
        pipeline_packing_pack1to8->set_optimal_local_size_xyz(local_size_xyz);

        pipeline_packing_pack4to8 = new Pipeline(vkdev);
        pipeline_packing_pack4to8->set_optimal_local_size_xyz(local_size_xyz);

        if (cast_type_from == cast_type_to)
        {
            if (compile_spirv_module(packing_pack8, opt, spirv_packing_pack8) == 0)
                pipeline_packing_pack8->create(spirv_packing_pack8, specializations);
            if (compile_spirv_module(packing_pack1to8, opt, spirv_packing_pack1to8) == 0)
                pipeline_packing_pack1to8->create(spirv_packing_pack1to8, specializations);
            if (compile_spirv_module(packing_pack4to8, opt, spirv_packing_pack4to8) == 0)
                pipeline_packing_pack4to8->create(spirv_packing_pack4to8, specializations);
        }
        else if (cast_type_from == 1)
        {
            if (compile_spirv_module(packing_pack8_fp32_to_fp16, opt, spirv_packing_pack8_fp32_to_fp16) == 0)
                pipeline_packing_pack8->create(spirv_packing_pack8_fp32_to_fp16, specializations);
            if (compile_spirv_module(packing_pack1to8_fp32_to_fp16, opt, spirv_packing_pack8_fp32_to_fp16) == 0)
                pipeline_packing_pack1to8->create(spirv_packing_pack1to8_fp32_to_fp16, specializations);
            if (compile_spirv_module(packing_pack4to8_fp32_to_fp16, opt, spirv_packing_pack8_fp32_to_fp16) == 0)
                pipeline_packing_pack4to8->create(spirv_packing_pack4to8_fp32_to_fp16, specializations);
        }
        else if (cast_type_to == 1)
        {
            if (compile_spirv_module(packing_pack8_fp16_to_fp32, opt, spirv_packing_pack8_fp16_to_fp32) == 0)
                pipeline_packing_pack8->create(spirv_packing_pack8_fp16_to_fp32, specializations);
            if (compile_spirv_module(packing_pack1to8_fp16_to_fp32, opt, spirv_packing_pack1to8_fp16_to_fp32) == 0)
                pipeline_packing_pack1to8->create(spirv_packing_pack1to8_fp16_to_fp32, specializations);
            if (compile_spirv_module(packing_pack4to8_fp16_to_fp32, opt, spirv_packing_pack4to8_fp16_to_fp32) == 0)
                pipeline_packing_pack4to8->create(spirv_packing_pack4to8_fp16_to_fp32, specializations);
        }
    }

    if (out_elempack == 4)
    {
        pipeline_packing_pack4 = new Pipeline(vkdev);
        pipeline_packing_pack4->set_optimal_local_size_xyz(local_size_xyz);

        pipeline_packing_pack1to4 = new Pipeline(vkdev);
        pipeline_packing_pack1to4->set_optimal_local_size_xyz(local_size_xyz);

        pipeline_packing_pack8to4 = new Pipeline(vkdev);
        pipeline_packing_pack8to4->set_optimal_local_size_xyz(local_size_xyz);

        if (cast_type_from == cast_type_to)
        {
            if (compile_spirv_module(packing_pack4, opt, spirv_packing_pack4) == 0)
                pipeline_packing_pack4->create(spirv_packing_pack4, specializations);
            if (compile_spirv_module(packing_pack1to4, opt, spirv_packing_pack1to4) == 0)
                pipeline_packing_pack1to4->create(spirv_packing_pack1to4, specializations);
            if (compile_spirv_module(packing_pack8to4, opt, spirv_packing_pack8to4) == 0)
                pipeline_packing_pack8to4->create(spirv_packing_pack8to4, specializations);
        }
        else if (cast_type_from == 1)
        {
            if (compile_spirv_module(packing_pack4_fp32_to_fp16, opt, spirv_packing_pack4_fp32_to_fp16) == 0)
                pipeline_packing_pack4->create(spirv_packing_pack4_fp32_to_fp16, specializations);
            if (compile_spirv_module(packing_pack1to4_fp32_to_fp16, opt, spirv_packing_pack1to4_fp32_to_fp16) == 0)
                pipeline_packing_pack1to4->create(spirv_packing_pack1to4_fp32_to_fp16, specializations);
            if (compile_spirv_module(packing_pack8to4_fp32_to_fp16, opt, spirv_packing_pack8to4_fp32_to_fp16) == 0)
                pipeline_packing_pack8to4->create(spirv_packing_pack8to4_fp32_to_fp16, specializations);
        }
        else if (cast_type_to == 1)
        {
            if (compile_spirv_module(packing_pack4_fp16_to_fp32, opt, spirv_packing_pack4_fp16_to_fp32) == 0)
                pipeline_packing_pack4->create(spirv_packing_pack4_fp16_to_fp32, specializations);
            if (compile_spirv_module(packing_pack1to4_fp16_to_fp32, opt, spirv_packing_pack1to4_fp16_to_fp32) == 0)
                pipeline_packing_pack1to4->create(spirv_packing_pack1to4_fp16_to_fp32, specializations);
            if (compile_spirv_module(packing_pack8to4_fp16_to_fp32, opt, spirv_packing_pack8to4_fp16_to_fp32) == 0)
                pipeline_packing_pack8to4->create(spirv_packing_pack8to4_fp16_to_fp32, specializations);
        }
    }

    if (out_elempack == 1)
    {
        pipeline_packing = new Pipeline(vkdev);
        pipeline_packing->set_optimal_local_size_xyz(local_size_xyz);

        pipeline_packing_pack4to1 = new Pipeline(vkdev);
        pipeline_packing_pack4to1->set_optimal_local_size_xyz(local_size_xyz);

        pipeline_packing_pack8to1 = new Pipeline(vkdev);
        pipeline_packing_pack8to1->set_optimal_local_size_xyz(local_size_xyz);

        if (cast_type_from == cast_type_to)
        {
            if (compile_spirv_module(packing, opt, spirv_packing) == 0)
                pipeline_packing->create(spirv_packing, specializations);
            if (compile_spirv_module(packing_pack4to1, opt, spirv_packing_pack4to1) == 0)
                pipeline_packing_pack4to1->create(spirv_packing_pack4to1, specializations);
            if (compile_spirv_module(packing_pack8to1, opt, spirv_packing_pack8to1) == 0)
                pipeline_packing_pack8to1->create(spirv_packing_pack8to1, specializations);
        }
        else if (cast_type_from == 1)
        {
            if (compile_spirv_module(packing_fp32_to_fp16, opt, spirv_packing_fp32_to_fp16) == 0)
                pipeline_packing->create(spirv_packing_fp32_to_fp16, specializations);
            if (compile_spirv_module(packing_pack4to1_fp32_to_fp16, opt, spirv_packing_pack4to1_fp32_to_fp16) == 0)
                pipeline_packing_pack4to1->create(spirv_packing_pack4to1_fp32_to_fp16, specializations);
            if (compile_spirv_module(packing_pack8to1_fp32_to_fp16, opt, spirv_packing_pack8to1_fp32_to_fp16) == 0)
                pipeline_packing_pack8to1->create(spirv_packing_pack8to1_fp32_to_fp16, specializations);
        }
        else if (cast_type_to == 1)
        {
            if (compile_spirv_module(packing_fp16_to_fp32, opt, spirv_packing_fp16_to_fp32) == 0)
                pipeline_packing->create(spirv_packing_fp16_to_fp32, specializations);
            if (compile_spirv_module(packing_pack4to1_fp16_to_fp32, opt, spirv_packing_pack4to1_fp16_to_fp32) == 0)
                pipeline_packing_pack4to1->create(spirv_packing_pack4to1_fp16_to_fp32, specializations);
            if (compile_spirv_module(packing_pack8to1_fp16_to_fp32, opt, spirv_packing_pack8to1_fp16_to_fp32) == 0)
                pipeline_packing_pack8to1->create(spirv_packing_pack8to1_fp16_to_fp32, specializations);
        }
    }

    return 0;
}

int Packing_vulkan::destroy_pipeline(const Option& /*opt*/)
{
    delete pipeline_packing;
    pipeline_packing = 0;

    delete pipeline_packing_pack4;
    pipeline_packing_pack4 = 0;

    delete pipeline_packing_pack8;
    pipeline_packing_pack8 = 0;

    delete pipeline_packing_pack1to4;
    pipeline_packing_pack1to4 = 0;

    delete pipeline_packing_pack4to1;
    pipeline_packing_pack4to1 = 0;

    delete pipeline_packing_pack1to8;
    pipeline_packing_pack1to8 = 0;

    delete pipeline_packing_pack4to8;
    pipeline_packing_pack4to8 = 0;

    delete pipeline_packing_pack8to4;
    pipeline_packing_pack8to4 = 0;

    delete pipeline_packing_pack8to1;
    pipeline_packing_pack8to1 = 0;

    return 0;
}

int Packing_vulkan::forward(const VkMat& bottom_blob, VkMat& top_blob, VkCompute& cmd, const Option& opt) const
{
    int elempack = bottom_blob.elempack;

    if (elempack == out_elempack && cast_type_from == cast_type_to && bottom_blob.allocator == opt.blob_vkallocator)
    {
        top_blob = bottom_blob;
        return 0;
    }

    int w = bottom_blob.w;
    int h = bottom_blob.h;
    int channels = bottom_blob.c;
    int dims = bottom_blob.dims;
    size_t elemsize = bottom_blob.elemsize;

    if (!use_padding)
    {
        // identity if use_padding not allowed
        if (dims == 1 && w * elempack % out_elempack != 0)
        {
            top_blob = bottom_blob;
            return 0;
        }
        if (dims == 2 && h * elempack % out_elempack != 0)
        {
            top_blob = bottom_blob;
            return 0;
        }
        if (dims == 3 && channels * elempack % out_elempack != 0)
        {
            top_blob = bottom_blob;
            return 0;
        }
    }

    size_t out_elemsize = out_elempack;
    ImDataType type = bottom_blob.type;
    if (cast_type_to == 0)
    {
        if (opt.use_fp16_storage)
        {
            out_elemsize = out_elempack * 2u;
            type = IM_DT_FLOAT16;
        }
        else if (opt.use_fp16_packed)
        {
            if (out_elempack == 8) out_elemsize = 8 * 2u;
            if (out_elempack == 4) out_elemsize = 4 * 2u;
            if (out_elempack == 1) out_elemsize = 4u;
            type = IM_DT_FLOAT16;
        }
        else
        {
            out_elemsize = out_elempack * 4u;
            type = IM_DT_FLOAT32;
        }
    }
    else if (cast_type_to == 1)
    {
        out_elemsize = out_elempack * 4u;
        type = IM_DT_FLOAT32;
    }
    else if (cast_type_to == 2)
    {
        if (out_elempack == 8) out_elemsize = 8 * 2u;
        if (out_elempack == 4) out_elemsize = 4 * 2u;
        if (out_elempack == 1) out_elemsize = 4u;
        type = IM_DT_FLOAT16;
    }
    else // if (cast_type_to == 3)
    {
        out_elemsize = out_elempack * 2u;
        type = IM_DT_FLOAT16;
    }

    if (dims == 1)
    {
        if (opt.use_fp16_storage && out_elempack == 1 && cast_type_from == cast_type_to && bottom_blob.allocator == opt.blob_vkallocator)
        {
            top_blob = bottom_blob;
            top_blob.w = w * elempack;
            top_blob.cstep = w * elempack;
            top_blob.elemsize = elemsize / elempack;
            top_blob.elempack = out_elempack;
            return 0;
        }

        int outw = (w * elempack + out_elempack - 1) / out_elempack;

        top_blob.create(outw, out_elemsize, out_elempack, opt.blob_vkallocator);
        if (top_blob.empty())
            return -100;
    }

    if (dims == 2)
    {
        int outh = (h * elempack + out_elempack - 1) / out_elempack;

        top_blob.create(w, outh, out_elemsize, out_elempack, opt.blob_vkallocator);
        if (top_blob.empty())
            return -100;
    }

    if (dims == 3)
    {
        int outc = (channels * elempack + out_elempack - 1) / out_elempack;

        top_blob.create(w, h, outc, out_elemsize, out_elempack, opt.blob_vkallocator);
        if (top_blob.empty())
            return -100;
    }

    top_blob.type = type;
    top_blob.color_space = bottom_blob.color_space;
    top_blob.color_format = bottom_blob.color_format;
    top_blob.color_range = bottom_blob.color_range;

    std::vector<VkMat> buffer_bindings(2);
    buffer_bindings[0] = bottom_blob;
    buffer_bindings[1] = top_blob;

    std::vector<VkImageMat> image_bindings(2);
    if (!opt.use_image_storage)
    {
        image_bindings.clear();
    }

    std::vector<vk_constant_type> constants(10);
    constants[0].i = bottom_blob.dims;
    constants[1].i = bottom_blob.w;
    constants[2].i = bottom_blob.h;
    constants[3].i = bottom_blob.c;
    constants[4].i = bottom_blob.cstep;
    constants[5].i = top_blob.dims;
    constants[6].i = top_blob.w;
    constants[7].i = top_blob.h;
    constants[8].i = top_blob.c;
    constants[9].i = top_blob.cstep;

    if (elempack == 1 && out_elempack == 1)
    {
        cmd.record_pipeline(pipeline_packing, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 4 && out_elempack == 4)
    {
        cmd.record_pipeline(pipeline_packing_pack4, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 1 && out_elempack == 4)
    {
        cmd.record_pipeline(pipeline_packing_pack1to4, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 4 && out_elempack == 1)
    {
        cmd.record_pipeline(pipeline_packing_pack4to1, buffer_bindings, image_bindings, constants, bottom_blob);
    }
    if (elempack == 8 && out_elempack == 8)
    {
        cmd.record_pipeline(pipeline_packing_pack8, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 1 && out_elempack == 8)
    {
        cmd.record_pipeline(pipeline_packing_pack1to8, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 4 && out_elempack == 8)
    {
        cmd.record_pipeline(pipeline_packing_pack4to8, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 8 && out_elempack == 4)
    {
        cmd.record_pipeline(pipeline_packing_pack8to4, buffer_bindings, image_bindings, constants, bottom_blob);
    }
    if (elempack == 8 && out_elempack == 1)
    {
        cmd.record_pipeline(pipeline_packing_pack8to1, buffer_bindings, image_bindings, constants, bottom_blob);
    }

    return 0;
}

int Packing_vulkan::forward(const VkImageMat& bottom_blob, VkImageMat& top_blob, VkCompute& cmd, const Option& opt) const
{
    int elempack = bottom_blob.elempack;

    if (elempack == out_elempack && cast_type_from == cast_type_to && bottom_blob.allocator == opt.blob_vkallocator)
    {
        top_blob = bottom_blob;
        return 0;
    }

    int w = bottom_blob.w;
    int h = bottom_blob.h;
    int channels = bottom_blob.c;
    int dims = bottom_blob.dims;

    if (!use_padding)
    {
        // identity if use_padding not allowed
        if (dims == 1 && w * elempack % out_elempack != 0)
        {
            top_blob = bottom_blob;
            return 0;
        }
        if (dims == 2 && h * elempack % out_elempack != 0)
        {
            top_blob = bottom_blob;
            return 0;
        }
        if (dims == 3 && channels * elempack % out_elempack != 0)
        {
            top_blob = bottom_blob;
            return 0;
        }
    }

    size_t out_elemsize = out_elempack;
    ImDataType type = bottom_blob.type;
    if (cast_type_to == 0)
    {
        if (opt.use_fp16_storage)
        {
            out_elemsize = out_elempack * 2u;
            type = IM_DT_FLOAT16;
        }
        else if (opt.use_fp16_packed)
        {
            if (out_elempack == 8) out_elemsize = 8 * 2u;
            if (out_elempack == 4) out_elemsize = 4 * 2u;
            if (out_elempack == 1) out_elemsize = 4u;
            type = IM_DT_FLOAT16;
        }
        else
        {
            out_elemsize = out_elempack * 4u;
            type = IM_DT_FLOAT32;
        }
    }
    else if (cast_type_to == 1)
    {
        out_elemsize = out_elempack * 4u;
        type = IM_DT_FLOAT32;
    }
    else if (cast_type_to == 2)
    {
        if (out_elempack == 8) out_elemsize = 8 * 2u;
        if (out_elempack == 4) out_elemsize = 4 * 2u;
        if (out_elempack == 1) out_elemsize = 4u;
        type = IM_DT_FLOAT16;
    }
    else // if (cast_type_to == 3)
    {
        out_elemsize = out_elempack * 2u;
        type = IM_DT_FLOAT16;
    }

    if (dims == 1)
    {
        int outw = (w * elempack + out_elempack - 1) / out_elempack;

        top_blob.create(outw, out_elemsize, out_elempack, opt.blob_vkallocator);
        if (top_blob.empty())
            return -100;
    }

    if (dims == 2)
    {
        int outh = (h * elempack + out_elempack - 1) / out_elempack;

        top_blob.create(w, outh, out_elemsize, out_elempack, opt.blob_vkallocator);
        if (top_blob.empty())
            return -100;
    }

    if (dims == 3)
    {
        int outc = (channels * elempack + out_elempack - 1) / out_elempack;

        top_blob.create(w, h, outc, out_elemsize, out_elempack, opt.blob_vkallocator);
        if (top_blob.empty())
            return -100;
    }

    top_blob.type = type;
    top_blob.color_space = bottom_blob.color_space;
    top_blob.color_format = bottom_blob.color_format;
    top_blob.color_range = bottom_blob.color_range;

    std::vector<VkMat> buffer_bindings(2);

    std::vector<VkImageMat> image_bindings(2);
    image_bindings[0] = bottom_blob;
    image_bindings[1] = top_blob;

    std::vector<vk_constant_type> constants(10);
    constants[0].i = bottom_blob.dims;
    constants[1].i = bottom_blob.w;
    constants[2].i = bottom_blob.h;
    constants[3].i = bottom_blob.c;
    constants[4].i = 0; //bottom_blob.cstep;
    constants[5].i = top_blob.dims;
    constants[6].i = top_blob.w;
    constants[7].i = top_blob.h;
    constants[8].i = top_blob.c;
    constants[9].i = 0; //top_blob.cstep;

    if (elempack == 1 && out_elempack == 1)
    {
        cmd.record_pipeline(pipeline_packing, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 4 && out_elempack == 4)
    {
        cmd.record_pipeline(pipeline_packing_pack4, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 1 && out_elempack == 4)
    {
        cmd.record_pipeline(pipeline_packing_pack1to4, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 4 && out_elempack == 1)
    {
        cmd.record_pipeline(pipeline_packing_pack4to1, buffer_bindings, image_bindings, constants, bottom_blob);
    }
    if (elempack == 8 && out_elempack == 8)
    {
        cmd.record_pipeline(pipeline_packing_pack8, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 1 && out_elempack == 8)
    {
        cmd.record_pipeline(pipeline_packing_pack1to8, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 4 && out_elempack == 8)
    {
        cmd.record_pipeline(pipeline_packing_pack4to8, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 8 && out_elempack == 4)
    {
        cmd.record_pipeline(pipeline_packing_pack8to4, buffer_bindings, image_bindings, constants, bottom_blob);
    }
    if (elempack == 8 && out_elempack == 1)
    {
        cmd.record_pipeline(pipeline_packing_pack8to1, buffer_bindings, image_bindings, constants, bottom_blob);
    }

    return 0;
}

int Packing_vulkan::forward(const VkMat& bottom_blob, VkImageMat& top_blob, VkCompute& cmd, const Option& opt) const
{
    int elempack = bottom_blob.elempack;

    int w = bottom_blob.w;
    int h = bottom_blob.h;
    int channels = bottom_blob.c;
    int dims = bottom_blob.dims;

    size_t out_elemsize = out_elempack;
    ImDataType type = bottom_blob.type;
    if (cast_type_to == 0)
    {
        if (opt.use_fp16_storage)
        {
            out_elemsize = out_elempack * 2u;
            type = IM_DT_FLOAT16;
        }
        else if (opt.use_fp16_packed)
        {
            if (out_elempack == 8) out_elemsize = 8 * 2u;
            if (out_elempack == 4) out_elemsize = 4 * 2u;
            if (out_elempack == 1) out_elemsize = 4u;
            type = IM_DT_FLOAT16;
        }
        else
        {
            out_elemsize = out_elempack * 4u;
            type = IM_DT_FLOAT32;
        }
    }
    else if (cast_type_to == 1)
    {
        out_elemsize = out_elempack * 4u;
        type = IM_DT_FLOAT32;
    }
    else if (cast_type_to == 2)
    {
        if (out_elempack == 8) out_elemsize = 8 * 2u;
        if (out_elempack == 4) out_elemsize = 4 * 2u;
        if (out_elempack == 1) out_elemsize = 4u;
        type = IM_DT_FLOAT16;
    }
    else // if (cast_type_to == 3)
    {
        out_elemsize = out_elempack * 2u;
        type = IM_DT_FLOAT16;
    }

    if (dims == 1)
    {
        int outw = (w * elempack + out_elempack - 1) / out_elempack;

        top_blob.create(outw, out_elemsize, out_elempack, opt.blob_vkallocator);
        if (top_blob.empty())
            return -100;
    }

    if (dims == 2)
    {
        int outh = (h * elempack + out_elempack - 1) / out_elempack;

        top_blob.create(w, outh, out_elemsize, out_elempack, opt.blob_vkallocator);
        if (top_blob.empty())
            return -100;
    }

    if (dims == 3)
    {
        int outc = (channels * elempack + out_elempack - 1) / out_elempack;

        top_blob.create(w, h, outc, out_elemsize, out_elempack, opt.blob_vkallocator);
        if (top_blob.empty())
            return -100;
    }

    top_blob.type = type;
    top_blob.color_space = bottom_blob.color_space;
    top_blob.color_format = bottom_blob.color_format;
    top_blob.color_range = bottom_blob.color_range;

    std::vector<VkMat> buffer_bindings(2);
    buffer_bindings[0] = bottom_blob;

    std::vector<VkImageMat> image_bindings(2);
    image_bindings[1] = top_blob;

    std::vector<vk_constant_type> constants(10);
    constants[0].i = bottom_blob.dims;
    constants[1].i = bottom_blob.w;
    constants[2].i = bottom_blob.h;
    constants[3].i = bottom_blob.c;
    constants[4].i = bottom_blob.cstep;
    constants[5].i = top_blob.dims;
    constants[6].i = top_blob.w;
    constants[7].i = top_blob.h;
    constants[8].i = top_blob.c;
    constants[9].i = 0; //top_blob.cstep;

    if (elempack == 1 && out_elempack == 1)
    {
        cmd.record_pipeline(pipeline_packing, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 4 && out_elempack == 4)
    {
        cmd.record_pipeline(pipeline_packing_pack4, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 1 && out_elempack == 4)
    {
        cmd.record_pipeline(pipeline_packing_pack1to4, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 4 && out_elempack == 1)
    {
        cmd.record_pipeline(pipeline_packing_pack4to1, buffer_bindings, image_bindings, constants, bottom_blob);
    }
    if (elempack == 8 && out_elempack == 8)
    {
        cmd.record_pipeline(pipeline_packing_pack8, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 1 && out_elempack == 8)
    {
        cmd.record_pipeline(pipeline_packing_pack1to8, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 4 && out_elempack == 8)
    {
        cmd.record_pipeline(pipeline_packing_pack4to8, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 8 && out_elempack == 4)
    {
        cmd.record_pipeline(pipeline_packing_pack8to4, buffer_bindings, image_bindings, constants, bottom_blob);
    }
    if (elempack == 8 && out_elempack == 1)
    {
        cmd.record_pipeline(pipeline_packing_pack8to1, buffer_bindings, image_bindings, constants, bottom_blob);
    }

    return 0;
}

int Packing_vulkan::forward(const VkImageMat& bottom_blob, VkMat& top_blob, VkCompute& cmd, const Option& opt) const
{
    int elempack = bottom_blob.elempack;

    int w = bottom_blob.w;
    int h = bottom_blob.h;
    int channels = bottom_blob.c;
    int dims = bottom_blob.dims;

    size_t out_elemsize = out_elempack;
    ImDataType type = bottom_blob.type;
    if (cast_type_to == 0)
    {
        if (opt.use_fp16_storage)
        {
            out_elemsize = out_elempack * 2u;
            type = IM_DT_FLOAT16;
        }
        else if (opt.use_fp16_packed)
        {
            if (out_elempack == 8) out_elemsize = 8 * 2u;
            if (out_elempack == 4) out_elemsize = 4 * 2u;
            if (out_elempack == 1) out_elemsize = 4u;
            type = IM_DT_FLOAT16;
        }
        else
        {
            out_elemsize = out_elempack * 4u;
            type = IM_DT_FLOAT32;
        }
    }
    else if (cast_type_to == 1)
    {
        out_elemsize = out_elempack * 4u;
        type = IM_DT_FLOAT32;
    }
    else if (cast_type_to == 2)
    {
        if (out_elempack == 8) out_elemsize = 8 * 2u;
        if (out_elempack == 4) out_elemsize = 4 * 2u;
        if (out_elempack == 1) out_elemsize = 4u;
        type = IM_DT_FLOAT16;
    }
    else // if (cast_type_to == 3)
    {
        out_elemsize = out_elempack * 2u;
        type = IM_DT_FLOAT16;
    }

    if (dims == 1)
    {
        int outw = (w * elempack + out_elempack - 1) / out_elempack;

        top_blob.create(outw, out_elemsize, out_elempack, opt.blob_vkallocator);
        if (top_blob.empty())
            return -100;
    }

    if (dims == 2)
    {
        int outh = (h * elempack + out_elempack - 1) / out_elempack;

        top_blob.create(w, outh, out_elemsize, out_elempack, opt.blob_vkallocator);
        if (top_blob.empty())
            return -100;
    }

    if (dims == 3)
    {
        int outc = (channels * elempack + out_elempack - 1) / out_elempack;

        top_blob.create(w, h, outc, out_elemsize, out_elempack, opt.blob_vkallocator);
        if (top_blob.empty())
            return -100;
    }

    top_blob.type = type;
    top_blob.color_space = bottom_blob.color_space;
    top_blob.color_format = bottom_blob.color_format;
    top_blob.color_range = bottom_blob.color_range;

    std::vector<VkMat> buffer_bindings(2);
    buffer_bindings[1] = top_blob;

    std::vector<VkImageMat> image_bindings(2);
    image_bindings[0] = bottom_blob;

    std::vector<vk_constant_type> constants(10);
    constants[0].i = bottom_blob.dims;
    constants[1].i = bottom_blob.w;
    constants[2].i = bottom_blob.h;
    constants[3].i = bottom_blob.c;
    constants[4].i = 0; //bottom_blob.cstep;
    constants[5].i = top_blob.dims;
    constants[6].i = top_blob.w;
    constants[7].i = top_blob.h;
    constants[8].i = top_blob.c;
    constants[9].i = top_blob.cstep;

    if (elempack == 1 && out_elempack == 1)
    {
        cmd.record_pipeline(pipeline_packing, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 4 && out_elempack == 4)
    {
        cmd.record_pipeline(pipeline_packing_pack4, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 1 && out_elempack == 4)
    {
        cmd.record_pipeline(pipeline_packing_pack1to4, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 4 && out_elempack == 1)
    {
        cmd.record_pipeline(pipeline_packing_pack4to1, buffer_bindings, image_bindings, constants, bottom_blob);
    }
    if (elempack == 8 && out_elempack == 8)
    {
        cmd.record_pipeline(pipeline_packing_pack8, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 1 && out_elempack == 8)
    {
        cmd.record_pipeline(pipeline_packing_pack1to8, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 4 && out_elempack == 8)
    {
        cmd.record_pipeline(pipeline_packing_pack4to8, buffer_bindings, image_bindings, constants, top_blob);
    }
    if (elempack == 8 && out_elempack == 4)
    {
        cmd.record_pipeline(pipeline_packing_pack8to4, buffer_bindings, image_bindings, constants, bottom_blob);
    }
    if (elempack == 8 && out_elempack == 1)
    {
        cmd.record_pipeline(pipeline_packing_pack8to1, buffer_bindings, image_bindings, constants, bottom_blob);
    }

    return 0;
}
} //namespace ImGui 
