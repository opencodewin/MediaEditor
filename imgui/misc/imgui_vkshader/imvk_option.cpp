#include "imvk_option.h"

namespace ImGui 
{
Option::Option()
{
    blob_allocator = nullptr;
    workspace_allocator = nullptr;

    blob_vkallocator = nullptr;
    workspace_vkallocator = nullptr;
    staging_vkallocator = nullptr;
    pipeline_cache = nullptr;

    use_bf16_storage = false;

    use_fp16_packed = false;
    use_fp16_storage = false;
    use_fp16_arithmetic = false;
    use_int8_packed = false;
    use_int8_storage = false;
    use_int8_arithmetic = false;

    use_packing_layout = false;

    use_shader_pack8 = false;

    use_subgroup_basic = false;
    use_subgroup_vote = false;
    use_subgroup_ballot = false;
    use_subgroup_shuffle = false;

    use_image_storage = false;
    use_tensor_storage = false;

    use_reserved_0 = false;

    use_local_pool_allocator = true;

    use_shader_local_memory = true;
    use_cooperative_matrix = true;

    use_fp16_uniform = false;
    use_int8_uniform = false;
}

} // namespace ImGui
