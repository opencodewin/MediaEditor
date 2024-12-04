#pragma once
#include "imvk_platform.h"
#include "imvk_gpu.h"

#include <vulkan/vulkan.h>

namespace ImGui 
{
class Option;
class PipelinePrivate;
class ShaderInfo;
class VKSHADER_API Pipeline
{
public:
    explicit Pipeline(const VulkanDevice* vkdev);
    virtual ~Pipeline();

public:
    void set_optimal_local_size_xyz(int w = 4, int h = 4, int c = 4);
    void set_optimal_local_size_xyz(const ImMat& local_size_xyz);
    void set_local_size_xyz(int w, int h, int c);

    int create(const uint32_t* spv_data, size_t spv_data_size, const std::vector<vk_specialization_type>& specializations);
    int create(const std::vector<uint32_t>& spv, const std::vector<vk_specialization_type>& specializations);

public:
    VkShaderModule shader_module() const;
    VkDescriptorSetLayout descriptorset_layout() const;
    VkPipelineLayout pipeline_layout() const;
    VkPipeline pipeline() const;
    VkDescriptorUpdateTemplateKHR descriptor_update_template() const;

    const ShaderInfo& shader_info() const;

    uint32_t local_size_x() const;
    uint32_t local_size_y() const;
    uint32_t local_size_z() const;

protected:
    void set_shader_module(VkShaderModule shader_module);
    void set_descriptorset_layout(VkDescriptorSetLayout descriptorset_layout);
    void set_pipeline_layout(VkPipelineLayout pipeline_layout);
    void set_pipeline(VkPipeline pipeline);
    void set_descriptor_update_template(VkDescriptorUpdateTemplateKHR descriptor_update_template);

    void set_shader_info(const ShaderInfo& shader_info);

public:
    const VulkanDevice* vkdev;

private:
    Pipeline(const Pipeline&);
    Pipeline& operator=(const Pipeline&);

private:
    PipelinePrivate* const d;
};

} // namespace ImGui

