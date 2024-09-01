#pragma once
#include "imvk_gpu.h"
#include "imvk_pipeline.h"
#include "imvk_command.h"
#include "immat.h"
#define NO_STB_IMAGE
#include "imgui.h"
#include <vulkan/vulkan.h>

namespace ImGui
{
    VKSHADER_API void  ImVulkanGetVersion(int& major, int& minor, int& patch, int& build);
    VKSHADER_API void  ImVulkanImMatToVkMat(const ImMat &src, VkMat &dst);
    VKSHADER_API void  ImVulkanVkMatToImMat(const VkMat &src, ImMat &dst);
    VKSHADER_API void  ImVulkanVkMatToVkImageMat(const VkMat &src, VkImageMat &dst);
    VKSHADER_API void* ImVulkanVkMatMapping(const VkMat &src);
    VKSHADER_API void  ImVulkanShaderInit();
    VKSHADER_API void  ImVulkanShaderClear();
    VKSHADER_API float ImVulkanPeak(VulkanDevice* vkdev, int loop, int count_mb, int cmd_loop, int storage_type, int arithmetic_type, int packing_type);
} //namespace ImGui