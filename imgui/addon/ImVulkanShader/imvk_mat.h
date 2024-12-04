#pragma once
#include "imvk_allocator.h"
#include "imvk_option.h"
#include "imvk_platform.h"
#include <vulkan/vulkan.h>
#include <string.h>
#include <immat.h>

namespace ImGui 
{
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// VkMat Class define
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
class VKSHADER_API VkMat final : public ImMat
{
public:
    // empty
    VkMat();
    // vec
    VkMat(int w, size_t elemsize, VkAllocator* allocator);
    // image
    VkMat(int w, int h, size_t elemsize, VkAllocator* allocator);
    // dim
    VkMat(int w, int h, int c, size_t elemsize, VkAllocator* allocator);
    // packed vec
    VkMat(int w, size_t elemsize, int elempack, VkAllocator* allocator);
    // packed image
    VkMat(int w, int h, size_t elemsize, int elempack, VkAllocator* allocator);
    // packed dim
    VkMat(int w, int h, int c, size_t elemsize, int elempack, VkAllocator* allocator);
    // copy from VkMat
    VkMat(const VkMat& m);
     // copy from ImMat
    VkMat(const ImMat& m);
    // external vec
    VkMat(int w, VkBufferMemory* data, size_t elemsize, VkAllocator* allocator);
    // external image
    VkMat(int w, int h, VkBufferMemory* data, size_t elemsize, VkAllocator* allocator);
    // external dim
    VkMat(int w, int h, int c, VkBufferMemory* data, size_t elemsize, VkAllocator* allocator);
    // external packed vec
    VkMat(int w, VkBufferMemory* data, size_t elemsize, int elempack, VkAllocator* allocator);
    // external packed image
    VkMat(int w, int h, VkBufferMemory* data, size_t elemsize, int elempack, VkAllocator* allocator);
    // external packed dim
    VkMat(int w, int h, int c, VkBufferMemory* data, size_t elemsize, int elempack, VkAllocator* allocator);
    // release
    ~VkMat() {};
    // assign from VkMat
    VkMat& operator=(const VkMat& m);
    // assign from ImMat
    VkMat& operator=(const ImMat& m);
    // allocate vec
    void create(int w, size_t elemsize, VkAllocator* allocator);
    // allocate image
    void create(int w, int h, size_t elemsize, VkAllocator* allocator);
    // allocate dim
    void create(int w, int h, int c, size_t elemsize, VkAllocator* allocator);
    // allocate packed vec
    void create(int w, size_t elemsize, int elempack, VkAllocator* allocator);
    // allocate packed image
    void create(int w, int h, size_t elemsize, int elempack, VkAllocator* allocator);
    // allocate packed dim
    void create(int w, int h, int c, size_t elemsize, int elempack, VkAllocator* allocator);
    // allocate vec with type
    void create_type(int w, ImDataType t, VkAllocator* allocator);
    // allocate image with type
    void create_type(int w, int h, ImDataType t, VkAllocator* allocator);
    // allocate dim with type
    void create_type(int w, int h, int c, ImDataType t, VkAllocator* allocator);
    // allocate like
    void create_like(const ImMat& m, VkAllocator* allocator);
    // allocate like
    void create_like(const VkMat& m, VkAllocator* allocator);

    // mapped
    ImMat mapped() const;
    void* mapped_ptr() const;

    // low-level reference
    VkBuffer buffer() const;
    size_t buffer_offset() const;
    size_t buffer_capacity() const;

protected:
    void allocate_buffer() override;
};

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// VkMat Class
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
inline VkMat::VkMat()
    : ImMat()
{
    device = IM_DD_VULKAN;
}

inline VkMat::VkMat(int _w, size_t _elemsize, VkAllocator* _allocator)
    : ImMat()
{
    create(_w, _elemsize, _allocator);
}

inline VkMat::VkMat(int _w, int _h, size_t _elemsize, VkAllocator* _allocator)
    : ImMat()
{
    create(_w, _h, _elemsize, _allocator);
}

inline VkMat::VkMat(int _w, int _h, int _c, size_t _elemsize, VkAllocator* _allocator)
    : ImMat()
{
    create(_w, _h, _c, _elemsize, _allocator);
}

inline VkMat::VkMat(int _w, size_t _elemsize, int _elempack, VkAllocator* _allocator)
    : ImMat()
{
    create(_w, _elemsize, _elempack, _allocator);
}

inline VkMat::VkMat(int _w, int _h, size_t _elemsize, int _elempack, VkAllocator* _allocator)
    : ImMat()
{
    create(_w, _h, _elemsize, _elempack, _allocator);
}

inline VkMat::VkMat(int _w, int _h, int _c, size_t _elemsize, int _elempack, VkAllocator* _allocator)
    : ImMat()
{
    create(_w, _h, _c, _elemsize, _elempack, _allocator);
}

inline VkMat::VkMat(const VkMat& m)
    : ImMat(m)
{
}

inline VkMat::VkMat(const ImMat& m)
    : ImMat(m)
{
}

inline VkMat::VkMat(int _w, VkBufferMemory* _data, size_t _elemsize, VkAllocator* _allocator)
    : ImMat(_w, _data, _elemsize, (Allocator *)_allocator)
{
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline VkMat::VkMat(int _w, int _h, VkBufferMemory* _data, size_t _elemsize, VkAllocator* _allocator)
    : ImMat(_w, _h, _data, _elemsize, (Allocator *)_allocator)
{
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline VkMat::VkMat(int _w, int _h, int _c, VkBufferMemory* _data, size_t _elemsize, VkAllocator* _allocator)
    : ImMat(_w, _h, _c, _data, _elemsize, (Allocator *)_allocator)
{
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline VkMat::VkMat(int _w, VkBufferMemory* _data, size_t _elemsize, int _elempack, VkAllocator* _allocator)
    : ImMat(_w, _data, _elemsize, _elempack, (Allocator *)_allocator)
{
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline VkMat::VkMat(int _w, int _h, VkBufferMemory* _data, size_t _elemsize, int _elempack, VkAllocator* _allocator)
    : ImMat(_w, _h, _data, _elemsize, _elempack, (Allocator *)_allocator)
{
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline VkMat::VkMat(int _w, int _h, int _c, VkBufferMemory* _data, size_t _elemsize, int _elempack, VkAllocator* _allocator)
    : ImMat(_w, _h, _c, _data, _elemsize, _elempack, (Allocator *)_allocator)
{
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline VkMat& VkMat::operator=(const VkMat& m)
{
    ImMat& dstMat = static_cast<ImMat&>(*this);
    const ImMat& srcMat = static_cast<const ImMat&>(m);
    dstMat = srcMat;
    return *this;
}

inline VkMat& VkMat::operator=(const ImMat& m)
{
    ImMat& dstMat = static_cast<ImMat&>(*this);
    dstMat = m;
    return *this;
}

inline void VkMat::allocate_buffer()
{
    size_t totalsize = Im_AlignSize(total() * elemsize, 4);

    data = ((VkAllocator*)allocator)->fastMalloc(totalsize);
    if (!data)
        return;

    refcount = std::make_shared<RefCount>();
}

inline void VkMat::create(int _w, size_t _elemsize, VkAllocator* _allocator)
{
    if (dims == 1 && w == _w && elemsize == _elemsize && elempack == 1 && allocator == _allocator)
        return;

    release();

    elemsize = _elemsize;
    elempack = 1;
    allocator = _allocator;

    dims = 1;
    dw = w = _w;
    dh = h = 1;
    c = 1;
    type = _elemsize == 1 ? IM_DT_INT8 : _elemsize == 2 ? IM_DT_INT16 : IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = IM_CF_GRAY;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    depth = _elemsize == 1 ? 8 : _elemsize == 2 ? 16 : 32;
    time_stamp = NAN;
    duration = NAN;
    index_count = -1;

    cstep = w;

    if (total() > 0)
        allocate_buffer();
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline void VkMat::create(int _w, int _h, size_t _elemsize, VkAllocator* _allocator)
{
    if (dims == 2 && w == _w && h == _h && elemsize == _elemsize && elempack == 1 && allocator == _allocator)
        return;

    release();

    elemsize = _elemsize;
    elempack = 1;
    allocator = _allocator;

    dims = 2;
    dw = w = _w;
    dh = h = _h;
    c = 1;
    type = _elemsize == 1 ? IM_DT_INT8 : _elemsize == 2 ? IM_DT_INT16 : IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = IM_CF_GRAY;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    depth = _elemsize == 1 ? 8 : _elemsize == 2 ? 16 : 32;
    time_stamp = NAN;
    duration = NAN;
    index_count = -1;

    cstep = (size_t)w * h;

    if (total() > 0)
        allocate_buffer();
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline void VkMat::create(int _w, int _h, int _c, size_t _elemsize, VkAllocator* _allocator)
{
    if (dims == 3 && w == _w && h == _h && c == _c && elemsize == _elemsize && elempack == 1 && allocator == _allocator)
        return;

    release();

    elemsize = _elemsize;
    elempack = 1;
    allocator = _allocator;

    dims = _c == 1 ? 2 : 3;
    dw = w = _w;
    dh = h = _h;
    c = _c;
    type = _elemsize == 1 ? IM_DT_INT8 : _elemsize == 2 ? IM_DT_INT16 : IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = c == 1 ? IM_CF_GRAY : c == 3 ? IM_CF_BGR : IM_CF_ABGR;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    depth = _elemsize == 1 ? 8 : _elemsize == 2 ? 16 : 32;
    time_stamp = NAN;
    duration = NAN;
    index_count = -1;

    cstep = Im_AlignSize((size_t)w * h * elemsize, 16) / elemsize;

    if (total() > 0)
        allocate_buffer();
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline void VkMat::create(int _w, size_t _elemsize, int _elempack, VkAllocator* _allocator)
{
    if (dims == 1 && w == _w && elemsize == _elemsize && elempack == _elempack && allocator == _allocator)
        return;

    release();

    elemsize = _elemsize;
    elempack = _elempack;
    allocator = _allocator;

    dims = 1;
    dw = w = _w;
    dh = h = 1;
    c = 1;
    type = _elemsize == 1 ? IM_DT_INT8 : _elemsize == 2 ? IM_DT_INT16 : IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = IM_CF_GRAY;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    depth = _elemsize == 1 ? 8 : _elemsize == 2 ? 16 : 32;
    time_stamp = NAN;
    duration = NAN;
    index_count = -1;

    cstep = w;

    if (total() > 0)
        allocate_buffer();
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline void VkMat::create(int _w, int _h, size_t _elemsize, int _elempack, VkAllocator* _allocator)
{
    if (dims == 2 && w == _w && h == _h && elemsize == _elemsize && elempack == _elempack && allocator == _allocator)
        return;

    release();

    elemsize = _elemsize;
    elempack = _elempack;
    allocator = _allocator;

    dims = 2;
    dw = w = _w;
    dh = h = _h;
    c = 1;
    type = _elemsize == 1 ? IM_DT_INT8 : _elemsize == 2 ? IM_DT_INT16 : IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = IM_CF_GRAY;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    depth = _elemsize == 1 ? 8 : _elemsize == 2 ? 16 : 32;
    time_stamp = NAN;
    duration = NAN;
    index_count = -1;

    cstep = (size_t)w * h;

    if (total() > 0)
        allocate_buffer();
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline void VkMat::create(int _w, int _h, int _c, size_t _elemsize, int _elempack, VkAllocator* _allocator)
{
    if (dims == 3 && w == _w && h == _h && c == _c && elemsize == _elemsize && elempack == _elempack && allocator == _allocator)
        return;

    release();

    elemsize = _elemsize;
    elempack = _elempack;
    allocator = _allocator;

    dims = _c == 1 ? 2 : 3;
    dw = w = _w;
    dh = h = _h;
    c = _c;
    type = _elemsize == 1 ? IM_DT_INT8 : _elemsize == 2 ? IM_DT_INT16 : IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = c == 1 ? IM_CF_GRAY : c == 3 ? IM_CF_BGR : IM_CF_ABGR;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = _elempack == _elemsize * _c ?  ORD_NWHC : ORD_NCWH;
    depth = _elemsize == 1 ? 8 : _elemsize == 2 ? 16 : 32;
    time_stamp = NAN;
    duration = NAN;
    index_count = -1;

    cstep = Im_AlignSize((size_t)w * h * elemsize, 16) / elemsize;

    if (total() > 0)
        allocate_buffer();
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline void VkMat::create_type(int _w, ImDataType _t, VkAllocator* _allocator)
{
    if (dims == 1 && w == _w && elempack == 1 && type == _t && allocator == _allocator)
        return;

    release();

    elemsize = IM_ESIZE(_t);
    elempack = 1;
    allocator = _allocator;

    dims = 1;
    dw = w = _w;
    dh = h = 1;
    c = 1;

    cstep = w;
    type = _t;
    color_space = IM_CS_SRGB;
    color_format = IM_CF_GRAY;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    time_stamp = NAN;
    duration = NAN;
    index_count = -1;
    depth = IM_DEPTH(_t);

    if (total() > 0)
        allocate_buffer();
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline void VkMat::create_type(int _w, int _h, ImDataType _t, VkAllocator* _allocator)
{
    if (dims == 2 && w == _w && h == _h && elempack == 1 && type == _t && allocator == _allocator)
        return;

    release();

    elemsize = IM_ESIZE(_t);
    elempack = 1;
    allocator = _allocator;

    dims = 2;
    dw = w = _w;
    dh = h = _h;
    c = 1;

    cstep = (size_t)w * h;
    type = _t;
    color_space = IM_CS_SRGB;
    color_format = IM_CF_GRAY;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    time_stamp = NAN;
    duration = NAN;
    index_count = -1;
    depth = IM_DEPTH(_t);

    if (total() > 0)
        allocate_buffer();
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline void VkMat::create_type(int _w, int _h, int _c, ImDataType _t, VkAllocator* _allocator)
{
    if (dims == 3 && w == _w && h == _h && c == _c && elempack == 1 && type == _t && allocator == _allocator)
        return;

    release();

    elemsize = IM_ESIZE(_t);
    elempack = 1;
    allocator = _allocator;

    dims = _c == 1 ? 2 : 3;
    dw = w = _w;
    dh = h = _h;
    c = _c;

    cstep = Im_AlignSize((size_t)w * h * elemsize, 16) / elemsize;
    type = _t;
    color_space = IM_CS_SRGB;
    color_format = c == 1 ? IM_CF_GRAY : c == 3 ? IM_CF_BGR : IM_CF_ABGR;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    time_stamp = NAN;
    duration = NAN;
    index_count = -1;
    depth = IM_DEPTH(_t);

    if (total() > 0)
        allocate_buffer();
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline void VkMat::create_like(const ImMat& m, VkAllocator* _allocator)
{
    int _dims = m.dims;
    if (_dims == 1)
        create(m.w, m.elemsize, m.elempack, _allocator);
    if (_dims == 2)
        create(m.w, m.h, m.elemsize, m.elempack, _allocator);
    if (_dims == 3)
        create(m.w, m.h, m.c, m.elemsize, m.elempack, _allocator);
    dw = m.dw;
    dh = m.dh;
    type = m.type;
    color_space = m.color_space;
    color_format = m.color_format;
    color_range = m.color_range;
    flags = m.flags;
    rate = m.rate;
    ord = m.ord;
    time_stamp = m.time_stamp;
    duration = m.duration;
    depth = m.depth;
    index_count = m.index_count;
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline void VkMat::create_like(const VkMat& m, VkAllocator* _allocator)
{
    int _dims = m.dims;
    if (_dims == 1)
        create(m.w, m.elemsize, m.elempack, _allocator);
    if (_dims == 2)
        create(m.w, m.h, m.elemsize, m.elempack, _allocator);
    if (_dims == 3)
        create(m.w, m.h, m.c, m.elemsize, m.elempack, _allocator);
    dw = m.dw;
    dh = m.dh;
    type = m.type;
    color_space = m.color_space;
    color_format = m.color_format;
    color_range = m.color_range;
    flags = m.flags;
    rate = m.rate;
    ord = m.ord;
    time_stamp = m.time_stamp;
    duration = m.duration;
    depth = m.depth;
    index_count = m.index_count;
    allocator = _allocator;
    device = IM_DD_VULKAN;
    device_number = _allocator ? _allocator->getDeviceIndex() : -1;
}

inline ImMat VkMat::mapped() const
{
    VkAllocator* _allocator = (VkAllocator*)allocator;
    if (!_allocator || _allocator->mappable)
        return ImMat();

    if (dims == 1)
        return ImMat(w, mapped_ptr(), elemsize, elempack, 0);

    if (dims == 2)
        return ImMat(w, h, mapped_ptr(), elemsize, elempack, 0);

    if (dims == 3)
        return ImMat(w, h, c, mapped_ptr(), elemsize, elempack, 0);

    return ImMat();
}

inline void* VkMat::mapped_ptr() const
{
    VkAllocator* _allocator = (VkAllocator*)allocator;
    
    if (!allocator || !_allocator->mappable)
        return NULL;
    VkBufferMemory* _data = (VkBufferMemory*)data;
    return (unsigned char*)_data->mapped_ptr + _data->offset;
}

inline VkBuffer VkMat::buffer() const
{
    VkBufferMemory* _data = (VkBufferMemory*)data;
    return _data->buffer;
}

inline size_t VkMat::buffer_offset() const
{
    VkBufferMemory* _data = (VkBufferMemory*)data;
    return _data->offset;
}

inline size_t VkMat::buffer_capacity() const
{
    VkBufferMemory* _data = (VkBufferMemory*)data;
    return _data->capacity;
}

} // namespace ImGui
