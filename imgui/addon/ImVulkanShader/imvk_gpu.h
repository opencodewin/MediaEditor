#pragma once
#include <immat.h>
#include "imvk_platform.h"
#include "imvk_allocator.h"
#include "imvk_option.h"
#include "imvk_mat.h"
#include "imvk_image_mat.h"

#define BINDING_MAX 32
#define SHADER_BUFFER

namespace ImGui 
{
// type for vulkan specialization constant and push constant
union vk_specialization_type
{
    int i;
    float f;
    uint32_t u32;
};
union vk_constant_type
{
    int i;
    float f;
};

// instance

// Create VkInstance and initialize some objects that need to be calculated by GPU
// Creates a VkInstance object, Checks the extended attributes supported by the Vulkan instance concerned,
// Initializes, and creates Vulkan validation layers (if ENABLE_VALIDATION_LAYER is enabled),
// Iterates over all supported physical devices, etc.
VKSHADER_API int create_gpu_instance();

// Get global VkInstance variable
// Must be called after create_gpu_instance() and before destroy_gpu_instance()
VKSHADER_API VkInstance get_gpu_instance();

// Destroy VkInstance object and free the memory of the associated object
// Usually called in the destructor of the main program exit
// The function will internally ensure that all vulkan devices are idle before proceeding with destruction.
VKSHADER_API void destroy_gpu_instance();

// vulkan core
extern PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
extern PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
extern PFN_vkAllocateMemory vkAllocateMemory;
extern PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
extern PFN_vkBindBufferMemory vkBindBufferMemory;
extern PFN_vkBindImageMemory vkBindImageMemory;
extern PFN_vkCmdBeginQuery vkCmdBeginQuery;
extern PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
extern PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
extern PFN_vkCmdBindPipeline vkCmdBindPipeline;
extern PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
extern PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
extern PFN_vkCmdCopyImage vkCmdCopyImage;
extern PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
extern PFN_vkCmdCopyQueryPoolResults vkCmdCopyQueryPoolResults;
extern PFN_vkCmdDispatch vkCmdDispatch;
extern PFN_vkCmdDispatchIndirect vkCmdDispatchIndirect;
extern PFN_vkCmdEndQuery vkCmdEndQuery;
extern PFN_vkCmdExecuteCommands vkCmdExecuteCommands;
extern PFN_vkCmdFillBuffer vkCmdFillBuffer;
extern PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
extern PFN_vkCmdPushConstants vkCmdPushConstants;
extern PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
extern PFN_vkCmdResolveImage vkCmdResolveImage;
extern PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer;
extern PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp;
extern PFN_vkCreateBuffer vkCreateBuffer;
extern PFN_vkCreateBufferView vkCreateBufferView;
extern PFN_vkCreateCommandPool vkCreateCommandPool;
extern PFN_vkCreateComputePipelines vkCreateComputePipelines;
extern PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
extern PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
extern PFN_vkCreateDevice vkCreateDevice;
extern PFN_vkCreateFence vkCreateFence;
extern PFN_vkCreateImage vkCreateImage;
extern PFN_vkCreateImageView vkCreateImageView;
extern PFN_vkCreatePipelineCache vkCreatePipelineCache;
extern PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
extern PFN_vkCreateQueryPool vkCreateQueryPool;
extern PFN_vkCreateSampler vkCreateSampler;
extern PFN_vkCreateSemaphore vkCreateSemaphore;
extern PFN_vkCreateShaderModule vkCreateShaderModule;
extern PFN_vkDestroyBuffer vkDestroyBuffer;
extern PFN_vkDestroyBufferView vkDestroyBufferView;
extern PFN_vkDestroyCommandPool vkDestroyCommandPool;
extern PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
extern PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
extern PFN_vkDestroyDevice vkDestroyDevice;
extern PFN_vkDestroyFence vkDestroyFence;
extern PFN_vkDestroyImage vkDestroyImage;
extern PFN_vkDestroyImageView vkDestroyImageView;
extern PFN_vkDestroyInstance vkDestroyInstance;
extern PFN_vkDestroyPipeline vkDestroyPipeline;
extern PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
extern PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
extern PFN_vkDestroyQueryPool vkDestroyQueryPool;
extern PFN_vkDestroySampler vkDestroySampler;
extern PFN_vkDestroySemaphore vkDestroySemaphore;
extern PFN_vkDestroyShaderModule vkDestroyShaderModule;
extern PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
extern PFN_vkEndCommandBuffer vkEndCommandBuffer;
extern PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
extern PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
extern PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
extern PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
extern PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
extern PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
extern PFN_vkFreeMemory vkFreeMemory;
extern PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
extern PFN_vkGetDeviceMemoryCommitment vkGetDeviceMemoryCommitment;
extern PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
extern PFN_vkGetDeviceQueue vkGetDeviceQueue;
extern PFN_vkGetFenceStatus vkGetFenceStatus;
extern PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
extern PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
extern PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
extern PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
extern PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
extern PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
extern PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
extern PFN_vkGetPipelineCacheData vkGetPipelineCacheData;
extern PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
extern PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
extern PFN_vkMapMemory vkMapMemory;
extern PFN_vkMergePipelineCaches vkMergePipelineCaches;
extern PFN_vkQueueSubmit vkQueueSubmit;
extern PFN_vkQueueWaitIdle vkQueueWaitIdle;
extern PFN_vkResetCommandBuffer vkResetCommandBuffer;
extern PFN_vkResetCommandPool vkResetCommandPool;
extern PFN_vkResetDescriptorPool vkResetDescriptorPool;
extern PFN_vkResetFences vkResetFences;
extern PFN_vkUnmapMemory vkUnmapMemory;
extern PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
extern PFN_vkWaitForFences vkWaitForFences;

// instance extension capability
extern int support_VK_KHR_external_memory_capabilities;
extern int support_VK_KHR_get_physical_device_properties2;
extern int support_VK_KHR_get_surface_capabilities2;
extern int support_VK_KHR_surface;
extern int support_VK_EXT_debug_utils;
extern int support_VK_EXT_validation_features;
extern int support_VK_EXT_validation_flags;

// VK_KHR_cooperative_matrix
extern PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR;

// VK_KHR_external_memory_capabilities
extern PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR vkGetPhysicalDeviceExternalBufferPropertiesKHR;

// VK_KHR_get_physical_device_properties2
extern PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
extern PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
extern PFN_vkGetPhysicalDeviceFormatProperties2KHR vkGetPhysicalDeviceFormatProperties2KHR;
extern PFN_vkGetPhysicalDeviceImageFormatProperties2KHR vkGetPhysicalDeviceImageFormatProperties2KHR;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR vkGetPhysicalDeviceQueueFamilyProperties2KHR;
extern PFN_vkGetPhysicalDeviceMemoryProperties2KHR vkGetPhysicalDeviceMemoryProperties2KHR;

// VK_KHR_get_surface_capabilities2
extern PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR vkGetPhysicalDeviceSurfaceCapabilities2KHR;
extern PFN_vkGetPhysicalDeviceSurfaceFormats2KHR vkGetPhysicalDeviceSurfaceFormats2KHR;

// VK_KHR_surface
extern PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
extern PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
extern PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
extern PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;

// VK_NV_cooperative_matrix
extern PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV vkGetPhysicalDeviceCooperativeMatrixPropertiesNV;

// get info
VKSHADER_API int get_gpu_count();
VKSHADER_API int get_default_gpu_index();

class GpuInfoPrivate;
class VKSHADER_API GpuInfo
{
public:
    explicit GpuInfo();
    virtual ~GpuInfo();

    // vulkan physical device
    VkPhysicalDevice physical_device() const;

    // memory properties
    const VkPhysicalDeviceMemoryProperties& physical_device_memory_properties() const;

    // info
    uint32_t api_version() const;
    uint32_t driver_version() const;
    uint32_t vendor_id() const;
    uint32_t device_id() const;
    const char* device_name() const;
    uint8_t* pipeline_cache_uuid() const;

    // 0 = discrete gpu
    // 1 = integrated gpu
    // 2 = virtual gpu
    // 3 = cpu
    int type() const;

    // hardware limit
    uint32_t max_shared_memory_size() const;
    uint32_t max_workgroup_count_x() const;
    uint32_t max_workgroup_count_y() const;
    uint32_t max_workgroup_count_z() const;
    uint32_t max_workgroup_invocations() const;
    uint32_t max_workgroup_size_x() const;
    uint32_t max_workgroup_size_y() const;
    uint32_t max_workgroup_size_z() const;
    size_t memory_map_alignment() const;
    size_t buffer_offset_alignment() const;
    size_t non_coherent_atom_size() const;
    size_t buffer_image_granularity() const;
    uint32_t max_image_dimension_1d() const;
    uint32_t max_image_dimension_2d() const;
    uint32_t max_image_dimension_3d() const;
    float timestamp_period() const;

    // runtime
    uint32_t compute_queue_family_index() const;
    uint32_t graphics_queue_family_index() const;
    uint32_t transfer_queue_family_index() const;

    uint32_t compute_queue_count() const;
    uint32_t graphics_queue_count() const;
    uint32_t transfer_queue_count() const;

    // property
    bool unified_compute_transfer_queue() const;

    // subgroup
    uint32_t subgroup_size() const;
    bool support_subgroup_basic() const;
    bool support_subgroup_vote() const;
    bool support_subgroup_ballot() const;
    bool support_subgroup_shuffle() const;

    // bug is not feature
    bool bug_storage_buffer_no_l1() const;
    bool bug_corrupted_online_pipeline_cache() const;
    bool bug_buffer_image_load_zero() const;

    // but sometimes bug is a feature
    bool bug_implicit_fp16_arithmetic() const;

    // fp16 and int8 feature
    bool support_fp16_packed() const;
    bool support_fp16_storage() const;
    bool support_fp16_uniform() const;
    bool support_fp16_arithmetic() const;
    bool support_int8_packed() const;
    bool support_int8_storage() const;
    bool support_int8_uniform() const;
    bool support_int8_arithmetic() const;

    // ycbcr conversion feature
    bool support_ycbcr_conversion() const;

    // cooperative matrix feature
    bool support_cooperative_matrix() const;
    bool support_cooperative_matrix_8_8_16() const;
    bool support_cooperative_matrix_16_8_8() const;
    bool support_cooperative_matrix_16_8_16() const;
    bool support_cooperative_matrix_16_16_16() const;

    // extension capability
    int support_VK_KHR_8bit_storage() const;
    int support_VK_KHR_16bit_storage() const;
    int support_VK_KHR_bind_memory2() const;
    int support_VK_KHR_buffer_device_address() const;
    int support_VK_KHR_create_renderpass2() const;
    int support_VK_KHR_cooperative_matrix() const;
    int support_VK_KHR_dedicated_allocation() const;
    int support_VK_KHR_descriptor_update_template() const;
    int support_VK_KHR_external_memory() const;
    int support_VK_KHR_get_memory_requirements2() const;
    int support_VK_KHR_maintenance1() const;
    int support_VK_KHR_maintenance2() const;
    int support_VK_KHR_maintenance3() const;
    int support_VK_KHR_multiview() const;
    int support_VK_KHR_portability_subset() const;
    int support_VK_KHR_push_descriptor() const;
    int support_VK_KHR_sampler_ycbcr_conversion() const;
    int support_VK_KHR_shader_float16_int8() const;
    int support_VK_KHR_shader_float_controls() const;
    int support_VK_KHR_storage_buffer_storage_class() const;
    int support_VK_KHR_swapchain() const;
    int support_VK_EXT_buffer_device_address() const;
    int support_VK_EXT_descriptor_indexing() const;
    int support_VK_EXT_memory_budget() const;
    int support_VK_EXT_memory_priority() const;
    int support_VK_EXT_queue_family_foreign() const;
    int support_VK_AMD_device_coherent_memory() const;
    int support_VK_NV_cooperative_matrix() const;

private:
    GpuInfo(const GpuInfo&);
    GpuInfo& operator=(const GpuInfo&);

private:
    friend int create_gpu_instance();
    GpuInfoPrivate* const d;
};

VKSHADER_API const GpuInfo& get_gpu_info(int device_index = get_default_gpu_index());

class VkAllocator;
class VkCompute;
class Option;
class PipelineCache;
class VulkanDevicePrivate;
class VKSHADER_API VulkanDevice
{
public:
    VulkanDevice(int device_index = get_default_gpu_index());
    ~VulkanDevice();

    const GpuInfo& info;

    VkDevice vkdevice() const;

    VkShaderModule compile_shader_module(const uint32_t* spv_data, size_t spv_data_size) const;

    // with fixed workgroup size
    VkShaderModule compile_shader_module(const uint32_t* spv_data, size_t spv_data_size, uint32_t local_size_x, uint32_t local_size_y, uint32_t local_size_z) const;

    // helper for creating pipeline
    int create_descriptorset_layout(int binding_count, const int* binding_types, VkDescriptorSetLayout* descriptorset_layout) const;
    int create_pipeline_layout(int push_constant_count, VkDescriptorSetLayout descriptorset_layout, VkPipelineLayout* pipeline_layout) const;
    int create_pipeline(VkShaderModule shader_module, VkPipelineLayout pipeline_layout, const std::vector<vk_specialization_type>& specializations, VkPipeline* pipeline) const;
    int create_descriptor_update_template(int binding_count, const int* binding_types, VkDescriptorSetLayout descriptorset_layout, VkPipelineLayout pipeline_layout, VkDescriptorUpdateTemplateKHR* descriptor_update_template) const;

    uint32_t find_memory_index(uint32_t memory_type_bits, VkFlags required, VkFlags preferred, VkFlags preferred_not) const;
    bool is_mappable(uint32_t memory_type_index) const;
    bool is_coherent(uint32_t memory_type_index) const;

    VkQueue acquire_queue(uint32_t queue_family_index) const;
    void reclaim_queue(uint32_t queue_family_index, VkQueue queue) const;

    // allocator on this device
    VkAllocator* acquire_blob_allocator() const;
    void reclaim_blob_allocator(VkAllocator* allocator) const;

    VkAllocator* acquire_staging_allocator() const;
    void reclaim_staging_allocator(VkAllocator* allocator) const;

    // immutable sampler for texelfetch
    const VkSampler* immutable_texelfetch_sampler() const;

    // dummy buffer image
    VkMat get_dummy_buffer() const;
    VkImageMat get_dummy_image() const;
    VkImageMat get_dummy_image_readonly() const;

    // pipeline cache on this device
    const PipelineCache* get_pipeline_cache() const;

    // test image allocation
    bool shape_support_image_storage(const ImMat& shape) const;

    // current gpu heap memory budget in MB
    uint32_t get_heap_budget() const;

    // current gpu heap memory usage in MB
    uint32_t get_heap_usage() const;

    // get current device index
    int get_device_index() const { return device_number; }

    // utility operator
    void convert_packing(const VkMat& src, VkMat& dst, int dst_elempack, VkCompute& cmd, const Option& opt) const;
    void convert_packing(const VkImageMat& src, VkImageMat& dst, int dst_elempack, VkCompute& cmd, const Option& opt) const;
    void convert_packing(const VkMat& src, VkImageMat& dst, int dst_elempack, VkCompute& cmd, const Option& opt) const;
    void convert_packing(const VkImageMat& src, VkMat& dst, int dst_elempack, VkCompute& cmd, const Option& opt) const;

    // VK_KHR_bind_memory2
    PFN_vkBindBufferMemory2KHR vkBindBufferMemory2KHR;
    PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR;

    // VK_KHR_buffer_device_address
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    PFN_vkGetBufferOpaqueCaptureAddressKHR vkGetBufferOpaqueCaptureAddressKHR;
    PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR vkGetDeviceMemoryOpaqueCaptureAddressKHR;

    // VK_KHR_descriptor_update_template
    PFN_vkCreateDescriptorUpdateTemplateKHR vkCreateDescriptorUpdateTemplateKHR;
    PFN_vkDestroyDescriptorUpdateTemplateKHR vkDestroyDescriptorUpdateTemplateKHR;
    PFN_vkUpdateDescriptorSetWithTemplateKHR vkUpdateDescriptorSetWithTemplateKHR;

    // VK_KHR_get_memory_requirements2
    PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR;
    PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR;

    // VK_KHR_maintenance1
    PFN_vkTrimCommandPoolKHR vkTrimCommandPoolKHR;

    // VK_KHR_maintenance3
    PFN_vkGetDescriptorSetLayoutSupportKHR vkGetDescriptorSetLayoutSupportKHR;

    // VK_KHR_push_descriptor
    PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR;
    PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;

    // VK_KHR_sampler_ycbcr_conversion
    PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR;
    PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR;

    // VK_KHR_swapchain
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    PFN_vkQueuePresentKHR vkQueuePresentKHR;

    // VK_EXT_buffer_device_address
    PFN_vkGetBufferDeviceAddressEXT vkGetBufferDeviceAddressEXT;

protected:
    // device extension
    int init_device_extension();

private:
    VulkanDevice(const VulkanDevice&);
    VulkanDevice& operator=(const VulkanDevice&);

private:
    VulkanDevicePrivate* const d;
    int device_number {-1};
};

VKSHADER_API VulkanDevice* get_gpu_device(int device_index = get_default_gpu_index());

// online spirv compilation
VKSHADER_API int compile_spirv_module(const char* comp_string, const Option& opt, std::vector<uint32_t>& spirv);
VKSHADER_API int compile_spirv_module(const char* comp_string, const Option& opt, std::vector<uint32_t>& spirv, std::string& log);
VKSHADER_API int compile_spirv_module(const char* comp_data, int comp_data_size, const Option& opt, std::vector<uint32_t>& spirv, std::string& log);

// info from spirv
class ShaderInfo
{
public:
    int specialization_count;
    int binding_count;
    int push_constant_count;

    // 0 = null
    // 1 = storage buffer
    // 2 = storage image
    // 3 = combined image sampler
    int binding_types[BINDING_MAX]; // 32 is large enough I think ...

    int reserved_0;
    int reserved_1;
    int reserved_2;
    int reserved_3;
};

VKSHADER_API int resolve_shader_info(const uint32_t* spv_data, size_t spv_data_size, ShaderInfo& shader_info);

VKSHADER_API void cast_float32_to_float16(const ImMat& src, ImMat& dst, const Option& opt = Option());
VKSHADER_API void cast_float16_to_float32(const ImMat& src, ImMat& dst, const Option& opt = Option());
VKSHADER_API void cast_int8_to_float32(const ImMat& src, ImMat& dst, const Option& opt = Option());
VKSHADER_API void cast_int8_to_float16(const ImMat& src, ImMat& dst, const Option& opt = Option());
VKSHADER_API void cast_float32_to_bfloat16(const ImMat& src, ImMat& dst, const Option& opt = Option());
VKSHADER_API void cast_bfloat16_to_float32(const ImMat& src, ImMat& dst, const Option& opt = Option());

} // namespace ImGui
