#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "imvk_platform.h"
#include "immat.h"
#include <stdlib.h>
#include <vulkan/vulkan.h>

namespace ImGui 
{
class VulkanDevice;
class VKSHADER_API VkBufferMemory
{
public:
    VkBuffer buffer;

    // the base offset assigned by allocator
    size_t offset;
    size_t capacity;

    VkDeviceMemory memory;
    void* mapped_ptr;

    // buffer state, modified by command functions internally
    mutable VkAccessFlags access_flags;
    mutable VkPipelineStageFlags stage_flags;

    // initialize and modified by mat
    int refcount;
};

class VkImageMemory
{
public:
    VkImage image;
    VkImageView imageview;

    // underlying info assigned by allocator
    int width;
    int height;
    int depth;
    VkFormat format;

    VkDeviceMemory memory;
    void* mapped_ptr;

    // the base offset assigned by allocator
    size_t bind_offset;
    size_t bind_capacity;

    // image state, modified by command functions internally
    mutable VkAccessFlags access_flags;
    mutable VkImageLayout image_layout;
    mutable VkPipelineStageFlags stage_flags;

    // in-execution state, modified by command functions internally
    mutable int command_refcount;

    // initialize and modified by mat
    int refcount;
};

class VkAllocator : public Allocator
{
public:
    explicit VkAllocator(const VulkanDevice* _vkdev);
    virtual ~VkAllocator();

    virtual void clear();

    virtual VkBufferMemory* fastMalloc(size_t size) = 0;
    virtual void fastFree(VkBufferMemory* ptr) = 0;
    virtual int flush(VkBufferMemory* ptr);
    virtual int invalidate(VkBufferMemory* ptr);

    virtual VkImageMemory* fastMalloc(int w, int h, int c, size_t elemsize, int elempack) = 0;
    virtual void fastFree(VkImageMemory* ptr) = 0;
    virtual int getDeviceIndex();

    void* fastMalloc(size_t size, ImDataDevice device)
    {
        if (device == IM_DD_VULKAN)
        {
            VkBufferMemory* ptr = fastMalloc(size);
            return ptr;
        }
        return nullptr;
    }

    void fastFree(void* ptr, ImDataDevice device)
    {
        if (device == IM_DD_VULKAN)
            fastFree((VkBufferMemory*)ptr);
        else if (device == IM_DD_VULKAN_IMAGE)
            fastFree((VkImageMemory*)ptr);
    }

    int flush(void* ptr, ImDataDevice device)
    {
        if (device == IM_DD_VULKAN)
            return flush((VkBufferMemory*)ptr);
        return -1;
    }

    int invalidate(void* ptr, ImDataDevice device)
    {
        if (device == IM_DD_VULKAN)
            return invalidate((VkBufferMemory*)ptr);
        return -1;
    }

public:
    const VulkanDevice* vkdev;
    uint32_t buffer_memory_type_index;
    uint32_t image_memory_type_index;
    uint32_t reserved_type_index;
    bool mappable;
    bool coherent;

protected:
    VkBuffer create_buffer(size_t size, VkBufferUsageFlags usage);
    VkDeviceMemory allocate_memory(size_t size, uint32_t memory_type_index);
    VkDeviceMemory allocate_dedicated_memory(size_t size, uint32_t memory_type_index, VkImage image, VkBuffer buffer);

    VkImage create_image(int width, int height, int depth, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage);
    VkImageView create_imageview(VkImage image, VkFormat format);
};

class VkBlobAllocatorPrivate;
class VkBlobAllocator : public VkAllocator
{
public:
    explicit VkBlobAllocator(const VulkanDevice* vkdev, size_t preferred_block_size = 16 * 1024 * 1024); // 16M
    virtual ~VkBlobAllocator();

public:
    // release all budgets immediately
    virtual void clear();

    virtual VkBufferMemory* fastMalloc(size_t size);
    virtual void fastFree(VkBufferMemory* ptr);
    virtual VkImageMemory* fastMalloc(int w, int h, int c, size_t elemsize, int elempack);
    virtual void fastFree(VkImageMemory* ptr);

    void* fastMalloc(size_t size, ImDataDevice device)
    {
        if (device == IM_DD_VULKAN)
        {
            VkBufferMemory* ptr = fastMalloc(size);
            return ptr;
        }
        return nullptr;
    }

    void* fastMalloc(int w, int h, int c, size_t elemsize, int elempack, ImDataDevice device)
    {
        if (device == IM_DD_VULKAN_IMAGE)
        {
            VkImageMemory* ptr = fastMalloc(w, h, c, elemsize, elempack);
            return ptr;
        }
        return nullptr;
    }

    void fastFree(void* ptr, ImDataDevice device)
    {
        if (device == IM_DD_VULKAN)
            fastFree((VkBufferMemory*)ptr);
        else if (device == IM_DD_VULKAN_IMAGE)
            fastFree((VkImageMemory*)ptr);
    }

private:
    VkBlobAllocator(const VkBlobAllocator&);
    VkBlobAllocator& operator=(const VkBlobAllocator&);

private:
    VkBlobAllocatorPrivate* const d;
};

class VkWeightAllocatorPrivate;
class VkWeightAllocator : public VkAllocator
{
public:
    explicit VkWeightAllocator(const VulkanDevice* vkdev, size_t preferred_block_size = 8 * 1024 * 1024); // 8M
    virtual ~VkWeightAllocator();

public:
    // release all blocks immediately
    virtual void clear();

public:
    virtual VkBufferMemory* fastMalloc(size_t size);
    virtual void fastFree(VkBufferMemory* ptr);
    virtual VkImageMemory* fastMalloc(int w, int h, int c, size_t elemsize, int elempack);
    virtual void fastFree(VkImageMemory* ptr);

    void* fastMalloc(size_t size, ImDataDevice device)
    {
        if (device == IM_DD_VULKAN)
        {
            VkBufferMemory* ptr = fastMalloc(size);
            return ptr;
        }
        return nullptr;
    }

    void* fastMalloc(int w, int h, int c, size_t elemsize, int elempack, ImDataDevice device)
    {
        if (device == IM_DD_VULKAN_IMAGE)
        {
            VkImageMemory* ptr = fastMalloc(w, h, c, elemsize, elempack);
            return ptr;
        }
        return nullptr;
    }

    void fastFree(void* ptr, ImDataDevice device)
    {
        if (device == IM_DD_VULKAN)
            fastFree((VkBufferMemory*)ptr);
        else if (device == IM_DD_VULKAN_IMAGE)
            fastFree((VkImageMemory*)ptr);
    }

private:
    VkWeightAllocator(const VkWeightAllocator&);
    VkWeightAllocator& operator=(const VkWeightAllocator&);

private:
    VkWeightAllocatorPrivate* const d;
};

class VkStagingAllocatorPrivate;
class VkStagingAllocator : public VkAllocator
{
public:
    explicit VkStagingAllocator(const VulkanDevice* vkdev);
    virtual ~VkStagingAllocator();

public:
    // ratio range 0 ~ 1
    // default cr = 0.75
    void set_size_compare_ratio(float scr);

    // release all budgets immediately
    virtual void clear();

    virtual VkBufferMemory* fastMalloc(size_t size);
    virtual void fastFree(VkBufferMemory* ptr);
    virtual VkImageMemory* fastMalloc(int w, int h, int c, size_t elemsize, int elempack);
    virtual void fastFree(VkImageMemory* ptr);

    void* fastMalloc(size_t size, ImDataDevice device)
    {
        if (device == IM_DD_VULKAN)
        {
            VkBufferMemory* ptr = fastMalloc(size);
            return ptr;
        }
        return nullptr;
    }

    void* fastMalloc(int w, int h, int c, size_t elemsize, int elempack, ImDataDevice device)
    {
        if (device == IM_DD_VULKAN_IMAGE)
        {
            VkImageMemory* ptr = fastMalloc(w, h, c, elemsize, elempack);
            return ptr;
        }
        return nullptr;
    }

    void fastFree(void* ptr, ImDataDevice device)
    {
        if (device == IM_DD_VULKAN)
            fastFree((VkBufferMemory*)ptr);
        else if (device == IM_DD_VULKAN_IMAGE)
            fastFree((VkImageMemory*)ptr);
    }

private:
    VkStagingAllocator(const VkStagingAllocator&);
    VkStagingAllocator& operator=(const VkStagingAllocator&);

private:
    VkStagingAllocatorPrivate* const d;
};

class VkWeightStagingAllocatorPrivate;
class VkWeightStagingAllocator : public VkAllocator
{
public:
    explicit VkWeightStagingAllocator(const VulkanDevice* vkdev);
    virtual ~VkWeightStagingAllocator();

public:
    virtual VkBufferMemory* fastMalloc(size_t size);
    virtual void fastFree(VkBufferMemory* ptr);
    virtual VkImageMemory* fastMalloc(int w, int h, int c, size_t elemsize, int elempack);
    virtual void fastFree(VkImageMemory* ptr);

private:
    VkWeightStagingAllocator(const VkWeightStagingAllocator&);
    VkWeightStagingAllocator& operator=(const VkWeightStagingAllocator&);

private:
    VkWeightStagingAllocatorPrivate* const d;
};

} // namespace ImGui

