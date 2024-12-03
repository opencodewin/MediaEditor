#include "imvk_pipeline.h"
#include "imvk_pipelinecache.h"
#include "imvk_option.h"

#include <math.h>

namespace ImGui 
{
class PipelinePrivate
{
public:
    VkShaderModule shader_module;
    VkDescriptorSetLayout descriptorset_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkDescriptorUpdateTemplateKHR descriptor_update_template;

    ShaderInfo shader_info;

    uint32_t local_size_x;
    uint32_t local_size_y;
    uint32_t local_size_z;
};

Pipeline::Pipeline(const VulkanDevice* _vkdev)
    : vkdev(_vkdev), d(new PipelinePrivate)
{
    d->shader_module = 0;
    d->descriptorset_layout = 0;
    d->pipeline_layout = 0;
    d->pipeline = 0;
    d->descriptor_update_template = 0;

    d->local_size_x = 16;
    d->local_size_y = 16;
    d->local_size_z = 1;
}

Pipeline::~Pipeline()
{
    delete d;
}

Pipeline::Pipeline(const Pipeline&)
    : d(0)
{
}

Pipeline& Pipeline::operator=(const Pipeline&)
{
    return *this;
}

void Pipeline::set_optimal_local_size_xyz(int w, int h, int c)
{
    set_optimal_local_size_xyz(ImMat(w, h, c));
}

void Pipeline::set_optimal_local_size_xyz(const ImMat& local_size_xyz)
{
    int w = local_size_xyz.w;
    int h = local_size_xyz.h;
    int c = local_size_xyz.c;

    if (w == 0 && h == 0 && c == 0)
    {
        // fallback to the common and safe 4x4x4
        w = 4;
        h = 4;
        c = 4;
    }

    w = std::min(w, (int)vkdev->info.max_workgroup_size_x());
    h = std::min(h, (int)vkdev->info.max_workgroup_size_y());
    c = std::min(c, (int)vkdev->info.max_workgroup_size_z());

    if (w * h * c <= (int)vkdev->info.max_workgroup_invocations())
    {
        return set_local_size_xyz(w, h, c);
    }

    int max_local_size_xy = (int)vkdev->info.max_workgroup_invocations() / c;

    int wh_max = std::max(1, (int)sqrt(max_local_size_xy));
    while (w * h >= wh_max)
    {
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    }

    set_local_size_xyz(w, h, c);
}

void Pipeline::set_local_size_xyz(int w, int h, int c)
{
    d->local_size_x = w;
    d->local_size_y = h;
    d->local_size_z = c;
}

int Pipeline::create(const uint32_t* spv_data, size_t spv_data_size, const std::vector<vk_specialization_type>& specializations)
{
    const PipelineCache* pipeline_cache = vkdev->get_pipeline_cache();

    // get from pipeline cache
    return pipeline_cache->get_pipeline(spv_data, spv_data_size, specializations, d->local_size_x, d->local_size_y, d->local_size_z,
                                        &d->shader_module, &d->descriptorset_layout, &d->pipeline_layout, &d->pipeline, &d->descriptor_update_template,
                                        d->shader_info);
}

int Pipeline::create(const std::vector<uint32_t>& spv, const std::vector<vk_specialization_type>& specializations)
{
    const uint32_t* spv_data = spv.data();
    size_t spv_data_size = spv.size() * 4;
    return create(spv_data, spv_data_size, specializations);
}

VkShaderModule Pipeline::shader_module() const
{
    return d->shader_module;
}

VkDescriptorSetLayout Pipeline::descriptorset_layout() const
{
    return d->descriptorset_layout;
}

VkPipelineLayout Pipeline::pipeline_layout() const
{
    return d->pipeline_layout;
}

VkPipeline Pipeline::pipeline() const
{
    return d->pipeline;
}

VkDescriptorUpdateTemplateKHR Pipeline::descriptor_update_template() const
{
    return d->descriptor_update_template;
}

const ShaderInfo& Pipeline::shader_info() const
{
    return d->shader_info;
}

uint32_t Pipeline::local_size_x() const
{
    return d->local_size_x;
}

uint32_t Pipeline::local_size_y() const
{
    return d->local_size_y;
}

uint32_t Pipeline::local_size_z() const
{
    return d->local_size_z;
}

void Pipeline::set_shader_module(VkShaderModule shader_module)
{
    d->shader_module = shader_module;
}

void Pipeline::set_descriptorset_layout(VkDescriptorSetLayout descriptorset_layout)
{
    d->descriptorset_layout = descriptorset_layout;
}

void Pipeline::set_pipeline_layout(VkPipelineLayout pipeline_layout)
{
    d->pipeline_layout = pipeline_layout;
}

void Pipeline::set_pipeline(VkPipeline pipeline)
{
    d->pipeline = pipeline;
}

void Pipeline::set_descriptor_update_template(VkDescriptorUpdateTemplateKHR descriptor_update_template)
{
    d->descriptor_update_template = descriptor_update_template;
}

void Pipeline::set_shader_info(const ShaderInfo& shader_info)
{
    d->shader_info = shader_info;
}

} // namespace ImGui
