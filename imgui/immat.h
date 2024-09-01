#ifndef __IMMAT_H__
#define __IMMAT_H__
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <assert.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <functional>
// the alignment of all the allocated buffers
#if __AVX__
#define IM_MALLOC_ALIGN 32
#include <immintrin.h>
#elif __SSE__
#define IM_MALLOC_ALIGN 16
#include <immintrin.h>
#elif __SSE4_1__
#define IM_MALLOC_ALIGN 16
#include <smmintrin.h>
#elif __ARM_NEON
#define IM_MALLOC_ALIGN 16
#include <arm_neon.h>
#else
#define IM_MALLOC_ALIGN 8
#endif

// we have some optimized kernels that may overread buffer a bit in loop
// it is common to interleave next-loop data load with arithmetic instructions
// allocating more bytes keeps us safe from SEGV_ACCERR failure
#define IM_MALLOC_OVERREAD 64

#define OMP_THREADS 2
// exchange-add operation for atomic operations on reference counters
#if defined __riscv && !defined __riscv_atomic
// riscv target without A extension
static inline int IM_XADD(int* addr, int delta)
{
    int tmp = *addr;
    *addr += delta;
    return tmp;
}
#elif defined __INTEL_COMPILER && !(defined WIN32 || defined _WIN32)
// atomic increment on the linux version of the Intel(tm) compiler
#define IM_XADD(addr, delta) (int)_InterlockedExchangeAdd(const_cast<void*>(reinterpret_cast<volatile void*>(addr)), delta)
#elif defined __GNUC__
#if defined __clang__ && __clang_major__ >= 3 && !defined __ANDROID__ && !defined __EMSCRIPTEN__ && !defined(__CUDACC__)
#ifdef __ATOMIC_ACQ_REL
#define IM_XADD(addr, delta) __c11_atomic_fetch_add((_Atomic(int)*)(addr), delta, __ATOMIC_ACQ_REL)
#else
#define IM_XADD(addr, delta) __atomic_fetch_add((_Atomic(int)*)(addr), delta, 4)
#endif
#else
#if defined __ATOMIC_ACQ_REL && !defined __clang__
// version for gcc >= 4.7
#define IM_XADD(addr, delta) (int)__atomic_fetch_add((unsigned*)(addr), (unsigned)(delta), __ATOMIC_ACQ_REL)
#else
#define IM_XADD(addr, delta) (int)__sync_fetch_and_add((unsigned*)(addr), (unsigned)(delta))
#endif
#endif
#elif defined _MSC_VER && !defined RC_INVOKED
#define IM_XADD(addr, delta) (int)_InterlockedExchangeAdd((long volatile*)addr, delta)
#else
// thread-unsafe branch
static inline int IM_XADD(int* addr, int delta)
{
    int tmp = *addr;
    *addr += delta;
    return tmp;
}
#endif

#ifndef IMMAT_API
#ifdef _WIN32
#define IMMAT_API __declspec( dllexport )
#else
#define IMMAT_API __attribute__((visibility("default")))
#endif
#endif

//////////////////////////////////////////////////
//  memory functions
/////////////////////////////////////////////////
template<typename _Tp>
inline _Tp* Im_AlignPtr(_Tp* ptr, int n = (int)sizeof(_Tp))
{
    return (_Tp*)(((size_t)ptr + n - 1) & -n);
}
inline size_t Im_AlignSize(size_t sz, int n)
{
    return (sz + n - 1) & -n;
}
inline void* Im_FastMalloc(size_t size)
{
#ifdef _WIN32
    void* ptr = _aligned_malloc(size, IM_MALLOC_ALIGN);
    if (ptr) memset(ptr, 0, size);
    return ptr;
#elif (defined(__unix__) || defined(__APPLE__)) && _POSIX_C_SOURCE >= 200112L || (__ANDROID__ && __ANDROID_API__ >= 17)
    void* ptr = nullptr;
    if (posix_memalign(&ptr, IM_MALLOC_ALIGN, size + IM_MALLOC_OVERREAD))
        ptr = nullptr;
    else
        memset(ptr, 0, size);
    return ptr;
#elif __ANDROID__ && __ANDROID_API__ < 17
    void* ptr = memalign(IM_MALLOC_ALIGN, size + IM_MALLOC_OVERREAD);
    if (ptr) memset(ptr, 0, size);
    return ptr;
#else
    unsigned char* udata = (unsigned char*)malloc(size + sizeof(void*) + IM_MALLOC_ALIGN + IM_MALLOC_OVERREAD);
    if (!udata)
        return 0;
    memset(udata, 0, size + sizeof(void*) + IM_MALLOC_ALIGN);
    unsigned char** adata = Im_AlignPtr((unsigned char**)udata + 1, IM_MALLOC_ALIGN);
    adata[-1] = udata;
    return adata;
#endif
}
inline void Im_FastFree(void* ptr)
{
    if (ptr)
    {
#ifdef _WIN32
        _aligned_free(ptr);
#elif (defined(__unix__) || defined(__APPLE__)) && _POSIX_C_SOURCE >= 200112L || (__ANDROID__ && __ANDROID_API__ >= 17)
        free(ptr);
#elif __ANDROID__ && __ANDROID_API__ < 17
        free(ptr);
#else
        unsigned char* udata = ((unsigned char**)ptr)[-1];
        free(udata);
#endif
    }
}

//////////////////////////////////////////////////
//  fp16 functions
/////////////////////////////////////////////////
static inline unsigned short im_float32_to_float16(float value)
{
    // 1 : 8 : 23
    union
    {
        unsigned int u;
        float f;
    } tmp;

    tmp.f = value;

    // 1 : 8 : 23
    unsigned short sign = (tmp.u & 0x80000000) >> 31;
    unsigned short exponent = (tmp.u & 0x7F800000) >> 23;
    unsigned int significand = tmp.u & 0x7FFFFF;

    // 1 : 5 : 10
    unsigned short fp16;
    if (exponent == 0)
    {
        // zero or denormal, always underflow
        fp16 = (sign << 15) | (0x00 << 10) | 0x00;
    }
    else if (exponent == 0xFF)
    {
        // infinity or NaN
        fp16 = (sign << 15) | (0x1F << 10) | (significand ? 0x200 : 0x00);
    }
    else
    {
        // normalized
        short newexp = exponent + (-127 + 15);
        if (newexp >= 31)
        {
            // overflow, return infinity
            fp16 = (sign << 15) | (0x1F << 10) | 0x00;
        }
        else if (newexp <= 0)
        {
            // Some normal fp32 cannot be expressed as normal fp16
            fp16 = (sign << 15) | (0x00 << 10) | 0x00;
        }
        else
        {
            // normal fp16
            fp16 = (sign << 15) | (newexp << 10) | (significand >> 13);
        }
    }

    return fp16;
}

static inline float im_float16_to_float32(unsigned short value)
{
    // 1 : 5 : 10
    unsigned short sign = (value & 0x8000) >> 15;
    unsigned short exponent = (value & 0x7c00) >> 10;
    unsigned short significand = value & 0x03FF;

    // 1 : 8 : 23
    union
    {
        unsigned int u;
        float f;
    } tmp;
    if (exponent == 0)
    {
        if (significand == 0)
        {
            // zero
            tmp.u = (sign << 31);
        }
        else
        {
            // denormal
            exponent = 0;
            // find non-zero bit
            while ((significand & 0x200) == 0)
            {
                significand <<= 1;
                exponent++;
            }
            significand <<= 1;
            significand &= 0x3FF;
            tmp.u = (sign << 31) | ((-exponent + (-15 + 127)) << 23) | (significand << 13);
        }
    }
    else if (exponent == 0x1F)
    {
        // infinity or NaN
        tmp.u = (sign << 31) | (0xFF << 23) | (significand << 13);
    }
    else
    {
        // normalized
        tmp.u = (sign << 31) | ((exponent + (-15 + 127)) << 23) | (significand << 13);
    }

    return tmp.f;
}

static inline unsigned short im_float32_to_bfloat16(float value)
{
    // 16 : 16
    union
    {
        unsigned int u;
        float f;
    } tmp;
    tmp.f = value;
    return tmp.u >> 16;
}

static inline float im_bfloat16_to_float32(unsigned short value)
{
    // 16 : 16
    union
    {
        unsigned int u;
        float f;
    } tmp;
    tmp.u = value << 16;
    return tmp.f;
}


////////////////////////////////////////////////////////////////////
// Type define
enum ImDataType {
    IM_DT_UNDEFINED = -1,
    IM_DT_INT8 = 0,
    IM_DT_INT16,
    IM_DT_INT32,
    IM_DT_INT64,
    IM_DT_FLOAT16,
    IM_DT_FLOAT32,
    IM_DT_FLOAT64,
    IM_DT_INT16_BE,
    IM_DT_NB_DATA_TYPE
};

enum ImDataDevice {
    IM_DD_CPU = 0,
    IM_DD_VULKAN,
    IM_DD_VULKAN_IMAGE,
    IM_DD_CUDA,
};

enum ImColorRange {
    IM_CR_FULL_RANGE = 0,
    IM_CR_NARROW_RANGE
};

enum ImColorSpace {
    IM_CS_SRGB = 0,
    IM_CS_BT601,
    IM_CS_BT709,
    IM_CS_BT2020,
    IM_CS_HSV,
    IM_CS_HLS,
    IM_CS_CMY,
    IM_CS_LAB
};

enum ImColorFormat {
    IM_CF_GRAY = 0,
    IM_CF_BGR,
    IM_CF_ABGR,
    IM_CF_BGRA,
    IM_CF_RGB,
    IM_CF_ARGB,
    IM_CF_RGBA,
    IM_CF_YUV420,
    IM_CF_YUV422,
    IM_CF_YUV440,
    IM_CF_YUV444,
    IM_CF_YUVA,
    IM_CF_NV12,
    IM_CF_P010LE,
    IM_CF_LAB,
    IM_CF_HSV,
    IM_CF_HSL,
};

enum ImInterpolateMode {
    IM_INTERPOLATE_NONE = 0,
    IM_INTERPOLATE_NEAREST,
    IM_INTERPOLATE_BILINEAR,
    IM_INTERPOLATE_BICUBIC,
    IM_INTERPOLATE_AREA,
    IM_INTERPOLATE_TRILINEAR,
    IM_INTERPOLATE_TETRAHEDRAL,
    IM_NB_INTERP_MODE
};

enum ImColorXYZSystem {
    IM_COLOR_XYZ_SRGB = 0,
    IM_COLOR_XYZ_ADOBE,
    IM_COLOR_XYZ_APPLE,
    IM_COLOR_XYZ_BRUCE,
    IM_COLOR_XYZ_PAL,
    IM_COLOR_XYZ_NTSC,
    IM_COLOR_XYZ_SMPTE,
    IM_COLOR_XYZ_CIE,
};

#define IM_MAT_FLAGS_NONE               (0 << 0)
// 0-7 bits for video
#define IM_MAT_FLAGS_VIDEO_FRAME        (1 << 0)
#define IM_MAT_FLAGS_VIDEO_INTERLACED   (1 << 1)
#define IM_MAT_FLAGS_VIDEO_FRAME_I      (1 << 2)
#define IM_MAT_FLAGS_VIDEO_FRAME_P      (1 << 3)
#define IM_MAT_FLAGS_VIDEO_FRAME_B      (1 << 4)
#define IM_MAT_FLAGS_VIDEO_HDR_PQ       (1 << 5)
#define IM_MAT_FLAGS_VIDEO_HDR_HLG      (1 << 6)
#define IM_MAT_FLAGS_VIDEO_FRAME_UV     (1 << 7)
// 8-15 bits for audio
#define IM_MAT_FLAGS_AUDIO_FRAME        (1 << 8)
//16-23 bits for image
#define IM_MAT_FLAGS_IMAGE_FRAME        (1 << 16)
//24-31 bits for custom
#define IM_MAT_FLAGS_CUSTOM_NORMAL      (1 << 24)
#define IM_MAT_FLAGS_CUSTOM_PREROLL     (1 << 25)
#define IM_MAT_FLAGS_CUSTOM_EOS         (1 << 26)
#define IM_MAT_FLAGS_CUSTOM_INVALID     (1 << 27)
#define IM_MAT_FLAGS_CUSTOM_UNSUPPORTED (1 << 28)
#define IM_MAT_FLAGS_CUSTOM_UPDATED     (1 << 29)


#define IM_ESIZE(a)    (a == IM_DT_INT8 ? (size_t)1u : (a == IM_DT_INT16 || a == IM_DT_INT16_BE || a == IM_DT_FLOAT16) ? (size_t)2u : (a == IM_DT_INT32 || a == IM_DT_FLOAT32) ? (size_t)4u : (a == IM_DT_INT64 || a == IM_DT_FLOAT64) ? (size_t)8u : (size_t)0u)
#define IM_DEPTH(a)    (a == IM_DT_INT8 ? 8 : (a == IM_DT_INT16 || a == IM_DT_INT16_BE || a == IM_DT_FLOAT16) ? 16 : (a == IM_DT_INT32 || a == IM_DT_FLOAT32) ? 32 : (a == IM_DT_INT64 || a == IM_DT_FLOAT64) ? 64 : 0)
#define IM_ISMONO(a)   (a == IM_CF_GRAY)
#define IM_ISRGB(a)    (a == IM_CF_BGR || a == IM_CF_RGB || a == IM_CF_ABGR || a == IM_CF_ARGB || a == IM_CF_BGRA || a == IM_CF_RGBA)
#define IM_ISYUV(a)    (a == IM_CF_YUV420 || a == IM_CF_YUV422 || a == IM_CF_YUV440 || a == IM_CF_YUV444 || a == IM_CF_YUVA || a == IM_CF_NV12 || a == IM_CF_P010LE)
#define IM_ISALPHA(a)  (a == IM_CF_ABGR || a == IM_CF_ARGB || a == IM_CF_YUVA)
#define IM_ISNV12(a)   (a == IM_CF_NV12 || a == IM_CF_P010LE)

template<typename T> 
static inline T CLAMP(T v, T mn, T mx) { return (v < mn) ? mn : (v > mx) ? mx : v; }

typedef struct Rational{
    int num; ///< Numerator
    int den; ///< Denominator
} Rational;

enum Ordination {
    ORD_NCWH = 0,
    ORD_NWHC,
    ORD_NCHW,
    ORD_NHWC,
    ORD_NUM
};

struct ImPoint
{
    float                                   x, y;
    constexpr ImPoint()                     : x(0.0f), y(0.0f) { }
    constexpr ImPoint(float _x, float _y)   : x(_x), y(_y) { }
    float  operator[] (size_t idx) const    { assert(idx <= 1); return (&x)[idx]; }
    float& operator[] (size_t idx)          { assert(idx <= 1); return (&x)[idx]; }
    bool operator==(const ImPoint& d) const { return fabs(x - d.x) < 10e-8 && fabs(y - d.y) < 10e-8; }
    bool operator==(const ImPoint& d)       { return fabs(x - d.x) < 10e-8 && fabs(y - d.y) < 10e-8; }
    bool operator!=(const ImPoint& d) const { return fabs(x - d.x) > 10e-8 || fabs(y - d.y) > 10e-8; }
    bool operator!=(const ImPoint& d)       { return fabs(x - d.x) > 10e-8 || fabs(y - d.y) > 10e-8; }
    ImPoint operator+(const float rhs)      { return ImPoint(x + rhs, y + rhs); }
    ImPoint operator-(const float rhs)      { return ImPoint(x - rhs, y - rhs); }
    ImPoint operator*(const float rhs)      { return ImPoint(x * rhs, y * rhs); }
    ImPoint operator/(const float rhs)      { return ImPoint(x / rhs, y / rhs); }
    ImPoint operator+(const ImPoint& rhs)   { return ImPoint(x + rhs.x, y + rhs.y); }
    ImPoint operator-(const ImPoint& rhs)   { return ImPoint(x - rhs.x, y - rhs.y); }
    ImPoint operator*(const ImPoint& rhs)   { return ImPoint(x * rhs.x, y * rhs.y); }
    ImPoint operator/(const ImPoint& rhs)   { return ImPoint(x / rhs.x, y / rhs.y); }
    void operator+=(const ImPoint& rhs)   { ImPoint t(*this); t.x += rhs.x; t.y += rhs.y;  *this = t; }
    void operator-=(const ImPoint& rhs)   { ImPoint t(*this); t.x -= rhs.x; t.y -= rhs.y;  *this = t; }
    void operator*=(const ImPoint& rhs)   { ImPoint t(*this); t.x *= rhs.x; t.y *= rhs.y;  *this = t; }
    void operator/=(const ImPoint& rhs)   { ImPoint t(*this); t.x /= rhs.x; t.y /= rhs.y;  *this = t; }
    void operator+=(const float rhs)   { ImPoint t(*this); t.x += rhs; t.y += rhs;  *this = t; }
    void operator-=(const float rhs)   { ImPoint t(*this); t.x -= rhs; t.y -= rhs;  *this = t; }
    void operator*=(const float rhs)   { ImPoint t(*this); t.x *= rhs; t.y *= rhs;  *this = t; }
    void operator/=(const float rhs)   { ImPoint t(*this); t.x /= rhs; t.y /= rhs;  *this = t; }
    ImPoint normalize()   {  ImPoint t(*this); float acc = sqrt(t.x*t.x + t.y*t.y); return ImPoint(t.x/acc, t.y/acc); }
    float dot(const ImPoint& d)             { return (x * d.x) + (y * d.y); }
    float length()                          { return sqrt(x * x + y * y); }
    float cross(const ImPoint& d)           { return (x * d.y) - (y * d.x); }
    float distance(const ImPoint& d)        { return sqrt((d.x - x) * (d.x - x) + (d.y - y) * (d.y - y)); }
};

struct ImPoint3D
{
    float                                       x, y, z;
    ImPoint3D()                                : x(0.0f), y(0.0f), z(0.0f) { }
    ImPoint3D(float _v)                        : x(_v), y(_v), z(_v) { }
    ImPoint3D(float _x, float _y, float _z)    : x(_x), y(_y), z(_z) { }
    float  operator[] (size_t idx) const    { assert(idx <= 1); return (&x)[idx]; }
    float& operator[] (size_t idx)          { assert(idx <= 1); return (&x)[idx]; }
    bool operator==(const ImPoint3D& d) const { return fabs(x - d.x) < 10e-8 && fabs(y - d.y) < 10e-8 && fabs(z - d.z) < 10e-8; }
    bool operator==(const ImPoint3D& d)       { return fabs(x - d.x) < 10e-8 && fabs(y - d.y) < 10e-8 && fabs(z - d.z) < 10e-8; }
    bool operator!=(const ImPoint3D& d) const { return fabs(x - d.x) > 10e-8 || fabs(y - d.y) > 10e-8 || fabs(z - d.z) > 10e-8; }
    bool operator!=(const ImPoint3D& d)       { return fabs(x - d.x) > 10e-8 || fabs(y - d.y) > 10e-8 || fabs(z - d.z) > 10e-8; }
    ImPoint3D operator+(const float rhs)      { return ImPoint3D(x + rhs, y + rhs, z + rhs); }
    ImPoint3D operator-(const float rhs)      { return ImPoint3D(x - rhs, y - rhs, z - rhs); }
    ImPoint3D operator*(const float rhs)      { return ImPoint3D(x * rhs, y * rhs, z * rhs); }
    ImPoint3D operator/(const float rhs)      { return ImPoint3D(x / rhs, y / rhs, z / rhs); }
    ImPoint3D operator+(const ImPoint3D& rhs) { return ImPoint3D(x + rhs.x, y + rhs.y, z + rhs.z); }
    ImPoint3D operator-(const ImPoint3D& rhs) { return ImPoint3D(x - rhs.x, y - rhs.y, z - rhs.z); }
    ImPoint3D operator*(const ImPoint3D& rhs) { return ImPoint3D(x * rhs.x, y * rhs.y, z * rhs.z); }
    ImPoint3D operator/(const ImPoint3D& rhs) { return ImPoint3D(x / rhs.x, y / rhs.y, z / rhs.z); }
    void operator+=(const ImPoint3D& rhs)   { ImPoint3D t(*this); t.x += rhs.x; t.y += rhs.y; t.z += rhs.z; *this = t; }
    void operator-=(const ImPoint3D& rhs)   { ImPoint3D t(*this); t.x -= rhs.x; t.y -= rhs.y; t.z += rhs.z; *this = t; }
    void operator*=(const ImPoint3D& rhs)   { ImPoint3D t(*this); t.x *= rhs.x; t.y *= rhs.y; t.z += rhs.z; *this = t; }
    void operator/=(const ImPoint3D& rhs)   { ImPoint3D t(*this); t.x /= rhs.x; t.y /= rhs.y; t.z += rhs.z; *this = t; }
    void operator+=(const float rhs)   { ImPoint3D t(*this); t.x += rhs; t.y += rhs; t.z += rhs; *this = t; }
    void operator-=(const float rhs)   { ImPoint3D t(*this); t.x -= rhs; t.y -= rhs; t.z += rhs; *this = t; }
    void operator*=(const float rhs)   { ImPoint3D t(*this); t.x *= rhs; t.y *= rhs; t.z += rhs; *this = t; }
    void operator/=(const float rhs)   { ImPoint3D t(*this); t.x /= rhs; t.y /= rhs; t.z += rhs; *this = t; }
    ImPoint3D normalize() { ImPoint3D t(*this); float acc = sqrt(t.x*t.x + t.y*t.y + t.z*t.z); return ImPoint3D(t.x/acc, t.y/acc, t.z/acc); }
    float dot(const ImPoint3D& b) const       { return x * b.x + y * b.y + z * b.z; }
    float length() const                      { return (float)sqrt(x * x + y * y + z * z); }
    ImPoint3D cross(const ImPoint3D& b) const { return ImPoint3D(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x); }
    float distance(const ImPoint3D& d)        { return sqrt((d.x - x) * (d.x - x) + (d.y - y) * (d.y - y) + (d.z - z) * (d.z - z)); }
};

struct ImPixel
{
    float r, g, b, a;
    constexpr ImPixel() : r(0.0f), g(0.0f), b(0.0f), a(0.0f) { }
    constexpr ImPixel(float _r, float _g, float _b, float _a)  : r(_r), g(_g), b(_b), a(_a) { }
    bool operator==(const ImPixel& d) const { return fabs(r - d.r) < 10e-8 && fabs(g - d.g) < 10e-8 && fabs(b - d.b) < 10e-8 && fabs(a - d.a) < 10e-8; }
    bool operator==(const ImPixel& d)       { return fabs(r - d.r) < 10e-8 && fabs(g - d.g) < 10e-8 && fabs(b - d.b) < 10e-8 && fabs(a - d.a) < 10e-8; }
    bool operator!=(const ImPixel& d) const { return fabs(r - d.r) > 10e-8 || fabs(g - d.g) > 10e-8 || fabs(b - d.b) > 10e-8 || fabs(a - d.a) > 10e-8; }
    bool operator!=(const ImPixel& d)       { return fabs(r - d.r) > 10e-8 || fabs(g - d.g) > 10e-8 || fabs(b - d.b) > 10e-8 || fabs(a - d.a) > 10e-8; }
    ImPixel operator+(const float rhs)      { return ImPixel(r + rhs, g + rhs, b + rhs, a + rhs); }
    ImPixel operator-(const float rhs)      { return ImPixel(r - rhs, g - rhs, b - rhs, a - rhs); }
    ImPixel operator*(const float rhs)      { return ImPixel(r * rhs, g * rhs, b * rhs, a * rhs); }
    ImPixel operator/(const float rhs)      { return ImPixel(r / rhs, g / rhs, b / rhs, a / rhs); }
    ImPixel operator+(const ImPixel& rhs)   { return ImPixel(r + rhs.r, g + rhs.g, b + rhs.b, a + rhs.a); }
    ImPixel operator-(const ImPixel& rhs)   { return ImPixel(r - rhs.r, g - rhs.g, b - rhs.b, a - rhs.a); }
    ImPixel operator*(const ImPixel& rhs)   { return ImPixel(r * rhs.r, g * rhs.g, b * rhs.b, a * rhs.a); }
    ImPixel operator/(const ImPixel& rhs)   { return ImPixel(r / rhs.r, g / rhs.g, b / rhs.b, a / rhs.a); }
};

struct ImSize
{
    int                                     w, h;
    constexpr ImSize()                      : w(0), h(0) { }
    constexpr ImSize(int _w, int _h)        : w(_w), h(_h) { }
    bool operator==(const ImSize& d) const  { return w == d.w && h == d.h; }
    bool operator==(const ImSize& d)        { return w == d.w && h == d.h; }
    bool operator!=(const ImSize& d) const  { return w != d.w || h != d.h; }
    bool operator!=(const ImSize& d)        { return w != d.w || h != d.h; }
    float area()                            { return w * h; }
};

struct ImBox
{
    ImPoint Min;
    ImPoint Max;

    constexpr       ImBox()                                        : Min(0.0f, 0.0f), Max(0.0f, 0.0f)  {}
    constexpr       ImBox(const ImPoint& min, const ImPoint& max)  : Min(min), Max(max)                {}
    constexpr       ImBox(float x1, float y1, float x2, float y2)  : Min(x1, y1), Max(x2, y2)          {}
    bool            operator==(const ImBox& d) const    { return Min == d.Min && Max == d.Max; }
    bool            operator==(const ImBox& d)          { return Min == d.Min && Max == d.Max; }
    bool            operator!=(const ImBox& d) const    { return Min != d.Min || Max != d.Max; }
    bool            operator!=(const ImBox& d)          { return Min != d.Min || Max != d.Max; }
    ImPoint         GetCenter() const                   { return ImPoint((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f); }
    ImPoint         GetSize() const                     { return ImPoint(Max.x - Min.x, Max.y - Min.y); }
    float           GetWidth() const                    { return Max.x - Min.x; }
    float           GetHeight() const                   { return Max.y - Min.y; }
    float           GetArea() const                     { return (Max.x - Min.x) * (Max.y - Min.y); }
    bool            Empty() const                       { return GetArea() <= FLT_EPSILON; }
    void            Clamp(const ImPoint& box_min, const ImPoint& box_max) { if (Min.x < box_min.x) Min.x = box_min.x; if (Min.y < box_min.y) Min.y = box_min.y; if (Max.x >= box_max.x) Max.x = box_max.x - 1; if (Max.y >= box_max.y) Max.y = box_max.y - 1; }
    void            Clamp(const ImBox& box)             { Clamp(box.Min, box.Max); }
    void            ClampSquare()                       { if (GetWidth() > GetHeight()) Max.x = Min.x + GetHeight(); else Max.y = Min.y + GetWidth(); }
    void            Expand(float ratio_w, float ratio_h, ImSize limite = ImSize())
                                                        {
                                                            auto width = GetWidth(); auto height = GetHeight();
                                                            Min.x -= width * ratio_w;
                                                            Min.y -= height * ratio_h;
                                                            Max.x += width * ratio_w;
                                                            Max.y += height * ratio_h;
                                                            if (limite.w > 0 && limite.h > 0) Clamp(ImBox(0, 0, limite.w, limite.h));
                                                        }
    void            Shift(float ratio_x, float ratio_y, ImSize limite = ImSize())
                                                        {
                                                            auto width = GetWidth(); auto height = GetHeight();
                                                            Min.x += width * ratio_x;
                                                            Min.y += height * ratio_y;
                                                            Max.x += width * ratio_x;
                                                            Max.y += height * ratio_y;
                                                            if (limite.w > 0 && limite.h > 0) Clamp(ImBox(0, 0, limite.w, limite.h));
                                                        }
    float           IntersectionArea(const ImBox& box) const
                                                        {
                                                            if (Min.x > box.Max.x || Max.x < box.Min.x || Min.y > box.Max.y || Max.y < box.Min.y) return 0.f;
                                                            float inter_width = std::min(Max.x, box.Max.x) - std::max(Min.x, box.Min.x);
                                                            float inter_height = std::min(Max.y, box.Max.y) - std::max(Min.y, box.Min.y);
                                                            return inter_width * inter_height;
                                                        }
};
////////////////////////////////////////////////////////////////////

namespace ImGui
{

static inline int GetColorFormatCategory(ImColorFormat fmt)
{
    if (fmt == IM_CF_GRAY)
        return 0;
    else if (fmt >= IM_CF_BGR && fmt <= IM_CF_RGBA)
        return 1;
    else if (fmt >= IM_CF_YUV420 && fmt <= IM_CF_P010LE)
        return 2;
    return -1;
}

static inline int GetChannelCountByColorFormat(ImColorFormat fmt)
{
    switch (fmt)
    {
        case IM_CF_GRAY:
            return 1;
        case IM_CF_YUV420:
        case IM_CF_YUV422:
        case IM_CF_YUV440:
        case IM_CF_NV12:
        case IM_CF_P010LE:
            return 2;
        case IM_CF_BGR:
        case IM_CF_RGB:
        case IM_CF_YUV444:
            return 3;
        case IM_CF_ABGR:
        case IM_CF_BGRA:
        case IM_CF_ARGB:
        case IM_CF_RGBA:
        case IM_CF_YUVA:
            return 4;
        case IM_CF_LAB:
        case IM_CF_HSL:
        case IM_CF_HSV:
            return 3;
        default:
            return 1;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// Allocator Class define
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

class Allocator
{
public:
    virtual ~Allocator() {};
    virtual void* fastMalloc(size_t size, ImDataDevice device) = 0;
    virtual void* fastMalloc(int w, int h, int c, size_t elemsize, int elempack, ImDataDevice device) = 0;
    virtual void fastFree(void* ptr, ImDataDevice device) = 0;
    virtual int flush(void* ptr, ImDataDevice device) = 0;
    virtual int invalidate(void* ptr, ImDataDevice device) = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// ImMat Class define
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
class ImMat
{
public:
    enum NormTypes : int {
        NORM_INF = 1,
        NORM_L1,
        NORM_L2,
        NORM_MINMAX,
    };
    // empty
    ImMat();
    // vec
    ImMat(int w, size_t elemsize = 4u, Allocator* allocator = 0);
    // image
    ImMat(int w, int h, size_t elemsize = 4u, Allocator* allocator = 0);
    // dim
    ImMat(int w, int h, int c, size_t elemsize = 4u, Allocator* allocator = 0);
    // packed vec
    ImMat(int w, size_t elemsize, int elempack, Allocator* allocator = 0);
    // packed image
    ImMat(int w, int h, size_t elemsize, int elempack, Allocator* allocator = 0);
    // packed dim
    ImMat(int w, int h, int c, size_t elemsize, int elempack, Allocator* allocator = 0);
    // copy
    ImMat(const ImMat& m);
    // external vec
    ImMat(int w, void* data, size_t elemsize = 4u, Allocator* allocator = 0);
    // external image
    ImMat(int w, int h, void* data, size_t elemsize = 4u, Allocator* allocator = 0);
    // external dim
    ImMat(int w, int h, int c, void* data, size_t elemsize = 4u, Allocator* allocator = 0);
    // external packed vec
    ImMat(int w, void* data, size_t elemsize, int elempack, Allocator* allocator = 0);
    // external packed image
    ImMat(int w, int h, void* data, size_t elemsize, int elempack, Allocator* allocator = 0);
    // external packed dim
    ImMat(int w, int h, int c, void* data, size_t elemsize, int elempack, Allocator* allocator = 0);
    // release
    virtual ~ImMat();
    // assign
    ImMat& operator=(const ImMat& m);
    // allocate vec
    void create(int w, size_t elemsize = 4u, Allocator* allocator = 0);
    // allocate image
    void create(int w, int h, size_t elemsize = 4u, Allocator* allocator = 0);
    // allocate dim
    void create(int w, int h, int c, size_t elemsize = 4u, Allocator* allocator = 0);
    // allocate packed vec
    void create(int w, size_t elemsize, int elempack, Allocator* allocator = 0);
    // allocate packed image
    void create(int w, int h, size_t elemsize, int elempack, Allocator* allocator = 0);
    // allocate packed dim
    void create(int w, int h, int c, size_t elemsize, int elempack, Allocator* allocator = 0);
    // allocate vec with type
    void create_type(int w, ImDataType t = IM_DT_INT8, Allocator* allocator = 0);
    // allocate image with type
    void create_type(int w, int h, ImDataType t = IM_DT_INT8, Allocator* allocator = 0);
    // allocate dim with type
    void create_type(int w, int h, int c, ImDataType t = IM_DT_INT8, Allocator* allocator = 0);
    // extern vec with type
    void create_type(int w, void* data, ImDataType t = IM_DT_INT8, Allocator* allocator = 0);
    // extern image with type
    void create_type(int w, int h, void* data, ImDataType t = IM_DT_INT8, Allocator* allocator = 0);
    // extern dim with type
    void create_type(int w, int h, int c, void* data, ImDataType t = IM_DT_INT8, Allocator* allocator = 0);
    // allocate like
    void create_like(const ImMat& m, Allocator* allocator = 0);
    // set all
    template<typename T> void fill(T v);
    inline void fill(int8_t _v);
    inline void fill(int16_t _v);
    inline void fill(int32_t _v);
    inline void fill(int64_t _v);
    inline void fill(uint8_t _v);
    inline void fill(uint16_t _v);
    inline void fill(uint32_t _v);
    inline void fill(uint64_t _v);
    inline void fill(float _v);
    inline void fill(double _v);
    // scalar add
    template<typename T> ImMat operator+ (T v);
    template<typename T> ImMat& operator+= (T v);
    // scalar sub
    template<typename T> ImMat operator- (T v);
    template<typename T> ImMat& operator-= (T v);
    // scalar mul
    template<typename T> ImMat operator* (T v);
    template<typename T> ImMat& operator*= (T v);
    // scalar div
    template<typename T> ImMat operator/ (T v);
    template<typename T> ImMat& operator/= (T v);
    // deep copy
    ImMat clone(Allocator* allocator = 0) const;
    // deep copy from other buffer, inplace
    void clone_from(const ImMat& mat, Allocator* allocator = 0);
    // reshape vec
    ImMat reshape(int w, Allocator* allocator = 0) const;
    // reshape image
    ImMat reshape(int w, int h, Allocator* allocator = 0) const;
    // reshape dim
    ImMat reshape(int w, int h, int c, Allocator* allocator = 0) const;
    // transpose
    ImMat t() const;
    // determinant
    float determinant();
    // eye
    template<typename T> ImMat& eye(T scale);
    // invert dim = 2 only
    template<typename T> ImMat inv() const;
    // diag
    template<typename T> ImMat diag() const;
    // rand
    template<typename T> ImMat& randn(T mean, T stddev, int seed = -1);
    // clip
    template<typename T> ImMat& clip(T v_min, T v_max);
    // mat add
    ImMat operator+(const ImMat& mat);
    ImMat& operator+=(const ImMat& mat);
    // mat sub
    ImMat operator-(const ImMat& mat);
    ImMat& operator-=(const ImMat& mat);
    // mat div
    ImMat operator/(const ImMat& mat);
    ImMat& operator/=(const ImMat& mat);
    // mat mul
    ImMat& mul(const ImMat& mat);
    // mat square
    ImMat& square();
    // mat dot mul dims = 2 only
    ImMat operator*(const ImMat& mat);
    ImMat& operator*=(const ImMat& mat);
    // negative
    ImMat  operator-();
    // sum
    ImMat sum();
    // mean
    ImMat mean();
    // absdiff
    ImMat absdiff(const ImMat& m) const;
    // sqr
    ImMat sqr() const;
    // norm
    float norm(int norm_type = NORM_L2);
    // min/max
    template<typename T> void minmax(T* vmin, T* vmax, int* imin = nullptr, int* imax = nullptr);
    // normalize
    template<typename T> void normalize(T vmin, T vmax, int norm_type = NORM_MINMAX);
    // vconcat
    ImMat vconcat(const ImMat& m);
    // hconcat
    ImMat hconcat(const ImMat& m);
    // convert type
    ImMat convert(ImDataType t, float scale = 1.0f) const;
    // convert color
    IMMAT_API ImMat cvtToLAB() const;
    IMMAT_API ImMat cvtToGray() const;
    IMMAT_API ImMat cvtToHSV() const;
    IMMAT_API ImMat cvtToHSL() const;
    IMMAT_API ImMat cvtToRGB(ImColorFormat format = IM_CF_BGR, ImDataType dtype = IM_DT_UNDEFINED, bool planar = true) const;

    // some draw function only support 3 dims
    // mat default ordination is ncwh
    // if need using nwhc then we need set elempack as elemsize * c
    IMMAT_API void clean(ImPixel color);
    IMMAT_API void get_pixel(int x, int y, ImPixel& color) const;
    IMMAT_API void get_pixel(ImPoint p, ImPixel& color) const;
    IMMAT_API ImPixel get_pixel(int x, int y) const;
    IMMAT_API ImPixel get_pixel(ImPoint p) const;
    IMMAT_API void set_pixel(int x, int y, ImPixel color, bool norm = true);
    IMMAT_API void set_pixel(ImPoint p, ImPixel color, bool norm = true);
    IMMAT_API void alphablend(int x, int y, float alpha, ImPixel color);
    IMMAT_API void alphablend(int x, int y, ImPixel color);
    IMMAT_API void draw_line(float x1, float y1, float x2, float y2, float t, ImPixel color);
    IMMAT_API void draw_line(ImPoint p1, ImPoint p2, float t, ImPixel color);
    IMMAT_API void draw_line(float x1, float y1, float x2, float y2, ImPixel color, int weight = 1);
    IMMAT_API void draw_line(ImPoint p1, ImPoint p2, ImPixel color, int weight = 1);
    IMMAT_API void draw_rectangle(float x1, float y1, float x2, float y2, ImPixel color, int weight = 1);
    IMMAT_API void draw_rectangle(ImPoint p1, ImPoint p2, ImPixel color, int weight = 1);
    IMMAT_API void draw_rectangle(float x1, float y1, float x2, float y2, float t, ImPixel color);
    IMMAT_API void draw_rectangle(ImPoint p1, ImPoint p2, float t, ImPixel color);
    IMMAT_API void draw_circle(float x, float y, float r, ImPixel color);
    IMMAT_API void draw_circle(ImPoint p, float r, ImPixel color);
    IMMAT_API void draw_circle_filled(float x, float y, float r, ImPixel color);
    IMMAT_API void draw_circle_filled(ImPoint p, float r, ImPixel color);
    IMMAT_API void draw_circle(float x, float y, float r, float t, ImPixel color);
    IMMAT_API void draw_circle(ImPoint p, float r, float t, ImPixel color);
    IMMAT_API void draw_circle(float x, float y, float r, float t, std::function<ImPixel(float)> const &color);
    IMMAT_API void draw_circle(ImPoint p, float r, float t, std::function<ImPixel(float)> const &color);

    // simple filters for gray
    IMMAT_API ImMat lowpass(float lambda);
    IMMAT_API ImMat highpass(float lambda);
    IMMAT_API ImMat threshold(float thres);
    #define MORPH_FLAGS_LEFT    (1 << 0)
    #define MORPH_FLAGS_RIGHT   (1 << 1)
    #define MORPH_FLAGS_TOP     (1 << 2)
    #define MORPH_FLAGS_BOTTOM  (1 << 3)
    IMMAT_API ImMat dilate(int radius = 1, uint8_t flags = 0xFF);
    IMMAT_API ImMat erode(int radius = 1, uint8_t flags = 0xFF);

    // simple filters
    IMMAT_API ImMat blur(int kernel_size, float sigma = 1.0f, bool norm = true); // Gaussian Blur
    IMMAT_API ImMat adaptive_threshold(float maxValue, int kernel_size, float delta);
    IMMAT_API ImMat resize(float w, float h, ImInterpolateMode interpolate = IM_INTERPOLATE_BILINEAR, bool norm = true) const;
    IMMAT_API ImMat resize(ImPoint scale, ImInterpolateMode interpolate = IM_INTERPOLATE_BILINEAR, bool norm = true) const;

    // copy to
    IMMAT_API void copy_to(ImMat & mat, ImPoint offset = {}, float alpha = 1.0f);

    // crop
    IMMAT_API ImMat crop(ImPoint p1, ImPoint p2) const;
    IMMAT_API ImMat crop(ImBox box) const;

    // repeat
    IMMAT_API ImMat repeat(int nx, int ny);
    
    // release
    void release();

    bool empty() const;
    size_t total() const;

    // bits per element
    int elembits() const;

    // shape only
    ImMat shape() const;

    // data reference
    ImMat channel(int c);
    const ImMat channel(int c) const;

    template<typename T>
    inline T* row(int y) { return (T*)((unsigned char*)data + (size_t)w * y * elemsize); }
    template<typename T>
    inline const T* row(int y) const { return (const T*)((unsigned char*)data + (size_t)w * y * elemsize); }

    template<typename T>
    inline T* row_c(int y) { return (T*)((unsigned char*)data + (size_t)w * y * c * elemsize); }
    template<typename T>
    inline const T* row_c(int y) const { return (const T*)((unsigned char*)data + (size_t)w * y * c * elemsize); }

    // range reference
    ImMat channel_range(int c, int channels);
    const ImMat channel_range(int c, int channels) const;
    ImMat row_range(int y, int rows);
    const ImMat row_range(int y, int rows) const;
    ImMat range(int x, int n);
    const ImMat range(int x, int n) const;

    // access raw data
    template<typename T> operator T*();
    template<typename T> operator const T*() const;

    // access element data
    template<typename _Tp> _Tp& at(int i=0) 
    {
        assert(device == IM_DD_CPU && dims == 1);
        return *(_Tp*)((unsigned char*)data + i * elemsize); 
    };
    template<typename _Tp> const _Tp& at(int i=0) const 
    {
        assert(device == IM_DD_CPU && dims == 1);
        return *(const _Tp*)((unsigned char*)data + i * elemsize); 
    };
    template<typename _Tp> _Tp& at(int x, int y) 
    {
        assert(device == IM_DD_CPU && dims == 2);
        return *(_Tp*)((unsigned char*)data + (y * w + x) * elemsize); 
    };
    template<typename _Tp> const _Tp& at(int x, int y) const 
    {
        assert(device == IM_DD_CPU && dims == 2);
        return *(const _Tp*)((unsigned char*)data + (y * w + x) * elemsize); 
    };
    template<typename _Tp> _Tp& at(int x, int y, int _c) 
    {
        assert(device == IM_DD_CPU && dims == 3);
        if (elempack == 1)
            return *(_Tp*)((unsigned char*)data + _c * cstep * elemsize + (y * w + x) * elemsize); 
        else
            return *(_Tp*)((unsigned char*)data + (y * w + x) * elemsize * c + _c * elemsize); 
    };
    template<typename _Tp> const _Tp& at(int x, int y, int _c) const 
    {
        assert(device == IM_DD_CPU && dims == 3);
        if (elempack == 1)
            return *(const _Tp*)((unsigned char*)data + _c * cstep * elemsize + (y * w + x) * elemsize);
        else
            return *(const _Tp*)((unsigned char*)data + (y * w + x) * elemsize * c + _c * elemsize); 
    };

    // debug
    IMMAT_API void print(std::string name = {});
    IMMAT_API void print_shape(std::string name = {});

    // convenient access float vec element
    float& operator[](size_t i);
    const float& operator[](size_t i) const;

    // pointer to the data
    void* data;

    // element size in bytes
    // 8 = double/int64
    // 4 = float32/int32
    // 2 = float16/int16
    // 1 = int8/uint8
    // 0 = empty
    size_t elemsize;

    // packed count inside element
    // c/1-h-w-1  h/1-w-1  w/1-1  scalar
    // c/4-h-w-4  h/4-w-4  w/4-4  sse/neon
    // c/8-h-w-8  h/8-w-8  w/8-8  avx/fp16
    int elempack;

    // the allocator
    Allocator* allocator;

    // the dimension rank
    int dims;

    // mat data size
    int w;
    int h;
    int c;

    size_t cstep;
    
    // payload data size
    int dw;
    int dh;

    // data device
    // 0 = cpu
    // 1 = vulkan
    // 2 = cuda
    ImDataDevice device;

    // device number
    // 0 = cpu
    // 0 - n = gpu index
    int device_number;

    // time stamp
    double time_stamp;

    // audio sample index
    // video pts/frame index
    int64_t index_count;

    // duration
    double duration;

    // audio sample rate
    // video frame rate
    Rational rate;

    // depth
    // 8~16 for int 32 for float
    int depth;

    // type
    // 0 = INT8/UINT8
    // 1 = INT16/UINT16
    // 2 = INT32/UINT32
    // 3 = INT64/UINT64
    // 4 = FLOAT16
    // 5 = FLOAT32
    // 6 = FLOAT64
    ImDataType type;

    // color
    // 0 = SRGB
    // 1 = BT601
    // 2 = BT709
    // 3 = BT2020
    ImColorSpace color_space;

    // format
    //  0 = GRAY
    //  1 = BGR
    //  2 = ABGR
    //  3 = BGRA
    //  4 = RGB
    //  5 = ARGB
    //  6 = RGBA
    //  7 = YUV420
    //  8 = YUV422
    //  9 = YUV444
    // 10 = YUV440
    // 11 = YUVA
    // 12 = NV12
    // 13 = P010LE
    // 14 = LAB
    // 15 = HSV
    // 16 = HLS
    ImColorFormat color_format;

    // range
    // 0 = FULL_RANGE
    // 1 = NARROW_RANGE
    ImColorRange color_range;

    // flags, see define IM_MAT_FLAGS_XXX
    int flags;

    // ordination, see enum Ordination, default is NCWH
    Ordination ord;

    void copy_attribute(const ImMat& mat)
    {
        time_stamp = mat.time_stamp;
        rate = mat.rate;
        flags = mat.flags;
        color_range = mat.color_range;
        color_space = mat.color_space;
        ord = mat.ord;
        duration = mat.duration;
        index_count = mat.index_count;
    }

protected:
    virtual void allocate_buffer();

    class RefCount
    {
    public:
        bool addref()
        {
            std::lock_guard<std::mutex> lk(l);
            if (c > 0)
            {
                c++;
                return true;
            }
            return false;
        }

        bool relref()
        {
            std::lock_guard<std::mutex> lk(l);
            if (c > 0)
            {
                c--;
                if (c == 0)
                    return true;
            }
            return false;
        }
    private:
        std::mutex l;
        unsigned int c{1};
    };

    // pointer to the reference counter
    // when points to user-allocated data, the pointer is NULL
    std::shared_ptr<RefCount> refcount;
};

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
// ImMat class
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
inline ImMat::ImMat()
    : data(0), device(IM_DD_CPU), device_number(-1), elemsize(0), elempack(0), allocator(0), dims(0), w(0), h(0), c(0), cstep(0), dw(0), dh(0), time_stamp(NAN), index_count(-1), duration(NAN)
{
    type = IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = IM_CF_ABGR;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    depth = 32;
    ord = ORD_NCWH;
    rate = {0, 0};
}

inline ImMat::ImMat(int _w, size_t _elemsize, Allocator* _allocator)
    : data(0), device(IM_DD_CPU), device_number(-1), elemsize(0), elempack(0), allocator(0), dims(0), w(0), h(0), c(0), cstep(0), dw(0), dh(0), time_stamp(NAN), index_count(-1), duration(NAN)
{
    create(_w, _elemsize, _allocator);
}

inline ImMat::ImMat(int _w, int _h, size_t _elemsize, Allocator* _allocator)
    : data(0), device(IM_DD_CPU), device_number(-1), elemsize(0), elempack(0), allocator(0), dims(0), w(0), h(0), c(0), cstep(0), dw(0), dh(0), time_stamp(NAN), index_count(-1), duration(NAN)
{
    create(_w, _h, _elemsize, _allocator);
}

inline ImMat::ImMat(int _w, int _h, int _c, size_t _elemsize, Allocator* _allocator)
    : data(0), device(IM_DD_CPU), device_number(-1), elemsize(0), elempack(0), allocator(0), dims(0), w(0), h(0), c(0), cstep(0), dw(0), dh(0), time_stamp(NAN), index_count(-1), duration(NAN)
{
    create(_w, _h, _c, _elemsize, _allocator);
}

inline ImMat::ImMat(int _w, size_t _elemsize, int _elempack, Allocator* _allocator)
    : data(0), device(IM_DD_CPU), device_number(-1), elemsize(0), elempack(0), allocator(0), dims(0), w(0), h(0), c(0), cstep(0), dw(0), dh(0), time_stamp(NAN), index_count(-1), duration(NAN)
{
    create(_w, _elemsize, _elempack, _allocator);
}

inline ImMat::ImMat(int _w, int _h, size_t _elemsize, int _elempack, Allocator* _allocator)
    : data(0), device(IM_DD_CPU), device_number(-1), elemsize(0), elempack(0), allocator(0), dims(0), w(0), h(0), c(0), cstep(0), dw(0), dh(0), time_stamp(NAN), index_count(-1), duration(NAN)
{
    create(_w, _h, _elemsize, _elempack, _allocator);
}

inline ImMat::ImMat(int _w, int _h, int _c, size_t _elemsize, int _elempack, Allocator* _allocator)
    : data(0), device(IM_DD_CPU), device_number(-1), elemsize(0), elempack(0), allocator(0), dims(0), w(0), h(0), c(0), cstep(0), dw(0), dh(0), time_stamp(NAN), index_count(-1), duration(NAN)
{
    create(_w, _h, _c, _elemsize, _elempack, _allocator);
}

inline ImMat::ImMat(const ImMat& m)
    : data(m.data), device(m.device), device_number(m.device_number), refcount(m.refcount), elemsize(m.elemsize), elempack(m.elempack), allocator(m.allocator), dims(m.dims), w(m.w), h(m.h), c(m.c), cstep(m.cstep), dw(m.dw), dh(m.dh), time_stamp(m.time_stamp), index_count(m.index_count), duration(m.duration)
{
    cstep = m.cstep;
    type = m.type;
    color_format = m.color_format;
    color_space = m.color_space;
    color_range = m.color_range;
    flags = m.flags;
    rate = m.rate;
    depth = m.depth;
    ord = m.ord;

    if (refcount && !refcount->addref())
    {
        // if argument 'm' is already released, then create an empty ImMat.
        refcount = nullptr;
        data = nullptr;
        *this = ImMat();
    }
}

inline ImMat::ImMat(int _w, void* _data, size_t _elemsize, Allocator* _allocator)
    : data(_data), device(IM_DD_CPU), device_number(-1), elemsize(_elemsize), elempack(1), allocator(_allocator), dims(1), w(_w), h(1), c(1), dw(_w), dh(1), time_stamp(NAN), index_count(-1), duration(NAN)
{
    cstep = w;
    type = _elemsize == 1 ? IM_DT_INT8 : _elemsize == 2 ? IM_DT_INT16 : IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = IM_CF_GRAY;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    depth = _elemsize == 1 ? 8 : _elemsize == 2 ? 16 : 32;
}

inline ImMat::ImMat(int _w, int _h, void* _data, size_t _elemsize, Allocator* _allocator)
    : data(_data), device(IM_DD_CPU), device_number(-1), elemsize(_elemsize), elempack(1), allocator(_allocator), dims(2), w(_w), h(_h), c(1), dw(_w), dh(_h), time_stamp(NAN), index_count(-1), duration(NAN)
{
    cstep = (size_t)w * h;
    type = _elemsize == 1 ? IM_DT_INT8 : _elemsize == 2 ? IM_DT_INT16 : IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = IM_CF_GRAY;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    depth = _elemsize == 1 ? 8 : _elemsize == 2 ? 16 : 32;
}

inline ImMat::ImMat(int _w, int _h, int _c, void* _data, size_t _elemsize, Allocator* _allocator)
    : data(_data), device(IM_DD_CPU), device_number(-1), elemsize(_elemsize), elempack(1), allocator(_allocator), dims(3), w(_w), h(_h), c(_c), dw(_w), dh(_h), time_stamp(NAN), index_count(-1), duration(NAN)
{
    cstep = Im_AlignSize((size_t)w * h * elemsize, 16) / elemsize;
    type = _elemsize == 1 ? IM_DT_INT8 : _elemsize == 2 ? IM_DT_INT16 : IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = c == 1 ? IM_CF_GRAY : c == 3 ? IM_CF_BGR : IM_CF_ABGR;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    depth = _elemsize == 1 ? 8 : _elemsize == 2 ? 16 : 32;
}

inline ImMat::ImMat(int _w, void* _data, size_t _elemsize, int _elempack, Allocator* _allocator)
    : data(_data), device(IM_DD_CPU), device_number(-1), elemsize(_elemsize), elempack(_elempack), allocator(_allocator), dims(1), w(_w), h(1), c(1), dw(_w), dh(1), time_stamp(NAN), index_count(-1), duration(NAN)
{
    cstep = w;
    type = _elemsize == 1 ? IM_DT_INT8 : _elemsize == 2 ? IM_DT_INT16 : IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = IM_CF_GRAY;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    depth = _elemsize == 1 ? 8 : _elemsize == 2 ? 16 : 32;
}

inline ImMat::ImMat(int _w, int _h, void* _data, size_t _elemsize, int _elempack, Allocator* _allocator)
    : data(_data), device(IM_DD_CPU), device_number(-1), elemsize(_elemsize), elempack(_elempack), allocator(_allocator), dims(2), w(_w), h(_h), c(1), dw(_w), dh(_h), time_stamp(NAN), index_count(-1), duration(NAN)
{
    cstep = (size_t)w * h;
    type = _elemsize == 1 ? IM_DT_INT8 : _elemsize == 2 ? IM_DT_INT16 : IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = IM_CF_GRAY;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    depth = _elemsize == 1 ? 8 : _elemsize == 2 ? 16 : 32;
}

inline ImMat::ImMat(int _w, int _h, int _c, void* _data, size_t _elemsize, int _elempack, Allocator* _allocator)
    : data(_data), device(IM_DD_CPU), device_number(-1), elemsize(_elemsize), elempack(_elempack), allocator(_allocator), dims(3), w(_w), h(_h), c(_c), dw(_w), dh(_h), time_stamp(NAN), index_count(-1), duration(NAN)
{
    cstep = Im_AlignSize((size_t)w * h * elemsize, 16) / elemsize;
    type = _elemsize == 1 ? IM_DT_INT8 : _elemsize == 2 ? IM_DT_INT16 : IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = c == 1 ? IM_CF_GRAY : c == 3 ? IM_CF_BGR : IM_CF_ABGR;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = _elempack == _elemsize * _c ?  ORD_NWHC : ORD_NCWH;
    depth = _elemsize == 1 ? 8 : _elemsize == 2 ? 16 : 32;
}

inline ImMat::~ImMat()
{
    release();
}

inline ImMat& ImMat::operator=(const ImMat& m)
{
    if (this == &m)
        return *this;

    if (m.refcount && !m.refcount->addref())
        // if argument 'm' is already released, then do nothing
        return *this;

    release();

    data = m.data;
    refcount = m.refcount;
    elemsize = m.elemsize;
    elempack = m.elempack;
    allocator = m.allocator;

    dims = m.dims;
    w = m.w;
    h = m.h;
    c = m.c;
    dw = m.dw;
    dh = m.dh;

    cstep = m.cstep;

    type = m.type;
    color_space = m.color_space;
    color_format = m.color_format;
    color_range = m.color_range;
    flags = m.flags;
    rate = m.rate;
    depth = m.depth;
    ord = m.ord;

    device = m.device;
    device_number = m.device_number;
    time_stamp = m.time_stamp;
    duration = m.duration;
    index_count = m.index_count;
    return *this;
}

inline void ImMat::allocate_buffer()
{
    size_t totalsize = Im_AlignSize(total() * elemsize, 4);

    if (allocator)
        data = allocator->fastMalloc(totalsize, device);
    else
        data = Im_FastMalloc(totalsize);
    if (!data)
        return;

    refcount = std::make_shared<RefCount>();
}

inline void ImMat::create(int _w, size_t _elemsize, Allocator* _allocator)
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
}

inline void ImMat::create(int _w, int _h, size_t _elemsize, Allocator* _allocator)
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
}

inline void ImMat::create(int _w, int _h, int _c, size_t _elemsize, Allocator* _allocator)
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
}

inline void ImMat::create(int _w, size_t _elemsize, int _elempack, Allocator* _allocator)
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
}

inline void ImMat::create(int _w, int _h, size_t _elemsize, int _elempack, Allocator* _allocator)
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
}

inline void ImMat::create(int _w, int _h, int _c, size_t _elemsize, int _elempack, Allocator* _allocator)
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
}

inline void ImMat::create_type(int _w, ImDataType _t, Allocator* _allocator)
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
}

inline void ImMat::create_type(int _w, int _h, ImDataType _t, Allocator* _allocator)
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
}

inline void ImMat::create_type(int _w, int _h, int _c, ImDataType _t, Allocator* _allocator)
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
}

inline void ImMat::create_type(int _w, void* _data, ImDataType _t, Allocator* _allocator)
{
    if (dims == 1 && w == _w && elempack == 1 && type == _t && allocator == _allocator)
        return;

    release();

    elemsize = IM_ESIZE(_t);
    elempack = 1;
    allocator = _allocator;
    refcount = nullptr;

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
    data = _data;
}

inline void ImMat::create_type(int _w, int _h, void* _data, ImDataType _t, Allocator* _allocator)
{
    if (dims == 2 && w == _w && h == _h && elempack == 1 && type == _t && allocator == _allocator)
        return;

    release();

    elemsize = IM_ESIZE(_t);
    elempack = 1;
    allocator = _allocator;
    refcount = nullptr;

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
    data = _data;
}

inline void ImMat::create_type(int _w, int _h, int _c, void* _data, ImDataType _t, Allocator* _allocator)
{
    if (dims == 3 && w == _w && h == _h && c == _c && elempack == 1 && type == _t && allocator == _allocator)
        return;

    release();

    elemsize = IM_ESIZE(_t);
    elempack = 1;
    allocator = _allocator;
    refcount = nullptr;

    dims = _c == 1 ? 2 : 3;
    dw = w = _w;
    dh = h = _h;
    c = _c;

    cstep = Im_AlignSize((size_t)w * h * elemsize, 4) / elemsize;
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
    data = _data;
}

inline void ImMat::create_like(const ImMat& m, Allocator* _allocator)
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
    depth = m.depth;
    time_stamp = m.time_stamp;
    index_count = m.index_count;
    duration = m.duration;
}

inline void ImMat::release()
{
    if (refcount && refcount->relref())
    {
        if (allocator && data)
            allocator->fastFree(data, device);
        else if (data)
            Im_FastFree(data);
    }
    data = 0;
    refcount = nullptr;

    elemsize = 0;
    elempack = 0;

    dims = 0;
    dw = w = 0;
    dh = h = 0;
    c = 0;

    cstep = 0;

    type = IM_DT_FLOAT32;
    color_space = IM_CS_SRGB;
    color_format = IM_CF_ABGR;
    color_range = IM_CR_FULL_RANGE;
    flags = IM_MAT_FLAGS_NONE;
    rate = {0, 0};
    ord = ORD_NCWH;
    depth = 32;
    time_stamp = NAN;
    duration = NAN;
    index_count = -1;
    device = IM_DD_CPU;
    device_number = -1;
}

inline bool ImMat::empty() const
{
    return data == 0 || total() == 0;
}

inline size_t ImMat::total() const
{
    return cstep * c;
}

inline int ImMat::elembits() const
{
    return elempack ? static_cast<int>(elemsize * 8) / elempack : 0;
}

inline ImMat ImMat::shape() const
{
    if (dims == 1)
        return ImMat(w * elempack, (void*)0);
    if (dims == 2)
        return ImMat(w, h * elempack, (void*)0);
    if (dims == 3)
        return ImMat(w, h, c * elempack, (void*)0);

    return ImMat();
}

inline ImMat ImMat::channel(int _c)
{
    ImMat m(w, h, (unsigned char*)data + cstep * _c * elemsize, elemsize, elempack, allocator);
    m.dims = dims - 1;
    return m;
}

inline const ImMat ImMat::channel(int _c) const
{
    ImMat m(w, h, (unsigned char*)data + cstep * _c * elemsize, elemsize, elempack, allocator);
    m.dims = dims - 1;
    return m;
}

inline ImMat ImMat::channel_range(int _c, int channels)
{
    return ImMat(w, h, channels, (unsigned char*)data + cstep * _c * elemsize, elemsize, elempack, allocator);
}

inline const ImMat ImMat::channel_range(int _c, int channels) const
{
    return ImMat(w, h, channels, (unsigned char*)data + cstep * _c * elemsize, elemsize, elempack, allocator);
}

inline ImMat ImMat::row_range(int y, int rows)
{
    return ImMat(w, rows, (unsigned char*)data + (size_t)w * y * elemsize, elemsize, elempack, allocator);
}

inline const ImMat ImMat::row_range(int y, int rows) const
{
    return ImMat(w, rows, (unsigned char*)data + (size_t)w * y * elemsize, elemsize, elempack, allocator);
}

inline ImMat ImMat::range(int x, int n)
{
    return ImMat(n, (unsigned char*)data + x * elemsize, elemsize, elempack, allocator);
}

inline const ImMat ImMat::range(int x, int n) const
{
    return ImMat(n, (unsigned char*)data + x * elemsize, elemsize, elempack, allocator);
}

template<typename T>
inline ImMat::operator T*()
{
    return (T*)data;
}

template<typename T>
inline ImMat::operator const T*() const
{
    return (const T*)data;
}

inline float& ImMat::operator[](size_t i)
{
    return ((float*)data)[i];
}

inline const float& ImMat::operator[](size_t i) const
{
    return ((const float*)data)[i];
}

template<typename T>
inline void ImMat::fill(T _v)
{
    int size = (int)total() * elemsize / sizeof(T);
    T* ptr = (T*)data;
    for (int i = 0; i < size; i++)
    {
        ptr[i] = _v;
    }
}

inline void ImMat::fill(int8_t _v)
{
    int size = (int)total() * elemsize / sizeof(int8_t), i = 0;
    int8_t* ptr = (int8_t*)data;
#if __AVX__
    __m256i V = _mm256_set1_epi8(_v);
    for (i = 0; i < size - 31; i += 32) _mm256_storeu_si256((__m256i *)(ptr + i), V);
#elif __SSE__
    __m128i V = _mm_set1_epi8(_v);
    for (i = 0; i < size - 15; i += 16) _mm_storeu_si128((__m128i *)(ptr + i), V);
#elif __ARM_NEON 
    int8x16_t V = vdupq_n_s8(_v);
    for (i = 0; i < size - 15; i += 16) vst1q_s8((int8_t *)(ptr + i), V);
#endif
    for (; i < size; ++i) *(ptr + i) = _v;
}

inline void ImMat::fill(int16_t _v)
{
    int size = (int)total() * elemsize / sizeof(int16_t), i = 0;
    int16_t* ptr = (int16_t*)data;
#if __AVX__
    __m256i V = _mm256_set1_epi16(_v);
    for (i = 0; i < size - 15; i += 16) _mm256_storeu_si256((__m256i *)(ptr + i), V);
#elif __SSE__
    __m128i V = _mm_set1_epi16(_v);
    for (i = 0; i < size - 7; i += 8) _mm_storeu_si128((__m128i *)(ptr + i), V);
#elif __ARM_NEON 
    int16x8_t V = vdupq_n_s16(_v);
    for (i = 0; i < size - 7; i += 8) vst1q_s16((int16_t *)(ptr + i), V);
#endif
    for (; i < size; ++i) *(ptr + i) = _v;
}

inline void ImMat::fill(int32_t _v)
{
    int size = (int)total() * elemsize / sizeof(int32_t), i = 0;
    int32_t* ptr = (int32_t*)data;
#if __AVX__
    __m256i V = _mm256_set1_epi32(_v);
    for (i = 0; i < size - 7; i += 8) _mm256_storeu_si256((__m256i *)(ptr + i), V);
#elif __SSE__
    __m128i V = _mm_set1_epi32(_v);
    for (i = 0; i < size - 3; i += 4) _mm_storeu_si128((__m128i *)(ptr + i), V);
#elif __ARM_NEON 
    int32x4_t V = vdupq_n_s32(_v);
    for (i = 0; i < size - 3; i += 4) vst1q_s32((int32_t *)(ptr + i), V);
#endif
    for (; i < size; ++i) *(ptr + i) = _v;
}

inline void ImMat::fill(int64_t _v)
{
    int size = (int)total() * elemsize / sizeof(int64_t), i = 0;
    int64_t* ptr = (int64_t*)data;
#if __AVX__
    __m256i V = _mm256_set1_epi64x(_v);
    for (i = 0; i < size - 3; i += 4) _mm256_storeu_si256((__m256i *)(ptr + i), V);
#elif __SSE__
    __m128i V = _mm_set1_epi32(_v);
    for (i = 0; i < size - 1; i += 2) _mm_storeu_si128((__m128i *)(ptr + i), V);
#elif __ARM_NEON 
    int64x2_t V = vdupq_n_s64(_v);
    for (i = 0; i < size - 1; i += 2) vst1q_s64((int64_t *)(ptr + i), V);
#endif
    for (; i < size; ++i) *(ptr + i) = _v;
}

inline void ImMat::fill(uint8_t _v)
{
    fill((int8_t)_v);
}

inline void ImMat::fill(uint16_t _v)
{
    fill((int16_t)_v);
}

inline void ImMat::fill(uint32_t _v)
{
    fill((int32_t)_v);
}

inline void ImMat::fill(uint64_t _v)
{
    fill((int64_t)_v);
}

inline void ImMat::fill(float _v)
{
    int size = (int)total() * elemsize / sizeof(float), i = 0;
    float* ptr = (float*)data;
#if __AVX__
    __m256 V = _mm256_set1_ps(_v);
    for (i = 0; i < size - 7; i += 8) _mm256_storeu_ps(ptr + i, V);
#elif __SSE__
    __m128 V = _mm_set1_ps(_v);
    for (i = 0; i < size - 3; i += 4) _mm_storeu_ps(ptr + i, V);
#elif __ARM_NEON 
    float32x4_t V = vdupq_n_f32(_v);
    for (i = 0; i < size - 3; i += 4) vst1q_f32((float *)(ptr + i), V);
#endif
    for (; i < size; ++i) *(ptr + i) = _v;
}

inline void ImMat::fill(double _v)
{
    int size = (int)total() * elemsize / sizeof(double), i = 0;
    double* ptr = (double*)data;
#if __AVX__
    __m256d V = _mm256_set1_pd(_v);
    for (i = 0; i < size - 3; i += 4) _mm256_storeu_pd(ptr + i, V);
#elif __SSE__
    __m128d V = _mm_set1_pd(_v);
    for (i = 0; i < size - 1; i += 2) _mm_storeu_pd(ptr + i, V);
#endif
    for (; i < size; ++i) *(ptr + i) = _v;
}

inline ImMat ImMat::clone(Allocator* _allocator) const
{
    if (empty())
        return ImMat();

    ImMat m;
    if (dims == 1)
        m.create(w, elemsize, elempack, _allocator);
    else if (dims == 2)
        m.create(w, h, elemsize, elempack, _allocator);
    else if (dims == 3)
        m.create(w, h, c, elemsize, elempack, _allocator);

    if (total() > 0 && device == IM_DD_CPU)
    {
        // only copy date from CPU mat, if mat is GPU mat, please concert to CPU mat first
        if (cstep == m.cstep)
            memcpy(m.data, data, total() * elemsize);
        else
        {
            // copy by channel for differnet cstep
            size_t size = (size_t)w * h * elemsize;
            for (int i = 0; i < c; i++)
            {
                memcpy(m.channel(i), channel(i), size);
            }
        }
    }
    m.color_format = color_format;
    m.color_range = color_range;
    m.color_space = color_space;
    m.type = type;
    m.time_stamp = time_stamp;
    m.duration = duration;
    m.flags = flags;
    m.depth = depth;
    m.rate = rate;
    m.ord = ord;
    m.index_count = index_count;
    return m;
}

inline void ImMat::clone_from(const ImMat& mat, Allocator* allocator)
{
    *this = mat.clone(allocator);
}

inline ImMat ImMat::reshape(int _w, Allocator* _allocator) const
{
    if (w * h * c != _w)
        return ImMat();

    if (dims == 3 && cstep != (size_t)w * h)
    {
        ImMat m;
        m.create(_w, elemsize, elempack, _allocator);
        if (!m.data)
            return m;
        // flatten
        for (int i = 0; i < c; i++)
        {
            const void* ptr = (unsigned char*)data + i * cstep * elemsize;
            void* mptr = (unsigned char*)m.data + (size_t)i * w * h * elemsize;
            memcpy(mptr, ptr, (size_t)w * h * elemsize);
        }

        return m;
    }

    ImMat m = *this;

    m.dims = 1;
    m.dw = m.w = _w;
    m.dh = m.h = 1;
    m.c = 1;

    m.cstep = _w;
    m.color_format = IM_CF_GRAY;

    m.time_stamp = time_stamp;
    m.duration = duration;
    m.flags = flags;
    m.rate = rate;
    m.ord = ord;
    m.index_count = index_count;

    return m;
}

inline ImMat ImMat::reshape(int _w, int _h, Allocator* _allocator) const
{
    if (w * h * c != _w * _h)
        return ImMat();

    if (dims == 3 && cstep != (size_t)w * h)
    {
        ImMat m;
        m.create(_w, _h, elemsize, elempack, _allocator);

        // flatten
        for (int i = 0; i < c; i++)
        {
            const void* ptr = (unsigned char*)data + i * cstep * elemsize;
            void* mptr = (unsigned char*)m.data + (size_t)i * w * h * elemsize;
            memcpy(mptr, ptr, (size_t)w * h * elemsize);
        }

        return m;
    }

    ImMat m = *this;

    m.dims = 2;
    m.dw = m.w = _w;
    m.dh = m.h = _h;
    m.c = 1;
    m.color_format = IM_CF_GRAY;

    m.cstep = (size_t)_w * _h;

    m.time_stamp = time_stamp;
    m.duration = duration;
    m.flags = flags;
    m.rate = rate;
    m.ord = ord;
    m.index_count = index_count;

    return m;
}

inline ImMat ImMat::reshape(int _w, int _h, int _c, Allocator* _allocator) const
{
    if (w * h * c != _w * _h * _c)
        return ImMat();

    if (dims < 3)
    {
        if ((size_t)_w * _h != Im_AlignSize((size_t)_w * _h * elemsize, 16) / elemsize)
        {
            ImMat m;
            m.create(_w, _h, _c, elemsize, elempack, _allocator);

            // align channel
            for (int i = 0; i < _c; i++)
            {
                const void* ptr = (unsigned char*)data + (size_t)i * _w * _h * elemsize;
                void* mptr = (unsigned char*)m.data + i * m.cstep * m.elemsize;
                memcpy(mptr, ptr, (size_t)_w * _h * elemsize);
            }

            return m;
        }
    }
    else if (c != _c)
    {
        // flatten and then align
        ImMat tmp = reshape(_w * _h * _c, _allocator);
        return tmp.reshape(_w, _h, _c, _allocator);
    }

    ImMat m = *this;

    m.dims = _c == 1 ? 2 : 3;
    m.dw = m.w = _w;
    m.dh = m.h = _h;
    m.c = _c;
    m.color_format = _c == 1 ? IM_CF_GRAY : _c == 3 ? IM_CF_BGR : IM_CF_ABGR;
    m.cstep = Im_AlignSize((size_t)_w * _h * elemsize, 16) / elemsize;

    m.time_stamp = time_stamp;
    m.duration = duration;
    m.flags = flags;
    m.rate = rate;
    m.ord = ord;
    m.index_count = index_count;

    return m;
}

// transpose
inline ImMat ImMat::t() const
{
    assert(device == IM_DD_CPU);
    assert(total() > 0);
    if (dims == 1)
    {
        ImMat m;
        m.create_type(w, type, allocator);
        if (!m.data)
            return m;
        const void* ptr = (unsigned char*)data;
        void* mptr = (unsigned char*)m.data;
        memcpy(mptr, ptr, (size_t)w * h * elemsize);
        m.dw = m.w = 1;
        m.dh = m.h = w;

        return m;
    }
    else if (dims == 2)
    {
        ImMat m;
        m.create_type(h, w, type, allocator);
        if (!m.data)
            return m;
        for (int _h = 0; _h < h; _h++)
        {
            for (int _w = 0; _w < w; _w++)
            {
                switch (type)
                {
                    case IM_DT_INT8:    m.at< int8_t>(_h, _w) = this->at< int8_t>(_w, _h); break;
                    case IM_DT_INT16:   m.at<int16_t>(_h, _w) = this->at<int16_t>(_w, _h); break;
                    case IM_DT_INT32:   m.at<int32_t>(_h, _w) = this->at<int32_t>(_w, _h); break;
                    case IM_DT_INT64:   m.at<int64_t>(_h, _w) = this->at<int64_t>(_w, _h); break;
                    case IM_DT_FLOAT32: m.at<float>  (_h, _w) = this->at<float>  (_w, _h); break;
                    case IM_DT_FLOAT64: m.at<double> (_h, _w) = this->at<double> (_w, _h); break;
                    case IM_DT_FLOAT16: m.at<int16_t>(_h, _w) = this->at<int16_t>(_w, _h); break;
                    default: break;
                }
            }
        }
        return m;
    }
    else if (dims == 3)
    {
        ImMat m;
        m.create_type(h, w, c, type, allocator);
        if (!m.data)
            return m;
        
        for (int _c = 0; _c < c; _c++)
        {
            for (int _h = 0; _h < h; _h++)
            {
                for (int _w = 0; _w < w; _w++)
                {
                    switch (type)
                    {
                        case IM_DT_INT8:    m.at< int8_t>(_h, _w, _c) = this->at< int8_t>(_w, _h, _c); break;
                        case IM_DT_INT16:   m.at<int16_t>(_h, _w, _c) = this->at<int16_t>(_w, _h, _c); break;
                        case IM_DT_INT32:   m.at<int32_t>(_h, _w, _c) = this->at<int32_t>(_w, _h, _c); break;
                        case IM_DT_INT64:   m.at<int64_t>(_h, _w, _c) = this->at<int64_t>(_w, _h, _c); break;
                        case IM_DT_FLOAT32: m.at<float>  (_h, _w, _c) = this->at<float>  (_w, _h, _c); break;
                        case IM_DT_FLOAT64: m.at<double> (_h, _w, _c) = this->at<double> (_w, _h, _c); break;
                        case IM_DT_FLOAT16: m.at<int16_t>(_h, _w, _c) = this->at<int16_t>(_w, _h, _c); break;
                        default: break;
                    }
                }
            }
        }
        return m;
    }
    return ImMat();
}

// determinant
inline float ImMat::determinant()
{
    assert(device == IM_DD_CPU);
    assert(type == IM_DT_FLOAT32);
    assert(w == h);
    float result = 0;
    ImMat a;
    a.clone_from(*this);

    float * A = (float *)a.data;
    int p = 1, k;
    int astep = h;
    for(int i = 0; i < h; i++ )
    {
        k = i;
        for(int j = i + 1; j < h; j++ )
            if( std::abs(A[j * astep + i]) > std::abs(A[k * astep + i]) )
                k = j;
        if( std::abs(A[k * astep + i]) < FLT_EPSILON )
        {
            p = 0;
            break;
        }
        if( k != i )
        {
            for(int j = i; j < h; j++ )
                std::swap(A[i * astep + j], A[k * astep + j]);
            p = -p;
        }

        float d = -1 / A[i * astep + i];

        for(int j = i + 1; j < h; j++ )
        {
            float alpha = A[j * astep + i] * d;

            for( k = i + 1; k < h; k++ )
                A[j * astep + k] += alpha * A[i * astep + k];
        }
    }

    if(p)
    {
        result = p;
        for( int i = 0; i < h; i++ )
            result *= a.at<float>(i,i);
    }
    return result;
}

// invert
template<typename T>
inline ImMat ImMat::inv() const
{
    assert(device == IM_DD_CPU);
    assert(dims == 2 && w == h);
    assert(total() > 0);
    ImGui::ImMat inverse_mat, tmp_mat;
    inverse_mat.create_type(w, h, type);
    tmp_mat.clone_from(*this);
    inverse_mat.eye((T)1);
    T max, temp, k;
    for (int i = 0; i < w; i++)
	{
        max = tmp_mat.at<T>(i, i);
        k = i;
		for (int j = i + 1; j < w; j++)
		{
            if (std::abs(tmp_mat.at<T>(j, i)) > std::abs(max))
			{
				max = tmp_mat.at<T>(j, i);
				k = j;
			}
        }
        if (k != i)
		{
			for (int j = 0; j < w; j++)
			{
				temp = tmp_mat.at<T>(i, j);
				tmp_mat.at<T>(i, j) = tmp_mat.at<T>(k, j);
				tmp_mat.at<T>(k, j) = temp;

				temp = inverse_mat.at<T>(i, j);
				inverse_mat.at<T>(i, j) = inverse_mat.at<T>(k, j);
				inverse_mat.at<T>(k, j) = temp;
			}
		}
        if (tmp_mat.at<T>(i, i) == 0)
		{
            // There is no inverse matrix
            inverse_mat.fill((T)0);
            return inverse_mat;
        }
        temp = tmp_mat.at<T>(i, i);
		for (int j = 0; j < w; j++)
		{
			tmp_mat.at<T>(i, j) = tmp_mat.at<T>(i, j) / temp;
			inverse_mat.at<T>(i, j) = inverse_mat.at<T>(i, j) / temp;
		}
        for (int j = 0; j < w; j++)
		{
			if (j != i)
			{
				temp = tmp_mat.at<T>(j, i);
				for (int l = 0; l < w; l++)
				{
					tmp_mat.at<T>(j, l) = tmp_mat.at<T>(j, l) - tmp_mat.at<T>(i, l) * temp;
					inverse_mat.at<T>(j, l) = inverse_mat.at<T>(j, l) - inverse_mat.at<T>(i, l) * temp;
				}
			}
		}
    }
    return inverse_mat;
}

template<typename T> 
inline ImMat ImMat::diag() const
{
    assert(device == IM_DD_CPU);
    assert(dims <= 2);
    assert(total() > 0);
    ImGui::ImMat diag_mat;
    int dl = std::max(w, h);
    diag_mat.create_type(dl, dl, type);
    for(int i = 0; i < dl; i++)
    {
        if (dims == 2)
            diag_mat.at<T>(i,i) = at<T>(i, 0);
        else
            diag_mat.at<T>(i,i) = at<T>(i);
    }
    return diag_mat;
}

// eye
template<typename T> 
inline ImMat& ImMat::eye(T scale)
{
    assert(device == IM_DD_CPU);
    assert(total() > 0);
    if (dims == 1)
    {
        this->at<T>(0) = (T)scale;
    }
    else if (dims == 2)
    {
        for (int _h = 0; _h < h; _h++)
        {
            for (int _w = 0; _w < w; _w++)
            {
                this->at<T>(_w, _h) = _w == _h ? (T)scale : (T)0;
            }
        }
    }
    else if (dims == 3)
    {
        for (int _c = 0; _c < c; _c++)
        {
            for (int _h = 0; _h < h; _h++)
            {
                for (int _w = 0; _w < w; _w++)
                {
                    this->at<T>(_w, _h, _c) = _w == _h ? (T)scale : (T)0;
                }
            }
        }
    }
    return *this;
}

// rand
template<typename T> 
inline ImMat& ImMat::randn(T mean, T stddev, int seed)
{
    assert(device == IM_DD_CPU);
    assert(total() > 0);

    unsigned int useed = seed < 0 ? std::chrono::system_clock::now().time_since_epoch().count() : seed;
    std::default_random_engine gen(useed);
    std::normal_distribution<T> dis(mean, stddev);
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < total(); i++)
    {
        ((T *) this->data)[i] = (T)dis(gen);
    }
    return *this;
}

// clip
template<typename T> ImMat& ImMat::clip(T v_min, T v_max)
{
    assert(device == IM_DD_CPU);
    assert(total() > 0);
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < total(); i++)
    {
        if (((T *) this->data)[i] < (T)  v_min) ((T *) this->data)[i] = (T) v_min;
        if (((T *) this->data)[i] > (T)  v_max) ((T *) this->data)[i] = (T) v_max;
    }
    return *this;
}

// matrix math simd
// simd add
#if __AVX__
static inline __attribute__((unused)) void add_int8_avx(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    int i = 0;
    __m256i V = _mm256_set1_epi8(v);
    __m256i X;
    for (i = 0; i < (long)len - 31; i += 32)
    {
        X = _mm256_loadu_si256((__m256i const *)(src + i)); // load chunk of 32 char
        X = _mm256_add_epi8(X, V);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_int16_avx(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    int i = 0;
    __m256i V = _mm256_set1_epi16(v);
    __m256i X;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = _mm256_loadu_si256((__m256i const *)(src + i)); // load chunk of 16 short
        X = _mm256_add_epi16(X, V);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_int32_avx(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    int i = 0;
    __m256i V = _mm256_set1_epi32(v);
    __m256i X;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_si256((__m256i const *)(src + i)); // load chunk of 8 int
        X = _mm256_add_epi32(X, V);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_int64_avx(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    int i = 0;
    __m256i V = _mm256_set1_epi64x(v);
    __m256i X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm256_loadu_si256((__m256i const *)(src + i)); // load chunk of 4 int64
        X = _mm256_add_epi64(X, V);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_float_avx(float* dst, const float* src, const size_t len, const float v)
{
    int i = 0;
    __m256 V = _mm256_set1_ps(v);
    __m256 X;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_ps(src + i); // load chunk of 8 floats
        X = _mm256_add_ps(X, V);
        _mm256_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_double_avx(double* dst, const double* src, const size_t len, const double v)
{
    int i = 0;
    __m256d V = _mm256_set1_pd(v);
    __m256d X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm256_loadu_pd(src + i); // load chunk of 4 double
        X = _mm256_add_pd(X, V);
        _mm256_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_float16_avx(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) + v);
}
#define add_int8_simd add_int8_avx
#define add_int16_simd add_int16_avx
#define add_int32_simd add_int32_avx
#define add_int64_simd add_int64_avx
#define add_float_simd add_float_avx
#define add_double_simd add_double_avx
#define add_float16_simd add_float16_avx
#elif __SSE__
static inline __attribute__((unused)) void add_int8_sse(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    int i = 0;
    __m128i V = _mm_set1_epi8(v);
    __m128i X;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = _mm_loadu_si128((__m128i const *)(src + i)); // load chunk of 16 char
        X = _mm_add_epi8(X, V);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_int16_sse(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    int i = 0;
    __m128i V = _mm_set1_epi16(v);
    __m128i X;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm_loadu_si128((__m128i const *)(src + i)); // load chunk of 8 short
        X = _mm_add_epi16(X, V);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_int32_sse(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    int i = 0;
    __m128i V = _mm_set1_epi32(v);
    __m128i X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_si128((__m128i const *)(src + i)); // load chunk of 4 int
        X = _mm_add_epi32(X, V);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_int64_sse(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    int i = 0;
    __m128i V = _mm_set1_epi64x(v);
    __m128i X;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = _mm_loadu_si128((__m128i const *)(src + i)); // load chunk of 2 int64
        X = _mm_add_epi64(X, V);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_float_sse(float* dst, const float* src, const size_t len, const float v)
{
    int i = 0;
    __m128 V = _mm_set1_ps(v);
    __m128 X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_ps(src + i); // load chunk of 4 floats
        X = _mm_add_ps(X, V);
        _mm_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_double_sse(double* dst, const double* src, const size_t len, const double v)
{
    int i = 0;
    __m128d V = _mm_set1_pd(v);
    __m128d X;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = _mm_loadu_pd(src + i); // load chunk of 2 double
        X = _mm_add_pd(X, V);
        _mm_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_float16_sse(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) + v);
}
#define add_int8_simd add_int8_sse
#define add_int16_simd add_int16_sse
#define add_int32_simd add_int32_sse
#define add_int64_simd add_int64_sse
#define add_float_simd add_float_sse
#define add_double_simd add_double_sse
#define add_float16_simd add_float16_sse
#elif __ARM_NEON
static inline __attribute__((unused)) void add_int8_neon(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    int i = 0;
    int8x16_t V = vdupq_n_u8(v);
    int8x16_t X;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = vld1q_u8(src + i); // load chunk of 16 char
        X = vaddq_u8(X, V);
        vst1q_u8(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_int16_neon(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    int i = 0;
    int16x8_t V = vdupq_n_u16(v);
    int16x8_t X;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = vld1q_u16(src + i); // load chunk of 8 short
        X = vaddq_u16(X, V);
        vst1q_u16(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_int32_neon(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    int i = 0;
    int32x4_t V = vdupq_n_s32(v);
    int32x4_t X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = vld1q_s32(src + i); // load chunk of 4 int
        X = vaddq_s32(X, V);
        vst1q_s32(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_int64_neon(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    int i = 0;
    int64x2_t V = vdupq_n_s64(v);
    int64x2_t X;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = vld1q_s64(src + i); // load chunk of 2 int64
        X = vaddq_s64(X, V);
        vst1q_s64(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_float_neon(float* dst, const float* src, const size_t len, const float v)
{
    int i = 0;
    float32x4_t V = vdupq_n_f32(v);
    float32x4_t X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = vld1q_f32(src + i); // load chunk of 4 floats
        X = vaddq_f32(X, V);
        vst1q_f32(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_double_neon(double* dst, const double* src, const size_t len, const double v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_float16_neon(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) + v);
}
#define add_int8_simd add_int8_neon
#define add_int16_simd add_int16_neon
#define add_int32_simd add_int32_neon
#define add_int64_simd add_int64_neon
#define add_float_simd add_float_neon
#define add_double_simd add_double_neon
#define add_float16_simd add_float16_neon
#else
static inline __attribute__((unused)) void add_int8_c(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_int16_c(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_int32_c(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_int64_c(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_float_c(float* dst, const float* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_double_c(double* dst, const double* src, const size_t len, const double v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) + v;
}
static inline __attribute__((unused)) void add_float16_c(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) + v);
}
#define add_int8_simd add_int8_c
#define add_int16_simd add_int16_c
#define add_int32_simd add_int32_c
#define add_int64_simd add_int64_c
#define add_float_simd add_float_c
#define add_double_simd add_double_c
#define add_float16_simd add_float16_c
#endif

// simd sub
#if __AVX__
static inline __attribute__((unused)) void sub_int8_avx(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    int i = 0;
    __m256i V = _mm256_set1_epi8(v);
    __m256i X;
    for (i = 0; i < (long)len - 31; i += 32)
    {
        X = _mm256_loadu_si256((__m256i const *)(src + i)); // load chunk of 32 char
        X = _mm256_subs_epu8(X, V);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_int16_avx(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    int i = 0;
    __m256i V = _mm256_set1_epi16(v);
    __m256i X;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = _mm256_loadu_si256((__m256i const *)(src + i)); // load chunk of 16 short
        X = _mm256_subs_epu16(X, V);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_int32_avx(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    int i = 0;
    __m256i V = _mm256_set1_epi32(v);
    __m256i X;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_si256((__m256i const *)(src + i)); // load chunk of 8 int
        X = _mm256_sub_epi32(X, V);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_int64_avx(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    int i = 0;
    __m256i V = _mm256_set1_epi64x(v);
    __m256i X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm256_loadu_si256((__m256i const *)(src + i)); // load chunk of 4 int64
        X = _mm256_sub_epi64(X, V);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_float_avx(float* dst, const float* src, const size_t len, const float v)
{
    int i = 0;
    __m256 V = _mm256_set1_ps(v);
    __m256 X;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_ps(src + i); // load chunk of 8 floats
        X = _mm256_sub_ps(X, V);
        _mm256_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_double_avx(double* dst, const double* src, const size_t len, const double v)
{
    int i = 0;
    __m256d V = _mm256_set1_pd(v);
    __m256d X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm256_loadu_pd(src + i); // load chunk of 4 double
        X = _mm256_sub_pd(X, V);
        _mm256_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_float16_avx(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) - v);
}
#define sub_int8_simd sub_int8_avx
#define sub_int16_simd sub_int16_avx
#define sub_int32_simd sub_int32_avx
#define sub_int64_simd sub_int64_avx
#define sub_float_simd sub_float_avx
#define sub_double_simd sub_double_avx
#define sub_float16_simd sub_float16_avx
#elif __SSE__
static inline __attribute__((unused)) void sub_int8_sse(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    int i = 0;
    __m128i V = _mm_set1_epi8(v);
    __m128i X;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = _mm_loadu_si128((__m128i const *)(src + i)); // load chunk of 16 char
        X = _mm_subs_epi8(X, V);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_int16_sse(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    int i = 0;
    __m128i V = _mm_set1_epi16(v);
    __m128i X;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm_loadu_si128((__m128i const *)(src + i)); // load chunk of 8 short
        X = _mm_subs_epi16(X, V);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_int32_sse(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    int i = 0;
    __m128i V = _mm_set1_epi32(v);
    __m128i X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_si128((__m128i const *)(src + i)); // load chunk of 4 int
        X = _mm_sub_epi32(X, V);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_int64_sse(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    int i = 0;
    __m128i V = _mm_set1_epi64x(v);
    __m128i X;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = _mm_loadu_si128((__m128i const *)(src + i)); // load chunk of 2 int64
        X = _mm_sub_epi64(X, V);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_float_sse(float* dst, const float* src, const size_t len, const float v)
{
    int i = 0;
    __m128 V = _mm_set1_ps(v);
    __m128 X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_ps(src + i); // load chunk of 4 floats
        X = _mm_sub_ps(X, V);
        _mm_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_double_sse(double* dst, const double* src, const size_t len, const double v)
{
    int i = 0;
    __m128d V = _mm_set1_pd(v);
    __m128d X;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = _mm_loadu_pd(src + i); // load chunk of 2 double
        X = _mm_sub_pd(X, V);
        _mm_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_float16_sse(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) - v);
}
#define sub_int8_simd sub_int8_sse
#define sub_int16_simd sub_int16_sse
#define sub_int32_simd sub_int32_sse
#define sub_int64_simd sub_int64_sse
#define sub_float_simd sub_float_sse
#define sub_double_simd sub_double_sse
#define sub_float16_simd sub_float16_sse
#elif __ARM_NEON
static inline __attribute__((unused)) void sub_int8_neon(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    int i = 0;
    int8x16_t V = vdupq_n_u8(v);
    int8x16_t X;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = vld1q_u8(src + i); // load chunk of 16 char
        X = vqsubq_u8(X, V);
        vst1q_u8(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_int16_neon(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    int i = 0;
    int16x8_t V = vdupq_n_u16(v);
    int16x8_t X;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = vld1q_u16(src + i); // load chunk of 8 short
        X = vqsubq_u16(X, V);
        vst1q_u16(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_int32_neon(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    int i = 0;
    int32x4_t V = vdupq_n_s32(v);
    int32x4_t X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = vld1q_s32(src + i); // load chunk of 4 int
        X = vsubq_s32(X, V);
        vst1q_s32(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_int64_neon(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    int i = 0;
    int64x2_t V = vdupq_n_s64(v);
    int64x2_t X;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = vld1q_s64(src + i); // load chunk of 2 int64
        X = vsubq_s64(X, V);
        vst1q_s64(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_float_neon(float* dst, const float* src, const size_t len, const float v)
{
    int i = 0;
    float32x4_t V = vdupq_n_f32(v);
    float32x4_t X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = vld1q_f32(src + i); // load chunk of 4 floats
        X = vsubq_f32(X, V);
        vst1q_f32(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_double_neon(double* dst, const double* src, const size_t len, const double v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_float16_neon(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) - v);
}
#define sub_int8_simd sub_int8_neon
#define sub_int16_simd sub_int16_neon
#define sub_int32_simd sub_int32_neon
#define sub_int64_simd sub_int64_neon
#define sub_float_simd sub_float_neon
#define sub_double_simd sub_double_neon
#define sub_float16_simd sub_float16_neon
#else
static inline __attribute__((unused)) void sub_int8_c(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_int16_c(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_int32_c(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_int64_c(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_float_c(float* dst, const float* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_double_c(double* dst, const double* src, const size_t len, const double v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void sub_float16_c(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) - v);
}
#define sub_int8_simd sub_int8_c
#define sub_int16_simd sub_int16_c
#define sub_int32_simd sub_int32_c
#define sub_int64_simd sub_int64_c
#define sub_float_simd sub_float_c
#define sub_double_simd sub_double_c
#define sub_float16_simd sub_float16_c
#endif

// simd mul
#if __AVX__
static inline __attribute__((unused)) void mul_int8_avx(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    // TODO::Dicky need optimize int8 mul for avx
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_int16_avx(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    int i = 0;
    __m256i V = _mm256_set1_epi16(v);
    __m256i X;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = _mm256_loadu_si256((__m256i const *)(src + i)); // load chunk of 16 short
        X = _mm256_mullo_epi16(X, V);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_int32_avx(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    int i = 0;
    __m256i V = _mm256_set1_epi32(v);
    __m256i X;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_si256((__m256i const *)(src + i)); // load chunk of 8 int
        X = _mm256_mul_epi32(X, V);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_int64_avx(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    // TODO::Dicky need optimize mul int64 for avc
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_float_avx(float* dst, const float* src, const size_t len, const float v)
{
    int i = 0;
    __m256 V = _mm256_set1_ps(v);
    __m256 X;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_ps(src + i); // load chunk of 8 floats
        X = _mm256_mul_ps(X, V);
        _mm256_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_double_avx(double* dst, const double* src, const size_t len, const double v)
{
    int i = 0;
    __m256d V = _mm256_set1_pd(v);
    __m256d X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm256_loadu_pd(src + i); // load chunk of 4 double
        X = _mm256_mul_pd(X, V);
        _mm256_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_float16_avx(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) * v);
}
#define mul_int8_simd mul_int8_avx
#define mul_int16_simd mul_int16_avx
#define mul_int32_simd mul_int32_avx
#define mul_int64_simd mul_int64_avx
#define mul_float_simd mul_float_avx
#define mul_double_simd mul_double_avx
#define mul_float16_simd mul_float16_avx
#elif __SSE__
static inline __attribute__((unused)) void mul_int8_sse(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    // TODO::Dicky need optimize nul int8 for sse
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_int16_sse(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    int i = 0;
    __m128i V = _mm_set1_epi16(v);
    __m128i X;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm_loadu_si128((__m128i const *)(src + i)); // load chunk of 8 short
        X = _mm_mullo_epi16(X, V);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_int32_sse(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    int i = 0;
    __m128i V = _mm_set1_epi32(v);
    __m128i X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_si128((__m128i const *)(src + i)); // load chunk of 4 int
        X = _mm_mul_epi32(X, V);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_int64_sse(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    // TODO::Dicky need optimize mul int64 for sse
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_float_sse(float* dst, const float* src, const size_t len, const float v)
{
    int i = 0;
    __m128 V = _mm_set1_ps(v);
    __m128 X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_ps(src + i); // load chunk of 4 floats
        X = _mm_mul_ps(X, V);
        _mm_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_double_sse(double* dst, const double* src, const size_t len, const double v)
{
    int i = 0;
    __m128d V = _mm_set1_pd(v);
    __m128d X;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = _mm_loadu_pd(src + i); // load chunk of 2 double
        X = _mm_mul_pd(X, V);
        _mm_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_float16_sse(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) * v);
}
#define mul_int8_simd mul_int8_sse
#define mul_int16_simd mul_int16_sse
#define mul_int32_simd mul_int32_sse
#define mul_int64_simd mul_int64_sse
#define mul_float_simd mul_float_sse
#define mul_double_simd mul_double_sse
#define mul_float16_simd mul_float16_sse
#elif __ARM_NEON
static inline __attribute__((unused)) void mul_int8_neon(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    int i = 0;
    int8x16_t V = vdupq_n_u8(v);
    int8x16_t X;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = vld1q_u8(src + i); // load chunk of 16 char
        X = vmulq_u8(X, V);
        vst1q_u8(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) - v;
}
static inline __attribute__((unused)) void mul_int16_neon(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    int i = 0;
    int16x8_t V = vdupq_n_u16(v);
    int16x8_t X;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = vld1q_u16(src + i); // load chunk of 8 short
        X = vmulq_u16(X, V);
        vst1q_u16(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_int32_neon(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    int i = 0;
    int32x4_t V = vdupq_n_s32(v);
    int32x4_t X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = vld1q_s32(src + i); // load chunk of 4 int
        X = vmulq_s32(X, V);
        vst1q_s32(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_int64_neon(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_float_neon(float* dst, const float* src, const size_t len, const float v)
{
    int i = 0;
    float32x4_t V = vdupq_n_f32(v);
    float32x4_t X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = vld1q_f32(src + i); // load chunk of 4 floats
        X = vmulq_f32(X, V);
        vst1q_f32(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_double_neon(double* dst, const double* src, const size_t len, const double v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_float16_neon(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) * v);
}
#define mul_int8_simd mul_int8_neon
#define mul_int16_simd mul_int16_neon
#define mul_int32_simd mul_int32_neon
#define mul_int64_simd mul_int64_neon
#define mul_float_simd mul_float_neon
#define mul_double_simd mul_double_neon
#define mul_float16_simd mul_float16_neon
#else
static inline __attribute__((unused)) void mul_int8_c(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_int16_c(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_int32_c(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_int64_c(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_float_c(float* dst, const float* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_double_c(double* dst, const double* src, const size_t len, const double v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) * v;
}
static inline __attribute__((unused)) void mul_float16_c(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) * v);
}
#define mul_int8_simd mul_int8_c
#define mul_int16_simd mul_int16_c
#define mul_int32_simd mul_int32_c
#define mul_int64_simd mul_int64_c
#define mul_float_simd mul_float_c
#define mul_double_simd mul_double_c
#define mul_float16_simd mul_float16_c
#endif

// simd div
#if __AVX__
static inline __attribute__((unused)) void div_int8_avx(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_int16_avx(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_int32_avx(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_int64_avx(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_float_avx(float* dst, const float* src, const size_t len, const float v)
{
    int i = 0;
    __m256 V = _mm256_set1_ps(v);
    __m256 X;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_ps(src + i); // load chunk of 8 floats
        X = _mm256_div_ps(X, V);
        _mm256_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_double_avx(double* dst, const double* src, const size_t len, const double v)
{
    int i = 0;
    __m256d V = _mm256_set1_pd(v);
    __m256d X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm256_loadu_pd(src + i); // load chunk of 4 double
        X = _mm256_div_pd(X, V);
        _mm256_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_float16_avx(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) / v);
}
#define div_int8_simd div_int8_avx
#define div_int16_simd div_int16_avx
#define div_int32_simd div_int32_avx
#define div_int64_simd div_int64_avx
#define div_float_simd div_float_avx
#define div_double_simd div_double_avx
#define div_float16_simd div_float16_avx
#elif __SSE__
static inline __attribute__((unused)) void div_int8_sse(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_int16_sse(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_int32_sse(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_int64_sse(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_float_sse(float* dst, const float* src, const size_t len, const float v)
{
    int i = 0;
    __m128 V = _mm_set1_ps(v);
    __m128 X;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_ps(src + i); // load chunk of 4 floats
        X = _mm_div_ps(X, V);
        _mm_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_double_sse(double* dst, const double* src, const size_t len, const double v)
{
    int i = 0;
    __m128d V = _mm_set1_pd(v);
    __m128d X;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = _mm_loadu_pd(src + i); // load chunk of 2 double
        X = _mm_div_pd(X, V);
        _mm_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_float16_sse(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) / v);
}
#define div_int8_simd div_int8_sse
#define div_int16_simd div_int16_sse
#define div_int32_simd div_int32_sse
#define div_int64_simd div_int64_sse
#define div_float_simd div_float_sse
#define div_double_simd div_double_sse
#define div_float16_simd div_float16_sse
#else
static inline __attribute__((unused)) void div_int8_c(uint8_t* dst, const uint8_t* src, const size_t len, const uint8_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_int16_c(uint16_t* dst, const uint16_t* src, const size_t len, const uint16_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_int32_c(int32_t* dst, const int32_t* src, const size_t len, const int32_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_int64_c(int64_t* dst, const int64_t* src, const size_t len, const int64_t v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_float_c(float* dst, const float* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_double_c(double* dst, const double* src, const size_t len, const double v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src + i) / v;
}
static inline __attribute__((unused)) void div_float16_c(uint16_t* dst, const uint16_t* src, const size_t len, const float v)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src + i)) / v);
}
#define div_int8_simd div_int8_c
#define div_int16_simd div_int16_c
#define div_int32_simd div_int32_c
#define div_int64_simd div_int64_c
#define div_float_simd div_float_c
#define div_double_simd div_double_c
#define div_float16_simd div_float16_c
#endif

// simd add mat
#if __AVX__
static inline __attribute__((unused)) void madd_int8_avx(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    int i = 0;
    __m256i X, Y;
    for (i = 0; i < (long)len - 31; i += 32)
    {
        X = _mm256_loadu_si256((__m256i const *)(src1 + i)); // load chunk of 32 char
        Y = _mm256_loadu_si256((__m256i const *)(src2 + i)); // load chunk of 32 char
        X = _mm256_add_epi8(X, Y);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_int16_avx(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    int i = 0;
    __m256i X, Y;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = _mm256_loadu_si256((__m256i const *)(src1 + i)); // load chunk of 16 short
        Y = _mm256_loadu_si256((__m256i const *)(src2 + i)); // load chunk of 16 short
        X = _mm256_add_epi16(X, Y);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_int32_avx(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    int i = 0;
    __m256i X, Y;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_si256((__m256i const *)(src1 + i)); // load chunk of 8 int
        Y = _mm256_loadu_si256((__m256i const *)(src2 + i)); // load chunk of 8 int
        X = _mm256_add_epi32(X, Y);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_int64_avx(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    int i = 0;
    __m256i X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm256_loadu_si256((__m256i const *)(src1 + i)); // load chunk of 4 int64
        Y = _mm256_loadu_si256((__m256i const *)(src2 + i)); // load chunk of 4 int64
        X = _mm256_add_epi64(X, Y);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_float_avx(float* dst, const float* src1, const float* src2, const size_t len)
{
    int i = 0;
    __m256 X, Y;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_ps(src1 + i); // load chunk of 8 floats
        Y = _mm256_loadu_ps(src2 + i); // load chunk of 8 floats
        X = _mm256_add_ps(X, Y);
        _mm256_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_double_avx(double* dst, const double* src1, const double* src2, const size_t len)
{
    int i = 0;
    __m256d X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm256_loadu_pd(src1 + i); // load chunk of 4 double
        Y = _mm256_loadu_pd(src2 + i); // load chunk of 4 double
        X = _mm256_add_pd(X, Y);
        _mm256_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_float16_avx(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) + im_float16_to_float32(*(src2 + i)));
}
#define madd_int8_simd      madd_int8_avx
#define madd_int16_simd     madd_int16_avx
#define madd_int32_simd     madd_int32_avx
#define madd_int64_simd     madd_int64_avx
#define madd_float_simd     madd_float_avx
#define madd_double_simd    madd_double_avx
#define madd_float16_simd   madd_float16_avx
#elif __SSE__
static inline __attribute__((unused)) void madd_int8_sse(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    int i = 0;
    __m128i X, Y;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = _mm_loadu_si128((__m128i const *)(src1 + i)); // load chunk of 16 char
        Y = _mm_loadu_si128((__m128i const *)(src2 + i)); // load chunk of 16 char
        X = _mm_add_epi8(X, Y);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_int16_sse(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    int i = 0;
    __m128i X, Y;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm_loadu_si128((__m128i const *)(src1 + i)); // load chunk of 8 short
        Y = _mm_loadu_si128((__m128i const *)(src2 + i)); // load chunk of 8 short
        X = _mm_add_epi16(X, Y);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_int32_sse(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    int i = 0;
    __m128i X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_si128((__m128i const *)(src1 + i)); // load chunk of 4 int
        Y = _mm_loadu_si128((__m128i const *)(src2 + i)); // load chunk of 4 int
        X = _mm_add_epi32(X, Y);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_int64_sse(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    int i = 0;
    __m128i X, Y;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = _mm_loadu_si128((__m128i const *)(src1 + i)); // load chunk of 2 int64
        Y = _mm_loadu_si128((__m128i const *)(src2 + i)); // load chunk of 2 int64
        X = _mm_add_epi64(X, Y);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_float_sse(float* dst, const float* src1, const float* src2, const size_t len)
{
    int i = 0;
    __m128 X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_ps(src1 + i); // load chunk of 4 floats
        Y = _mm_loadu_ps(src2 + i); // load chunk of 4 floats
        X = _mm_add_ps(X, Y);
        _mm_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_double_sse(double* dst, const double* src1, const double* src2, const size_t len)
{
    int i = 0;
    __m128d X, Y;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = _mm_loadu_pd(src1 + i); // load chunk of 2 double
        Y = _mm_loadu_pd(src2 + i); // load chunk of 2 double
        X = _mm_add_pd(X, Y);
        _mm_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_float16_sse(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) + im_float16_to_float32(*(src2 + i)));
}
#define madd_int8_simd      madd_int8_sse
#define madd_int16_simd     madd_int16_sse
#define madd_int32_simd     madd_int32_sse
#define madd_int64_simd     madd_int64_sse
#define madd_float_simd     madd_float_sse
#define madd_double_simd    madd_double_sse
#define madd_float16_simd   madd_float16_sse
#elif __ARM_NEON
static inline __attribute__((unused)) void madd_int8_neon(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    int i = 0;
    int8x16_t X, Y;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = vld1q_u8(src1 + i); // load chunk of 16 char
        Y = vld1q_u8(src2 + i); // load chunk of 16 char
        X = vaddq_u8(X, Y);
        vst1q_u8(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_int16_neon(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    int i = 0;
    int16x8_t X, Y;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = vld1q_u16(src1 + i); // load chunk of 8 short
        Y = vld1q_u16(src2 + i); // load chunk of 8 short
        X = vaddq_u16(X, Y);
        vst1q_u16(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_int32_neon(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    int i = 0;
    int32x4_t X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = vld1q_s32(src1 + i); // load chunk of 4 int
        Y = vld1q_s32(src2 + i); // load chunk of 4 int
        X = vaddq_s32(X, Y);
        vst1q_s32(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_int64_neon(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    int i = 0;
    int64x2_t X, Y;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = vld1q_s64(src1 + i); // load chunk of 2 int64
        Y = vld1q_s64(src2 + i); // load chunk of 2 int64
        X = vaddq_s64(X, Y);
        vst1q_s64(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_float_neon(float* dst, const float* src1, const float* src2, const size_t len)
{
    int i = 0;
    float32x4_t X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = vld1q_f32(src1 + i); // load chunk of 4 floats
        Y = vld1q_f32(src2 + i); // load chunk of 4 floats
        X = vaddq_f32(X, Y);
        vst1q_f32(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_double_neon(double* dst, const double* src1, const double* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_float16_neon(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) + im_float16_to_float32(*(src2 + i)));
}
#define madd_int8_simd      madd_int8_neon
#define madd_int16_simd     madd_int16_neon
#define madd_int32_simd     madd_int32_neon
#define madd_int64_simd     madd_int64_neon
#define madd_float_simd     madd_float_neon
#define madd_double_simd    madd_double_neon
#define madd_float16_simd   madd_float16_neon
#else
static inline __attribute__((unused)) void madd_int8_c(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_int16_c(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_int32_c(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_int64_c(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_float_c(float* dst, const float* src1, const float* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_double_c(double* dst, const double* src1, const double* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) + *(src2 + i);
}
static inline __attribute__((unused)) void madd_float16_c(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) + im_float16_to_float32(*(src2 + i)));
}
#define madd_int8_simd      madd_int8_c
#define madd_int16_simd     madd_int16_c
#define madd_int32_simd     madd_int32_c
#define madd_int64_simd     madd_int64_c
#define madd_float_simd     madd_float_c
#define madd_double_simd    madd_double_c
#define madd_float16_simd   madd_float16_c
#endif

// simd sub mat
#if __AVX__
static inline __attribute__((unused)) void msub_int8_avx(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    int i = 0;
    __m256i X, Y;
    for (i = 0; i < (long)len - 31; i += 32)
    {
        X = _mm256_loadu_si256((__m256i const *)(src1 + i)); // load chunk of 32 char
        Y = _mm256_loadu_si256((__m256i const *)(src2 + i)); // load chunk of 32 char
        X = _mm256_sub_epi8(X, Y); // signed?
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_int16_avx(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    int i = 0;
    __m256i X, Y;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = _mm256_loadu_si256((__m256i const *)(src1 + i)); // load chunk of 16 short
        Y = _mm256_loadu_si256((__m256i const *)(src2 + i)); // load chunk of 16 short
        X = _mm256_sub_epi16(X, Y); // signed?
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_int32_avx(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    int i = 0;
    __m256i X, Y;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_si256((__m256i const *)(src1 + i)); // load chunk of 8 int
        Y = _mm256_loadu_si256((__m256i const *)(src2 + i)); // load chunk of 8 int
        X = _mm256_sub_epi32(X, Y);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_int64_avx(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    int i = 0;
    __m256i X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm256_loadu_si256((__m256i const *)(src1 + i)); // load chunk of 4 int64
        Y = _mm256_loadu_si256((__m256i const *)(src2 + i)); // load chunk of 4 int64
        X = _mm256_sub_epi64(X, Y);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_float_avx(float* dst, const float* src1, const float* src2, const size_t len)
{
    int i = 0;
    __m256 X, Y;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_ps(src1 + i); // load chunk of 8 floats
        Y = _mm256_loadu_ps(src2 + i); // load chunk of 8 floats
        X = _mm256_sub_ps(X, Y);
        _mm256_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_double_avx(double* dst, const double* src1, const double* src2, const size_t len)
{
    int i = 0;
    __m256d X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm256_loadu_pd(src1 + i); // load chunk of 4 double
        Y = _mm256_loadu_pd(src2 + i); // load chunk of 4 double
        X = _mm256_sub_pd(X, Y);
        _mm256_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_float16_avx(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) - im_float16_to_float32(*(src2 + i)));
}
#define msub_int8_simd      msub_int8_avx
#define msub_int16_simd     msub_int16_avx
#define msub_int32_simd     msub_int32_avx
#define msub_int64_simd     msub_int64_avx
#define msub_float_simd     msub_float_avx
#define msub_double_simd    msub_double_avx
#define msub_float16_simd   msub_float16_avx
#elif __SSE__
static inline __attribute__((unused)) void msub_int8_sse(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    int i = 0;
    __m128i X, Y;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = _mm_loadu_si128((__m128i const *)(src1 + i)); // load chunk of 16 char
        Y = _mm_loadu_si128((__m128i const *)(src2 + i)); // load chunk of 16 char
        X = _mm_sub_epi8(X, Y); // signed?
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_int16_sse(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    int i = 0;
    __m128i X, Y;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm_loadu_si128((__m128i const *)(src1 + i)); // load chunk of 8 short
        Y = _mm_loadu_si128((__m128i const *)(src2 + i)); // load chunk of 8 short
        X = _mm_sub_epi16(X, Y); // signed?
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_int32_sse(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    int i = 0;
    __m128i X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_si128((__m128i const *)(src1 + i)); // load chunk of 4 int
        Y = _mm_loadu_si128((__m128i const *)(src2 + i)); // load chunk of 4 int
        X = _mm_sub_epi32(X, Y);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_int64_sse(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    int i = 0;
    __m128i X, Y;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = _mm_loadu_si128((__m128i const *)(src1 + i)); // load chunk of 2 int64
        Y = _mm_loadu_si128((__m128i const *)(src2 + i)); // load chunk of 2 int64
        X = _mm_sub_epi64(X, Y);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_float_sse(float* dst, const float* src1, const float* src2, const size_t len)
{
    int i = 0;
    __m128 X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_ps(src1 + i); // load chunk of 4 floats
        Y = _mm_loadu_ps(src2 + i); // load chunk of 4 floats
        X = _mm_sub_ps(X, Y);
        _mm_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_double_sse(double* dst, const double* src1, const double* src2, const size_t len)
{
    int i = 0;
    __m128d X, Y;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = _mm_loadu_pd(src1 + i); // load chunk of 2 double
        Y = _mm_loadu_pd(src2 + i); // load chunk of 2 double
        X = _mm_sub_pd(X, Y);
        _mm_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_float16_sse(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) - im_float16_to_float32(*(src2 + i)));
}
#define msub_int8_simd      msub_int8_sse
#define msub_int16_simd     msub_int16_sse
#define msub_int32_simd     msub_int32_sse
#define msub_int64_simd     msub_int64_sse
#define msub_float_simd     msub_float_sse
#define msub_double_simd    msub_double_sse
#define msub_float16_simd   msub_float16_sse
#elif __ARM_NEON
static inline __attribute__((unused)) void msub_int8_neon(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    int i = 0;
    int8x16_t X, Y;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = vld1q_u8(src1 + i); // load chunk of 16 char
        Y = vld1q_u8(src2 + i); // load chunk of 16 char
        X = vsubq_u8(X, Y);
        vst1q_u8(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_int16_neon(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    int i = 0;
    int16x8_t X, Y;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = vld1q_u16(src1 + i); // load chunk of 8 short
        Y = vld1q_u16(src2 + i); // load chunk of 8 short
        X = vsubq_u16(X, Y);
        vst1q_u16(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_int32_neon(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    int i = 0;
    int32x4_t X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = vld1q_s32(src1 + i); // load chunk of 4 int
        Y = vld1q_s32(src2 + i); // load chunk of 4 int
        X = vsubq_s32(X, Y);
        vst1q_s32(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_int64_neon(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    int i = 0;
    int64x2_t X, Y;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = vld1q_s64(src1 + i); // load chunk of 2 int64
        Y = vld1q_s64(src2 + i); // load chunk of 2 int64
        X = vsubq_s64(X, Y);
        vst1q_s64(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_float_neon(float* dst, const float* src1, const float* src2, const size_t len)
{
    int i = 0;
    float32x4_t X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = vld1q_f32(src1 + i); // load chunk of 4 floats
        Y = vld1q_f32(src2 + i); // load chunk of 4 floats
        X = vsubq_f32(X, Y);
        vst1q_f32(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_double_neon(double* dst, const double* src1, const double* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_float16_neon(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) - im_float16_to_float32(*(src2 + i)));
}
#define msub_int8_simd      msub_int8_neon
#define msub_int16_simd     msub_int16_neon
#define msub_int32_simd     msub_int32_neon
#define msub_int64_simd     msub_int64_neon
#define msub_float_simd     msub_float_neon
#define msub_double_simd    msub_double_neon
#define msub_float16_simd   msub_float16_neon
#else
static inline __attribute__((unused)) void msub_int8_c(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_int16_c(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_int32_c(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_int64_c(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_float_c(float* dst, const float* src1, const float* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_double_c(double* dst, const double* src1, const double* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) - *(src2 + i);
}
static inline __attribute__((unused)) void msub_float16_c(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) - im_float16_to_float32(*(src2 + i)));
}
#define msub_int8_simd      msub_int8_c
#define msub_int16_simd     msub_int16_c
#define msub_int32_simd     msub_int32_c
#define msub_int64_simd     msub_int64_c
#define msub_float_simd     msub_float_c
#define msub_double_simd    msub_double_c
#define msub_float16_simd   msub_float16_c
#endif

// simd div mat
#if __AVX__
static inline __attribute__((unused)) void mdiv_int8_avx(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src2 + i) != 0 ? *(src1 + i) / *(src2 + i) : UINT8_MAX;
}
static inline __attribute__((unused)) void mdiv_int16_avx(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src2 + i) != 0 ? *(src1 + i) / *(src2 + i) : UINT16_MAX;
}
static inline __attribute__((unused)) void mdiv_int32_avx(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src2 + i) != 0 ? *(src1 + i) / *(src2 + i) : INT32_MAX;
}
static inline __attribute__((unused)) void mdiv_int64_avx(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src2 + i) != 0 ? *(src1 + i) / *(src2 + i) : INT64_MAX;
}
static inline __attribute__((unused)) void mdiv_float_avx(float* dst, const float* src1, const float* src2, const size_t len)
{
    int i = 0;
    __m256 X, Y;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_ps(src1 + i); // load chunk of 8 floats
        Y = _mm256_loadu_ps(src2 + i); // load chunk of 8 floats
        X = _mm256_div_ps(X, Y);
        _mm256_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) / (*(src2 + i) + FLT_EPSILON);
}
static inline __attribute__((unused)) void mdiv_double_avx(double* dst, const double* src1, const double* src2, const size_t len)
{
    int i = 0;
    __m256d X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm256_loadu_pd(src1 + i); // load chunk of 4 double
        Y = _mm256_loadu_pd(src2 + i); // load chunk of 4 double
        X = _mm256_div_pd(X, Y);
        _mm256_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) / (*(src2 + i) + DBL_EPSILON);
}
static inline __attribute__((unused)) void mdiv_float16_avx(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) / im_float16_to_float32(*(src2 + i)));
}
#define mdiv_int8_simd       mdiv_int8_avx
#define mdiv_int16_simd      mdiv_int16_avx
#define mdiv_int32_simd      mdiv_int32_avx
#define mdiv_int64_simd      mdiv_int64_avx
#define mdiv_float_simd      mdiv_float_avx
#define mdiv_double_simd     mdiv_double_avx
#define mdiv_float16_simd    mdiv_float16_avx
#elif __SSE__
static inline __attribute__((unused)) void mdiv_int8_sse(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) / *(src2 + i);
}
static inline __attribute__((unused)) void mdiv_int16_sse(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) / *(src2 + i);
}
static inline __attribute__((unused)) void mdiv_int32_sse(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) / *(src2 + i);
}
static inline __attribute__((unused)) void mdiv_int64_sse(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) / *(src2 + i);
}
static inline __attribute__((unused)) void mdiv_float_sse(float* dst, const float* src1, const float* src2, const size_t len)
{
    int i = 0;
    __m128 X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_ps(src1 + i); // load chunk of 4 floats
        Y = _mm_loadu_ps(src2 + i); // load chunk of 4 floats
        X = _mm_div_ps(X, Y);
        _mm_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) / *(src2 + i);
}
static inline __attribute__((unused)) void mdiv_double_sse(double* dst, const double* src1, const double* src2, const size_t len)
{
    int i = 0;
    __m128d X, Y;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = _mm_loadu_pd(src1 + i); // load chunk of 2 double
        Y = _mm_loadu_pd(src2 + i); // load chunk of 2 double
        X = _mm_div_pd(X, Y);
        _mm_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) / *(src2 + i);
}
static inline __attribute__((unused)) void mdiv_float16_sse(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) / im_float16_to_float32(*(src2 + i)));
}
#define mdiv_int8_simd       mdiv_int8_sse
#define mdiv_int16_simd      mdiv_int16_sse
#define mdiv_int32_simd      mdiv_int32_sse
#define mdiv_int64_simd      mdiv_int64_sse
#define mdiv_float_simd      mdiv_float_sse
#define mdiv_double_simd     mdiv_double_sse
#define mdiv_float16_simd    mdiv_float16_sse
//#elif __ARM_NEON
// TODO::Dicky Add arm neon simd
#else
static inline __attribute__((unused)) void mdiv_int8_c(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) / *(src2 + i);
}
static inline __attribute__((unused)) void mdiv_int16_c(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) / *(src2 + i);
}
static inline __attribute__((unused)) void mdiv_int32_c(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) / *(src2 + i);
}
static inline __attribute__((unused)) void mdiv_int64_c(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) / *(src2 + i);
}
static inline __attribute__((unused)) void mdiv_float_c(float* dst, const float* src1, const float* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) / *(src2 + i);
}
static inline __attribute__((unused)) void mdiv_double_c(double* dst, const double* src1, const double* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) =*(src1 + i) / *(src2 + i);
}
static inline __attribute__((unused)) void mdiv_float16_c(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) / im_float16_to_float32(*(src2 + i)));
}
#define mdiv_int8_simd       mdiv_int8_c
#define mdiv_int16_simd      mdiv_int16_c
#define mdiv_int32_simd      mdiv_int32_c
#define mdiv_int64_simd      mdiv_int64_c
#define mdiv_float_simd      mdiv_float_c
#define mdiv_double_simd     mdiv_double_c
#define mdiv_float16_simd    mdiv_float16_c
#endif

// simd mul mat
#if __AVX__
static inline __attribute__((unused)) void mmul_int8_avx(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_int16_avx(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    int i = 0;
    __m256i X, Y;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = _mm256_loadu_si256((__m256i const *)(src1 + i)); // load chunk of 16 short
        Y = _mm256_loadu_si256((__m256i const *)(src2 + i)); // load chunk of 16 short
        X = _mm256_mullo_epi16(X, Y);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_int32_avx(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    int i = 0;
    __m256i X, Y;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_si256((__m256i const *)(src1 + i)); // load chunk of 8 int
        Y = _mm256_loadu_si256((__m256i const *)(src2 + i)); // load chunk of 8 int
        X = _mm256_mul_epi32(X, Y);
        _mm256_storeu_si256((__m256i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_int64_avx(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_float_avx(float* dst, const float* src1, const float* src2, const size_t len)
{
    int i = 0;
    __m256 X, Y;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm256_loadu_ps(src1 + i); // load chunk of 8 floats
        Y = _mm256_loadu_ps(src2 + i); // load chunk of 8 floats
        X = _mm256_mul_ps(X, Y);
        _mm256_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_double_avx(double* dst, const double* src1, const double* src2, const size_t len)
{
    int i = 0;
    __m256d X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm256_loadu_pd(src1 + i); // load chunk of 4 double
        Y = _mm256_loadu_pd(src2 + i); // load chunk of 4 double
        X = _mm256_mul_pd(X, Y);
        _mm256_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_float16_avx(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) * im_float16_to_float32(*(src2 + i)));
}
#define mmul_int8_simd       mmul_int8_avx
#define mmul_int16_simd      mmul_int16_avx
#define mmul_int32_simd      mmul_int32_avx
#define mmul_int64_simd      mmul_int64_avx
#define mmul_float_simd      mmul_float_avx
#define mmul_double_simd     mmul_double_avx
#define mmul_float16_simd    mmul_float16_avx
#elif __SSE__
static inline __attribute__((unused)) void mmul_int8_sse(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_int16_sse(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    int i = 0;
    __m128i X, Y;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = _mm_loadu_si128((__m128i const *)(src1 + i)); // load chunk of 8 short
        Y = _mm_loadu_si128((__m128i const *)(src2 + i)); // load chunk of 8 short
        X = _mm_mullo_epi16(X, Y); // signed?
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_int32_sse(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    int i = 0;
    __m128i X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_si128((__m128i const *)(src1 + i)); // load chunk of 4 int
        Y = _mm_loadu_si128((__m128i const *)(src2 + i)); // load chunk of 4 int
        X = _mm_mul_epi32(X, Y);
        _mm_storeu_si128((__m128i *)(dst + i), X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_int64_sse(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_float_sse(float* dst, const float* src1, const float* src2, const size_t len)
{
    int i = 0;
    __m128 X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = _mm_loadu_ps(src1 + i); // load chunk of 4 floats
        Y = _mm_loadu_ps(src2 + i); // load chunk of 4 floats
        X = _mm_mul_ps(X, Y);
        _mm_storeu_ps(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_double_sse(double* dst, const double* src1, const double* src2, const size_t len)
{
    int i = 0;
    __m128d X, Y;
    for (i = 0; i < (long)len - 1; i += 2)
    {
        X = _mm_loadu_pd(src1 + i); // load chunk of 2 double
        Y = _mm_loadu_pd(src2 + i); // load chunk of 2 double
        X = _mm_mul_pd(X, Y);
        _mm_storeu_pd(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_float16_sse(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) * im_float16_to_float32(*(src2 + i)));
}
#define mmul_int8_simd       mmul_int8_sse
#define mmul_int16_simd      mmul_int16_sse
#define mmul_int32_simd      mmul_int32_sse
#define mmul_int64_simd      mmul_int64_sse
#define mmul_float_simd      mmul_float_sse
#define mmul_double_simd     mmul_double_sse
#define mmul_float16_simd    mmul_float16_sse
#elif __ARM_NEON
static inline __attribute__((unused)) void mmul_int8_neon(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    int i = 0;
    int8x16_t X, Y;
    for (i = 0; i < (long)len - 15; i += 16)
    {
        X = vld1q_u8(src1 + i); // load chunk of 16 int8
        Y = vld1q_u8(src2 + i); // load chunk of 16 int8
        X = vmulq_u8(X, Y);
        vst1q_u8(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_int16_neon(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    int i = 0;
    int16x8_t X, Y;
    for (i = 0; i < (long)len - 7; i += 8)
    {
        X = vld1q_u16(src1 + i); // load chunk of 8 short
        Y = vld1q_u16(src2 + i); // load chunk of 8 short
        X = vmulq_u16(X, Y);
        vst1q_u16(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_int32_neon(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    int i = 0;
    int32x4_t X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = vld1q_s32(src1 + i); // load chunk of 4 int
        Y = vld1q_s32(src2 + i); // load chunk of 4 int
        X = vmulq_s32(X, Y);
        vst1q_s32(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_int64_neon(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_float_neon(float* dst, const float* src1, const float* src2, const size_t len)
{
    int i = 0;
    float32x4_t X, Y;
    for (i = 0; i < (long)len - 3; i += 4)
    {
        X = vld1q_f32(src1 + i); // load chunk of 4 floats
        Y = vld1q_f32(src2 + i); // load chunk of 4 floats
        X = vmulq_f32(X, Y);
        vst1q_f32(dst + i, X);
    }
    for (; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_double_neon(double* dst, const double* src1, const double* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_float16_neon(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) * im_float16_to_float32(*(src2 + i)));
}
#define mmul_int8_simd       mmul_int8_neon
#define mmul_int16_simd      mmul_int16_neon
#define mmul_int32_simd      mmul_int32_neon
#define mmul_int64_simd      mmul_int64_neon
#define mmul_float_simd      mmul_float_neon
#define mmul_double_simd     mmul_double_neon
#define mmul_float16_simd    mmul_float16_neon
#else
static inline __attribute__((unused)) void mmul_int8_c(uint8_t* dst, const uint8_t* src1, const uint8_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_int16_c(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_int32_c(int32_t* dst, const int32_t* src1, const int32_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_int64_c(int64_t* dst, const int64_t* src1, const int64_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_float_c(float* dst, const float* src1, const float* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_double_c(double* dst, const double* src1, const double* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) *(dst + i) = *(src1 + i) * *(src2 + i);
}
static inline __attribute__((unused)) void mmul_float16_c(uint16_t* dst, const uint16_t* src1, const uint16_t* src2, const size_t len)
{
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < len; ++i) 
        *(dst + i) = im_float32_to_float16(im_float16_to_float32(*(src1 + i)) * im_float16_to_float32(*(src2 + i)));
}
#define mmul_int8_simd       mmul_int8_c
#define mmul_int16_simd      mmul_int16_c
#define mmul_int32_simd      mmul_int32_c
#define mmul_int64_simd      mmul_int64_c
#define mmul_float_simd      mmul_float_c
#define mmul_double_simd     mmul_double_c
#define mmul_float16_simd    mmul_float16_c
#endif

// scalar add
template<typename T> 
inline ImMat ImMat::operator+ (T v)
{
    assert(device == IM_DD_CPU);
    ImMat m;
    m.create_like(*this);
    if (!m.data)
        return m;
    switch (type)
    {
        case IM_DT_INT8:    add_int8_simd((uint8_t *)m.data, (uint8_t *) this->data, total(), static_cast<uint8_t> (v)); break;
        case IM_DT_INT16:   add_int16_simd((uint16_t *)m.data, (uint16_t *) this->data, total(), static_cast<uint16_t> (v)); break;
        case IM_DT_INT32:   add_int32_simd((int32_t *)m.data, (int32_t *) this->data, total(), static_cast<int32_t> (v)); break;
        case IM_DT_INT64:   add_int64_simd((int64_t *)m.data, (int64_t *) this->data, total(), static_cast<int64_t> (v)); break;
        case IM_DT_FLOAT32: add_float_simd((float *)m.data, (float *) this->data, total(), static_cast<float> (v)); break;
        case IM_DT_FLOAT64: add_double_simd((double *)m.data, (double *) this->data, total(), static_cast<double> (v)); break;
        case IM_DT_FLOAT16: add_float16_simd((uint16_t *)m.data, (uint16_t *) this->data, total(), static_cast<float> (v)); break;
        default: break;
    }
    return m;
}

template<typename T> 
inline ImMat& ImMat::operator+=(T v)
{
    assert(device == IM_DD_CPU);
    switch (type)
    {
        case IM_DT_INT8:    add_int8_simd((uint8_t *)this->data, (uint8_t *) this->data, total(), static_cast<uint8_t> (v)); break;
        case IM_DT_INT16:   add_int16_simd((uint16_t *)this->data, (uint16_t *) this->data, total(), static_cast<uint16_t> (v)); break;
        case IM_DT_INT32:   add_int32_simd((int32_t *)this->data, (int32_t *) this->data, total(), static_cast<int32_t> (v)); break;
        case IM_DT_INT64:   add_int64_simd((int64_t *)this->data, (int64_t *) this->data, total(), static_cast<int64_t> (v)); break;
        case IM_DT_FLOAT32: add_float_simd((float *)this->data, (float *) this->data, total(), static_cast<float> (v)); break;
        case IM_DT_FLOAT64: add_double_simd((double *)this->data, (double *) this->data, total(), static_cast<double> (v)); break;
        case IM_DT_FLOAT16: add_float16_simd((uint16_t *)this->data, (uint16_t *) this->data, total(), static_cast<float> (v)); break;
        default: break;
    }
    return *this;
}

// scalar sub
template<typename T> 
inline ImMat ImMat::operator- (T v)
{
    assert(device == IM_DD_CPU);
    ImMat m;
    m.create_like(*this);
    if (!m.data)
        return m;
    switch (type)
    {
        case IM_DT_INT8:    sub_int8_simd((uint8_t *)m.data, (uint8_t *) this->data, total(), static_cast<uint8_t> (v)); break;
        case IM_DT_INT16:   sub_int16_simd((uint16_t *)m.data, (uint16_t *) this->data, total(), static_cast<uint16_t> (v)); break;
        case IM_DT_INT32:   sub_int32_simd((int32_t *)m.data, (int32_t *) this->data, total(), static_cast<int32_t> (v)); break;
        case IM_DT_INT64:   sub_int64_simd((int64_t *)m.data, (int64_t *) this->data, total(), static_cast<int64_t> (v)); break;
        case IM_DT_FLOAT32: sub_float_simd((float *)m.data, (float *) this->data, total(), static_cast<float> (v)); break;
        case IM_DT_FLOAT64: sub_double_simd((double *)m.data, (double *) this->data, total(), static_cast<double> (v)); break;
        case IM_DT_FLOAT16: sub_float16_simd((uint16_t *)m.data, (uint16_t *) this->data, total(), static_cast<float> (v)); break;
        default: break;
    }
    return m;
}

template<typename T> 
inline ImMat& ImMat::operator-=(T v)
{
    assert(device == IM_DD_CPU);
    switch (type)
    {
        case IM_DT_INT8:    sub_int8_simd((uint8_t *)this->data, (uint8_t *) this->data, total(), static_cast<uint8_t> (v)); break;
        case IM_DT_INT16:   sub_int16_simd((uint16_t *)this->data, (uint16_t *) this->data, total(), static_cast<uint16_t> (v)); break;
        case IM_DT_INT32:   sub_int32_simd((int32_t *)this->data, (int32_t *) this->data, total(), static_cast<int32_t> (v)); break;
        case IM_DT_INT64:   sub_int64_simd((int64_t *)this->data, (int64_t *) this->data, total(), static_cast<int64_t> (v)); break;
        case IM_DT_FLOAT32: sub_float_simd((float *)this->data, (float *) this->data, total(), static_cast<float> (v)); break;
        case IM_DT_FLOAT64: sub_double_simd((double *)this->data, (double *) this->data, total(), static_cast<double> (v)); break;
        case IM_DT_FLOAT16: sub_float16_simd((uint16_t *)this->data, (uint16_t *) this->data, total(), static_cast<float> (v)); break;
        default: break;
    }
    return *this;
}

// scalar mul
template<typename T> 
inline ImMat ImMat::operator* (T v)
{
    assert(device == IM_DD_CPU);
    ImMat m;
    m.create_like(*this);
    if (!m.data)
        return m;
    switch (type)
    {
        case IM_DT_INT8:    mul_int8_simd((uint8_t *)m.data, (uint8_t *) this->data, total(), static_cast<uint8_t> (v)); break;
        case IM_DT_INT16:   mul_int16_simd((uint16_t *)m.data, (uint16_t *) this->data, total(), static_cast<uint16_t> (v)); break;
        case IM_DT_INT32:   mul_int32_simd((int32_t *)m.data, (int32_t *) this->data, total(), static_cast<int32_t> (v)); break;
        case IM_DT_INT64:   mul_int64_simd((int64_t *)m.data, (int64_t *) this->data, total(), static_cast<int64_t> (v)); break;
        case IM_DT_FLOAT32: mul_float_simd((float *)m.data, (float *) this->data, total(), static_cast<float> (v)); break;
        case IM_DT_FLOAT64: mul_double_simd((double *)m.data, (double *) this->data, total(), static_cast<double> (v)); break;
        case IM_DT_FLOAT16: mul_float16_simd((uint16_t *)m.data, (uint16_t *) this->data, total(), static_cast<float> (v)); break;
        default: break;
    }
    return m;
}

template<typename T> 
inline ImMat& ImMat::operator*=(T v)
{
    assert(device == IM_DD_CPU);
    switch (type)
    {
        case IM_DT_INT8:    mul_int8_simd((uint8_t *)this->data, (uint8_t *) this->data, total(), static_cast<uint8_t> (v)); break;
        case IM_DT_INT16:   mul_int16_simd((uint16_t *)this->data, (uint16_t *) this->data, total(), static_cast<uint16_t> (v)); break;
        case IM_DT_INT32:   mul_int32_simd((int32_t *)this->data, (int32_t *) this->data, total(), static_cast<int32_t> (v)); break;
        case IM_DT_INT64:   mul_int64_simd((int64_t *)this->data, (int64_t *) this->data, total(), static_cast<int64_t> (v)); break;
        case IM_DT_FLOAT32: mul_float_simd((float *)this->data, (float *) this->data, total(), static_cast<float> (v)); break;
        case IM_DT_FLOAT64: mul_double_simd((double *)this->data, (double *) this->data, total(), static_cast<double> (v)); break;
        case IM_DT_FLOAT16: mul_float16_simd((uint16_t *)this->data, (uint16_t *) this->data, total(), static_cast<float> (v)); break;
        default: break;
    }
    return *this;
}

// scalar div
template<typename T> 
inline ImMat ImMat::operator/ (T v)
{
    assert(device == IM_DD_CPU);
    ImMat m;
    m.create_like(*this);
    if (!m.data)
        return m;
    switch (type)
    {
        case IM_DT_INT8:    if (static_cast<uint8_t> (v) != 0) div_int8_simd((uint8_t *)m.data, (uint8_t *) this->data, total(), static_cast<uint8_t> (v)); break;
        case IM_DT_INT16:   if (static_cast<uint16_t>(v) != 0) div_int16_simd((uint16_t *)m.data, (uint16_t *) this->data, total(), static_cast<uint16_t> (v)); break;
        case IM_DT_INT32:   if (static_cast<int32_t>(v) != 0) div_int32_simd((int32_t *)m.data, (int32_t *) this->data, total(), static_cast<int32_t> (v)); break;
        case IM_DT_INT64:   if (static_cast<int64_t>(v) != 0) div_int64_simd((int64_t *)m.data, (int64_t *) this->data, total(), static_cast<int64_t> (v)); break;
        case IM_DT_FLOAT32: if (static_cast<float>  (v) != 0) div_float_simd((float *)m.data, (float *) this->data, total(), static_cast<float> (v)); break;
        case IM_DT_FLOAT64: if (static_cast<double> (v) != 0) div_double_simd((double *)m.data, (double *) this->data, total(), static_cast<double> (v)); break;
        case IM_DT_FLOAT16: if (static_cast<float>  (v) != 0) div_float16_simd((uint16_t *)m.data, (uint16_t *) this->data, total(), static_cast<float> (v)); break;
        default: break;
    }
    return m;
}

template<typename T> 
inline ImMat& ImMat::operator/=(T v)
{
    assert(device == IM_DD_CPU);
    switch (type)
    {
        case IM_DT_INT8:    if (static_cast<uint8_t> (v) != 0) div_int8_simd((uint8_t *)this->data, (uint8_t *) this->data, total(), static_cast<uint8_t> (v)); break;
        case IM_DT_INT16:   if (static_cast<uint16_t>(v) != 0) div_int16_simd((uint16_t *)this->data, (uint16_t *) this->data, total(), static_cast<uint16_t> (v)); break;
        case IM_DT_INT32:   if (static_cast<int32_t>(v) != 0) div_int32_simd((int32_t *)this->data, (int32_t *) this->data, total(), static_cast<int32_t> (v)); break;
        case IM_DT_INT64:   if (static_cast<int64_t>(v) != 0) div_int64_simd((int64_t *)this->data, (int64_t *) this->data, total(), static_cast<int64_t> (v)); break;
        case IM_DT_FLOAT32: if (static_cast<float>  (v) != 0) div_float_simd((float *)this->data, (float *) this->data, total(), static_cast<float> (v)); break;
        case IM_DT_FLOAT64: if (static_cast<double> (v) != 0) div_double_simd((double *)this->data, (double *) this->data, total(), static_cast<double> (v)); break;
        case IM_DT_FLOAT16: if (static_cast<float>  (v) != 0) div_float16_simd((uint16_t *)this->data, (uint16_t *) this->data, total(), static_cast<float> (v)); break;
        default: break;
    }
    return *this;
}

// mat add
inline ImMat ImMat::operator+(const ImMat& mat)
{
    assert(device == IM_DD_CPU);
    assert(w == mat.w);
    assert(h == mat.h);
    assert(c == mat.c);
    assert(type == mat.type);
    ImMat m;
    m.create_like(*this);
    switch (type)
    {
        case IM_DT_INT8:    madd_int8_simd((uint8_t *)m.data, (uint8_t *) this->data, (uint8_t *) mat.data, total()); break;
        case IM_DT_INT16:   madd_int16_simd((uint16_t *)m.data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        case IM_DT_INT32:   madd_int32_simd((int32_t *)m.data, (int32_t *) this->data, (int32_t *) mat.data, total()); break;
        case IM_DT_INT64:   madd_int64_simd((int64_t *)m.data, (int64_t *) this->data, (int64_t *) mat.data, total()); break;
        case IM_DT_FLOAT32: madd_float_simd((float *)m.data, (float *) this->data, (float *) mat.data, total()); break;
        case IM_DT_FLOAT64: madd_double_simd((double *)m.data, (double *) this->data, (double *) mat.data, total()); break;
        case IM_DT_FLOAT16: madd_float16_simd((uint16_t *)m.data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        default: break;
    }
    return m;
}

inline ImMat& ImMat::operator+=(const ImMat& mat)
{
    assert(device == IM_DD_CPU);
    assert(w == mat.w);
    assert(h == mat.h);
    assert(c == mat.c);
    assert(type == mat.type);
    switch (type)
    {
        case IM_DT_INT8:    madd_int8_simd((uint8_t *)this->data, (uint8_t *) this->data, (uint8_t *) mat.data, total()); break;
        case IM_DT_INT16:   madd_int16_simd((uint16_t *)this->data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        case IM_DT_INT32:   madd_int32_simd((int32_t *)this->data, (int32_t *) this->data, (int32_t *) mat.data, total()); break;
        case IM_DT_INT64:   madd_int64_simd((int64_t *)this->data, (int64_t *) this->data, (int64_t *) mat.data, total()); break;
        case IM_DT_FLOAT32: madd_float_simd((float *)this->data, (float *) this->data, (float *) mat.data, total()); break;
        case IM_DT_FLOAT64: madd_double_simd((double *)this->data, (double *) this->data, (double *) mat.data, total()); break;
        case IM_DT_FLOAT16: madd_float16_simd((uint16_t *)this->data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        default: break;
    }
    return *this;
}

// mat sub
inline ImMat ImMat::operator-(const ImMat& mat)
{
    assert(device == IM_DD_CPU);
    assert(w == mat.w);
    assert(h == mat.h);
    assert(c == mat.c);
    assert(type == mat.type);
    ImMat m;
    m.create_like(*this);
    switch (type)
    {
        case IM_DT_INT8:    msub_int8_simd((uint8_t *)m.data, (uint8_t *) this->data, (uint8_t *) mat.data, total()); break;
        case IM_DT_INT16:   msub_int16_simd((uint16_t *)m.data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        case IM_DT_INT32:   msub_int32_simd((int32_t *)m.data, (int32_t *) this->data, (int32_t *) mat.data, total()); break;
        case IM_DT_INT64:   msub_int64_simd((int64_t *)m.data, (int64_t *) this->data, (int64_t *) mat.data, total()); break;
        case IM_DT_FLOAT32: msub_float_simd((float *)m.data, (float *) this->data, (float *) mat.data, total()); break;
        case IM_DT_FLOAT64: msub_double_simd((double *)m.data, (double *) this->data, (double *) mat.data, total()); break;
        case IM_DT_FLOAT16: msub_float16_simd((uint16_t *)m.data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        default: break;
    }
    return m;
}

inline ImMat& ImMat::operator-=(const ImMat& mat)
{
    assert(device == IM_DD_CPU);
    assert(w == mat.w);
    assert(h == mat.h);
    assert(c == mat.c);
    assert(type == mat.type);
    switch (type)
    {
        case IM_DT_INT8:    msub_int8_simd((uint8_t *)this->data, (uint8_t *) this->data, (uint8_t *) mat.data, total()); break;
        case IM_DT_INT16:   msub_int16_simd((uint16_t *)this->data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        case IM_DT_INT32:   msub_int32_simd((int32_t *)this->data, (int32_t *) this->data, (int32_t *) mat.data, total()); break;
        case IM_DT_INT64:   msub_int64_simd((int64_t *)this->data, (int64_t *) this->data, (int64_t *) mat.data, total()); break;
        case IM_DT_FLOAT32: msub_float_simd((float *)this->data, (float *) this->data, (float *) mat.data, total()); break;
        case IM_DT_FLOAT64: msub_double_simd((double *)this->data, (double *) this->data, (double *) mat.data, total()); break;
        case IM_DT_FLOAT16: msub_float16_simd((uint16_t *)this->data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        default: break;
    }
    return *this;
}

// mat div
inline ImMat ImMat::operator/(const ImMat& mat)
{
    assert(device == IM_DD_CPU);
    assert(w == mat.w);
    assert(h == mat.h);
    assert(c == mat.c);
    assert(type == mat.type);
    ImMat m;
    m.create_like(*this);
    switch (type)
    {
        case IM_DT_INT8:    mdiv_int8_simd((uint8_t *)m.data, (uint8_t *) this->data, (uint8_t *) mat.data, total()); break;
        case IM_DT_INT16:   mdiv_int16_simd((uint16_t *)m.data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        case IM_DT_INT32:   mdiv_int32_simd((int32_t *)m.data, (int32_t *) this->data, (int32_t *) mat.data, total()); break;
        case IM_DT_INT64:   mdiv_int64_simd((int64_t *)m.data, (int64_t *) this->data, (int64_t *) mat.data, total()); break;
        case IM_DT_FLOAT32: mdiv_float_simd((float *)m.data, (float *) this->data, (float *) mat.data, total()); break;
        case IM_DT_FLOAT64: mdiv_double_simd((double *)m.data, (double *) this->data, (double *) mat.data, total()); break;
        case IM_DT_FLOAT16: mdiv_float16_simd((uint16_t *)m.data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        default: break;
    }
    return m;
}

inline ImMat& ImMat::operator/=(const ImMat& mat)
{
    assert(device == IM_DD_CPU);
    assert(w == mat.w);
    assert(h == mat.h);
    assert(c == mat.c);
    assert(type == mat.type);
    switch (type)
    {
        case IM_DT_INT8:    mdiv_int8_simd((uint8_t *)this->data, (uint8_t *) this->data, (uint8_t *) mat.data, total()); break;
        case IM_DT_INT16:   mdiv_int16_simd((uint16_t *)this->data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        case IM_DT_INT32:   mdiv_int32_simd((int32_t *)this->data, (int32_t *) this->data, (int32_t *) mat.data, total()); break;
        case IM_DT_INT64:   mdiv_int64_simd((int64_t *)this->data, (int64_t *) this->data, (int64_t *) mat.data, total()); break;
        case IM_DT_FLOAT32: mdiv_float_simd((float *)this->data, (float *) this->data, (float *) mat.data, total()); break;
        case IM_DT_FLOAT64: mdiv_double_simd((double *)this->data, (double *) this->data, (double *) mat.data, total()); break;
        case IM_DT_FLOAT16: mdiv_float16_simd((uint16_t *)this->data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        default: break;
    }
    return *this;
}

inline ImMat& ImMat::square()
{
    assert(device == IM_DD_CPU);
    switch (type)
    {
        case IM_DT_INT8:    mmul_int8_simd((uint8_t *)this->data, (uint8_t *) this->data, (uint8_t *) this->data, total()); break;
        case IM_DT_INT16:   mmul_int16_simd((uint16_t *)this->data, (uint16_t *) this->data, (uint16_t *) this->data, total()); break;
        case IM_DT_INT32:   mmul_int32_simd((int32_t *)this->data, (int32_t *) this->data, (int32_t *) this->data, total()); break;
        case IM_DT_INT64:   mmul_int64_simd((int64_t *)this->data, (int64_t *) this->data, (int64_t *) this->data, total()); break;
        case IM_DT_FLOAT32: mmul_float_simd((float *)this->data, (float *) this->data, (float *) this->data, total()); break;
        case IM_DT_FLOAT64: mmul_double_simd((double *)this->data, (double *) this->data, (double *) this->data, total()); break;
        case IM_DT_FLOAT16: mmul_float16_simd((uint16_t *)this->data, (uint16_t *) this->data, (uint16_t *) this->data, total()); break;
        default: break;
    }
    return *this;
}

// mat mul
inline ImMat& ImMat::mul(const ImMat& mat)
{
    assert(device == IM_DD_CPU);
    assert(w == mat.w);
    assert(h == mat.h);
    assert(c == mat.c);
    assert(type == mat.type);
    switch (type)
    {
        case IM_DT_INT8:    mmul_int8_simd((uint8_t *)this->data, (uint8_t *) this->data, (uint8_t *) mat.data, total()); break;
        case IM_DT_INT16:   mmul_int16_simd((uint16_t *)this->data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        case IM_DT_INT32:   mmul_int32_simd((int32_t *)this->data, (int32_t *) this->data, (int32_t *) mat.data, total()); break;
        case IM_DT_INT64:   mmul_int64_simd((int64_t *)this->data, (int64_t *) this->data, (int64_t *) mat.data, total()); break;
        case IM_DT_FLOAT32: mmul_float_simd((float *)this->data, (float *) this->data, (float *) mat.data, total()); break;
        case IM_DT_FLOAT64: mmul_double_simd((double *)this->data, (double *) this->data, (double *) mat.data, total()); break;
        case IM_DT_FLOAT16: mmul_float16_simd((uint16_t *)this->data, (uint16_t *) this->data, (uint16_t *) mat.data, total()); break;
        default: break;
    }
    return *this;
}

// mat dot mul
inline ImMat ImMat::operator*(const ImMat& mat)
{
    assert(device == IM_DD_CPU);
    assert(dims == 2);
    assert(w == mat.h);
    ImMat m;
    m.create_type(mat.w, h, type, allocator);
    if (!m.data)
        return m;
    for (int i = 0; i < m.h; i++)
    {
        for (int j = 0; j < m.w; j++)
        {
            for (int k = 0; k < w; k++)
            {
                switch (type)
                {
                    case IM_DT_INT8:    m.at<uint8_t> (j, i) += this->at<uint8_t> (k, i) * mat.at<uint8_t> (j, k); break;
                    case IM_DT_INT16:   m.at<uint16_t>(j, i) += this->at<uint16_t>(k, i) * mat.at<uint16_t>(j, k); break;
                    case IM_DT_INT32:   m.at<int32_t>(j, i) += this->at<int32_t>(k, i) * mat.at<int32_t>(j, k); break;
                    case IM_DT_INT64:   m.at<int64_t>(j, i) += this->at<int64_t>(k, i) * mat.at<int64_t>(j, k); break;
                    case IM_DT_FLOAT32: m.at<float>  (j, i) += this->at<float>  (k, i) * mat.at<float>  (j, k); break;
                    case IM_DT_FLOAT64: m.at<double> (j, i) += this->at<double> (k, i) * mat.at<double> (j, k); break;
                    case IM_DT_FLOAT16: m.at<uint16_t>(j, i) = im_float32_to_float16(
                                                                im_float16_to_float32(m.at<uint16_t>(j, i)) + 
                                                                im_float16_to_float32(this->at<uint16_t>(k, i)) *
                                                                im_float16_to_float32(mat.at<uint16_t> (j, k))); break;
                    default: break;
                }
            }
        }
    }
    return m;
}

inline ImMat& ImMat::operator*=(const ImMat& mat)
{
    assert(device == IM_DD_CPU);
    assert(dims == 2);
    assert(w == mat.h);
    ImMat m;
    m.clone_from(*this);
    this->release();
    this->create_type(mat.w, m.h, m.type, allocator);
    for (int i = 0; i < h; i++)
    {
        for (int j = 0; j < w; j++)
        {
            for (int k = 0; k < m.w; k++)
            {
                switch (type)
                {
                    case IM_DT_INT8:    this->at<uint8_t> (j, i) += m.at<uint8_t> (k, i) * mat.at<uint8_t> (j, k); break;
                    case IM_DT_INT16:   this->at<uint16_t>(j, i) += m.at<uint16_t>(k, i) * mat.at<uint16_t>(j, k); break;
                    case IM_DT_INT32:   this->at<int32_t>(j, i) += m.at<int32_t>(k, i) * mat.at<int32_t>(j, k); break;
                    case IM_DT_INT64:   this->at<int64_t>(j, i) += m.at<int64_t>(k, i) * mat.at<int64_t>(j, k); break;
                    case IM_DT_FLOAT32: this->at<float>  (j, i) += m.at<float>  (k, i) * mat.at<float>  (j, k); break;
                    case IM_DT_FLOAT64: this->at<double> (j, i) += m.at<double> (k, i) * mat.at<double> (j, k); break;
                    case IM_DT_FLOAT16: this->at<uint16_t>(j, i) = im_float32_to_float16(
                                                                    im_float16_to_float32(this->at<uint16_t>(j, i)) + 
                                                                    im_float16_to_float32(m.at<uint16_t>(k, i)) *
                                                                    im_float16_to_float32(mat.at<uint16_t> (j, k))); break;
                    default: break;
                }
            }
        }
    }
    return *this;
}

// negative
inline ImMat ImMat::operator-()
{
    assert(device == IM_DD_CPU);
    assert(total() > 0);
    ImMat m;
    m.create_like(*this);
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = 0; i < total(); i++)
    {
        switch (type)
        {
            case IM_DT_INT8:    ((uint8_t*)m.data)[i]  = - ((uint8_t*)this->data)[i]; break;
            case IM_DT_INT16:   ((uint16_t*)m.data)[i] = - ((uint16_t*)this->data)[i]; break;
            case IM_DT_INT32:   ((int32_t*)m.data)[i] = - ((int32_t*)this->data)[i]; break;
            case IM_DT_INT64:   ((int64_t*)m.data)[i] = - ((int64_t*)this->data)[i]; break;
            case IM_DT_FLOAT32: ((float*)m.data)[i]   = - ((float*)this->data)[i]; break;
            case IM_DT_FLOAT64: ((double*)m.data)[i]  = - ((double*)this->data)[i]; break;
            case IM_DT_FLOAT16: ((int16_t*)m.data)[i] = im_float32_to_float16(-im_float16_to_float32(((int16_t*)this->data)[i])); break; 
            default: break;
        }
    }
    return m;
}

// mean
inline ImMat ImMat::mean()
{
    assert(device == IM_DD_CPU);
    assert(total() > 0);
    ImMat m;
    m.create_type(c, IM_DT_FLOAT32);
    if (dims == 1)
    {
        for (int _w = 0; _w < w; _w++)
        {
            switch (type)
            {
                case IM_DT_INT8:    m.at<float>(0) += at<uint8_t>(_w); break;
                case IM_DT_INT16:   m.at<float>(0) += at<uint16_t>(_w); break;
                case IM_DT_INT32:   m.at<float>(0) += at<int32_t>(_w); break;
                case IM_DT_INT64:   m.at<float>(0) += at<int64_t>(_w); break;
                case IM_DT_FLOAT32: m.at<float>(0) += at<float>(_w); break;
                case IM_DT_FLOAT64: m.at<float>(0) += at<double>(_w); break;
                case IM_DT_FLOAT16: m.at<float>(0) += im_float16_to_float32(at<int16_t>(_w)); break; 
                default: break;
            }
        }
        m.at<float>(0) /= w;
    }
    else if (dims == 2)
    {
        for (int _h = 0; _h < h; _h++)
        {
            for (int _w = 0; _w < w; _w++)
            {
                switch (type)
                {
                    case IM_DT_INT8:    m.at<float>(0) += at<uint8_t>(_w, _h); break;
                    case IM_DT_INT16:   m.at<float>(0) += at<uint16_t>(_w, _h); break;
                    case IM_DT_INT32:   m.at<float>(0) += at<int32_t>(_w, _h); break;
                    case IM_DT_INT64:   m.at<float>(0) += at<int64_t>(_w, _h); break;
                    case IM_DT_FLOAT32: m.at<float>(0) += at<float>(_w, _h); break;
                    case IM_DT_FLOAT64: m.at<float>(0) += at<double>(_w, _h); break;
                    case IM_DT_FLOAT16: m.at<float>(0) += im_float16_to_float32(m.at<int16_t>(0)); break; 
                    default: break;
                }
            }
        }
        m.at<float>(0) /= w * h;
    }
    else if (dims == 3)
    {
        for (int _c = 0; _c < c; _c++)
        {
            for (int _h = 0; _h < h; _h++)
            {
                for (int _w = 0; _w < w; _w++)
                {
                    switch (type)
                    {
                        case IM_DT_INT8:    m.at<float>(_c) += at<uint8_t>(_w, _h, _c); break;
                        case IM_DT_INT16:   m.at<float>(_c) += at<uint16_t>(_w, _h, _c); break;
                        case IM_DT_INT32:   m.at<float>(_c) += at<int32_t>(_w, _h, _c); break;
                        case IM_DT_INT64:   m.at<float>(_c) += at<int64_t>(_w, _h, _c); break;
                        case IM_DT_FLOAT32: m.at<float>(_c) += at<float>(_w, _h, _c); break;
                        case IM_DT_FLOAT64: m.at<float>(_c) += at<double>(_w, _h, _c); break;
                        case IM_DT_FLOAT16: m.at<float>(_c) += im_float16_to_float32(m.at<int16_t>(_c)); break; 
                        default: break;
                    }
                }
            }
            m.at<float>(_c) /= w * h;
        }
    }
    return m;
}

// absdiff
inline ImMat ImMat::absdiff(const ImMat& m) const
{
    assert(device == IM_DD_CPU);
    assert(total() > 0);
    ImMat dst;
    dst.create_type(w, h, c, type);
    for (int i = 0; i < total(); i++)
    {
        switch (type)
        {
            case IM_DT_INT8:
                ((uint8_t*)dst.data)[i] = std::abs(((uint8_t*)data)[i] - ((uint8_t*)m.data)[i]);
                break;
            case IM_DT_INT16:
                ((uint16_t*)dst.data)[i] = std::abs(((uint16_t*)data)[i] - ((uint16_t*)m.data)[i]);
                break;
            case IM_DT_INT32:
                ((int32_t*)dst.data)[i] = std::abs(((int32_t*)data)[i] - ((int32_t*)m.data)[i]);
                break;
            case IM_DT_INT64:
                ((int64_t*)dst.data)[i] = std::abs(((int64_t*)data)[i] - ((int64_t*)m.data)[i]);
                break;
            case IM_DT_FLOAT32:
                ((float*)dst.data)[i] = std::abs(((float*)data)[i] - ((float*)m.data)[i]);
                break;
            case IM_DT_FLOAT64:
                ((double*)dst.data)[i] = std::abs(((double*)data)[i] - ((double*)m.data)[i]);
                break;
            case IM_DT_FLOAT16:
                ((int16_t*)dst.data)[i] = im_float32_to_float16(std::abs(im_float16_to_float32(((int16_t*)data)[i]) - im_float16_to_float32(((int16_t*)m.data)[i])));
                break;
            default: break;
        }
    }
    return dst;
}

// sqr
inline ImMat ImMat::sqr() const
{
    assert(device == IM_DD_CPU);
    assert(total() > 0);
    ImMat dst;
    dst.create_type(w, h, c, IM_DT_FLOAT32);
    for (int i = 0; i < total(); i++)
    {
        switch (type)
        {
            case IM_DT_INT8:
                ((float*)dst.data)[i] = std::pow(((uint8_t*)data)[i], 2);
                break;
            case IM_DT_INT16:
                ((float*)dst.data)[i] = std::pow(((uint16_t*)data)[i], 2);
                break;
            case IM_DT_INT32:
                ((float*)dst.data)[i] = std::pow(((int32_t*)data)[i], 2);
                break;
            case IM_DT_INT64:
                ((float*)dst.data)[i] = std::pow(((int64_t*)data)[i], 2);
                break;
            case IM_DT_FLOAT32:
                ((float*)dst.data)[i] = std::pow(((float*)data)[i], 2);
                break;
            case IM_DT_FLOAT64:
                ((float*)dst.data)[i] = std::pow(((double*)data)[i], 2);
                break;
            case IM_DT_FLOAT16:
                ((float*)dst.data)[i] = std::pow(im_float16_to_float32(((int16_t*)data)[i]), 2);
                break;
            default: break;
        }
    }
    return dst;
}

// sum
inline ImMat ImMat::sum()
{
    assert(device == IM_DD_CPU);
    assert(total() > 0);
    ImMat m;
    m.create_type(c, (ImDataType)IM_DT_FLOAT32);
    if (dims == 1)
    {
        for (int _w = 0; _w < w; _w++)
        {
            switch (type)
            {
                case IM_DT_INT8:    m.at<float>(0) += at<uint8_t>(_w); break;
                case IM_DT_INT16:   m.at<float>(0) += at<uint16_t>(_w); break;
                case IM_DT_INT32:   m.at<float>(0) += at<int32_t>(_w); break;
                case IM_DT_INT64:   m.at<float>(0) += at<int64_t>(_w); break;
                case IM_DT_FLOAT32: m.at<float>(0) += at<float>(_w); break;
                case IM_DT_FLOAT64: m.at<float>(0) += at<double>(_w); break;
                case IM_DT_FLOAT16: m.at<float>(0) = im_float16_to_float32(at<int16_t>(_w)) +
                                                        im_float16_to_float32(m.at<int16_t>(0)); break; 
                default: break;
            }
        }
    }
    else if (dims == 2)
    {
        for (int _h = 0; _h < h; _h++)
        {
            for (int _w = 0; _w < w; _w++)
            {
                switch (type)
                {
                    case IM_DT_INT8:    m.at<float>(0) += at<uint8_t>(_w, _h); break;
                    case IM_DT_INT16:   m.at<float>(0) += at<uint16_t>(_w, _h); break;
                    case IM_DT_INT32:   m.at<float>(0) += at<int32_t>(_w, _h); break;
                    case IM_DT_INT64:   m.at<float>(0) += at<int64_t>(_w, _h); break;
                    case IM_DT_FLOAT32: m.at<float>(0) += at<float>(_w, _h); break;
                    case IM_DT_FLOAT64: m.at<float>(0) += at<double>(_w, _h); break;
                    case IM_DT_FLOAT16: m.at<float>(0) = im_float16_to_float32(at<int16_t>(_w, _h)) +
                                                            im_float16_to_float32(m.at<int16_t>(0)); break; 
                    default: break;
                }
            }
        }
    }
    else if (dims == 3)
    {
        for (int _c = 0; _c < c; _c++)
        {
            for (int _h = 0; _h < h; _h++)
            {
                for (int _w = 0; _w < w; _w++)
                {
                    switch (type)
                    {
                        case IM_DT_INT8:    m.at<float>(_c) += at<uint8_t>(_w, _h, _c); break;
                        case IM_DT_INT16:   m.at<float>(_c) += at<uint16_t>(_w, _h, _c); break;
                        case IM_DT_INT32:   m.at<float>(_c) += at<int32_t>(_w, _h, _c); break;
                        case IM_DT_INT64:   m.at<float>(_c) += at<int64_t>(_w, _h, _c); break;
                        case IM_DT_FLOAT32: m.at<float>(_c) += at<float>(_w, _h, _c); break;
                        case IM_DT_FLOAT64: m.at<float>(_c) += at<double>(_w, _h, _c); break;
                        case IM_DT_FLOAT16: m.at<float>(_c) = im_float16_to_float32(at<int16_t>(_w, _h, _c)) +
                                                                im_float16_to_float32(m.at<int16_t>(_c)); break; 
                        default: break;
                    }
                }
            }
        }
    }
    return m;
}

// norm
inline float ImMat::norm(int norm_type)
{
    assert(device == IM_DD_CPU);
    float result = 0;
    switch (norm_type)
    {
        case NORM_L2:
        {
            for (int i = 0; i < total(); i++)
            {
                switch (type)
                {
                    case IM_DT_INT8:    result += ((uint8_t*)data)[i] * ((uint8_t*)data)[i]; break;
                    case IM_DT_INT16:   result += ((uint16_t*)data)[i] * ((uint16_t*)data)[i]; break;
                    case IM_DT_INT32:   result += ((int32_t*)data)[i] * ((int32_t*)data)[i]; break;  
                    case IM_DT_INT64:   result += ((int64_t*)data)[i] * ((int64_t*)data)[i]; break;
                    case IM_DT_FLOAT32: result += ((float*)data)[i] * ((float*)data)[i]; break;
                    case IM_DT_FLOAT64: result += ((double*)data)[i] * ((double*)data)[i]; break;
                    case IM_DT_FLOAT16: result += im_float16_to_float32(((int16_t*)data)[i]) * im_float16_to_float32(((int16_t*)data)[i]); break;
                    default: break;
                }
            }
            result = std::sqrt(result);
        }
        break;
        case NORM_L1:
        {
            for (int i = 0; i < total(); i++)
            {
                switch (type)
                {
                    case IM_DT_INT8:    result += ((uint8_t*)data)[i]; break;
                    case IM_DT_INT16:   result += ((uint16_t*)data)[i]; break;
                    case IM_DT_INT32:   result += std::abs(((int32_t*)data)[i]); break;  
                    case IM_DT_INT64:   result += std::abs(((int64_t*)data)[i]); break;
                    case IM_DT_FLOAT32: result += std::abs(((float*)data)[i]); break;
                    case IM_DT_FLOAT64: result += std::abs(((double*)data)[i]); break;
                    case IM_DT_FLOAT16: result += std::abs(im_float16_to_float32(((int8_t*)data)[i])); break;
                    default: break;
                }
            }
        }
        break;
        case NORM_INF:
        {
            for (int i = 0; i < total(); i++)
            {
                switch (type)
                {
                    case IM_DT_INT8:    result = std::max(result, (float)((uint8_t*)data)[i]); break;
                    case IM_DT_INT16:   result = std::max(result, (float)((uint16_t*)data)[i]); break;
                    case IM_DT_INT32:   result = std::max(result, (float)std::abs(((int32_t*)data)[i])); break;  
                    case IM_DT_INT64:   result = std::max(result, (float)std::abs(((int64_t*)data)[i])); break;
                    case IM_DT_FLOAT32: result = std::max(result, (float)std::abs(((float*)data)[i])); break;
                    case IM_DT_FLOAT64: result = std::max(result, (float)std::abs(((double*)data)[i])); break;
                    case IM_DT_FLOAT16: result = std::max(result, (float)std::abs(im_float16_to_float32(((int8_t*)data)[i]))); break;
                    default: break;
                }
            }
        }
        break;
        default: break;
    }
    return result;
}

// min/max
template<typename T> 
inline void ImMat::minmax(T* vmin, T* vmax, int* imin, int* imax)
{
    assert(device == IM_DD_CPU);
    T _vmin = std::numeric_limits<T>::max(), _vmax = std::numeric_limits<T>::min();
    for (int i = 0; i < total(); i++)
    {
        switch (type)
        {
            case IM_DT_INT8:
                if (((uint8_t*)data)[i] > _vmax) { _vmax = ((uint8_t*)data)[i]; if (imax) *imax = i; }
                if (((uint8_t*)data)[i] < _vmin) { _vmin = ((uint8_t*)data)[i]; if (imin) *imin = i; }
                break;
            case IM_DT_INT16:
                if (((uint16_t*)data)[i] > _vmax) { _vmax = ((uint16_t*)data)[i]; if (imax) *imax = i; }
                if (((uint16_t*)data)[i] < _vmin) { _vmin = ((uint16_t*)data)[i]; if (imin) *imin = i; }
                break;
            case IM_DT_INT32:
                if (((int32_t*)data)[i] > _vmax) { _vmax = ((int32_t*)data)[i]; if (imax) *imax = i; }
                if (((int32_t*)data)[i] < _vmin) { _vmin = ((int32_t*)data)[i]; if (imin) *imin = i; }
                break;
            case IM_DT_INT64:
                if (((int64_t*)data)[i] > _vmax) { _vmax = ((int64_t*)data)[i]; if (imax) *imax = i; }
                if (((int64_t*)data)[i] < _vmin) { _vmin = ((int64_t*)data)[i]; if (imin) *imin = i; }
                break;
            case IM_DT_FLOAT32:
                if (((float*)data)[i] > _vmax) { _vmax = ((float*)data)[i]; if (imax) *imax = i; }
                if (((float*)data)[i] < _vmin) { _vmin = ((float*)data)[i]; if (imin) *imin = i; }
                break;
            case IM_DT_FLOAT64:
                if (((double*)data)[i] > _vmax) { _vmax = ((double*)data)[i]; if (imax) *imax = i; }
                if (((double*)data)[i] < _vmin) { _vmin = ((double*)data)[i]; if (imin) *imin = i; }
                break;
            case IM_DT_FLOAT16:
                if (im_float16_to_float32(((int16_t*)data)[i]) > im_float16_to_float32(_vmax)) { _vmax = ((int16_t*)data)[i]; if (imax) *imax = i; }
                if (im_float16_to_float32(((int16_t*)data)[i]) < im_float16_to_float32(_vmin)) { _vmin = ((int16_t*)data)[i]; if (imin) *imin = i; }
                break;
            default: break;
        }
    }
    if (vmin) *vmin = _vmin;
    if (vmax) *vmax = _vmax;
}

// normalize
template<typename T> 
inline void ImMat::normalize(T vmin, T vmax, int norm_type)
{
    assert(device == IM_DD_CPU);
    assert(total() > 0);
    double scale = 1., shift = 0.;
    switch (norm_type)
    {
        case NORM_MINMAX:
        {
            T _smin, _smax;
            double smin, smax, dmin, dmax;
            minmax(&_smin, &_smax);
            smin = (double)_smin;
            smax = (double)_smax;
            if (type == IM_DT_FLOAT16)
            {
                dmin = std::min(im_float16_to_float32(vmin), im_float16_to_float32(vmax));
                dmax = std::max(im_float16_to_float32(vmin), im_float16_to_float32(vmax));
            }
            else
            {
                dmin = std::min(vmin, vmax);
                dmax = std::max(vmin, vmax);
            }

            scale = (dmax - dmin)*(smax - smin > DBL_EPSILON ? 1./(smax - smin) : 0);
            shift = dmin - smin * scale;
            for (int i = 0; i < total(); i++)
            {
                switch (type)
                {
                    case IM_DT_INT8:    ((uint8_t*)data)[i] = (uint8_t)(((uint8_t*)data)[i] * scale + shift); break;
                    case IM_DT_INT16:   ((uint16_t*)data)[i] = (uint16_t)(((uint16_t*)data)[i] * scale + shift); break;
                    case IM_DT_INT32:   ((int32_t*)data)[i] = (int32_t)(((int32_t*)data)[i] * scale + shift); break;
                    case IM_DT_INT64:   ((int64_t*)data)[i] = (int64_t)(((int64_t*)data)[i] * scale + shift); break;
                    case IM_DT_FLOAT32: ((float*)data)[i] = (float)(((float*)data)[i] * scale + shift); break;
                    case IM_DT_FLOAT64: ((double*)data)[i] = (double)(((double*)data)[i] * scale + shift); break;
                    case IM_DT_FLOAT16: ((int16_t*)data)[i] = im_float32_to_float16(im_float16_to_float32(((int16_t*)data)[i]) * scale + shift); break;
                    break;
                    default: break;
                }
            }
        }
        break;
        case NORM_INF:
        case NORM_L1:
        case NORM_L2:
            // TODO::Dicky
        break;
        default: break;
    }
}

// vconcat
inline ImMat ImMat::vconcat(const ImMat& m)
{
    assert(device == IM_DD_CPU);
    assert(m.w == w);
    assert(m.c == c);
    assert(m.type == type);
    ImMat dst;
    if (c == 1 && w == 1)
        dst.create_type(h + m.h, (ImDataType)type);
    else if (c == 1)
        dst.create_type(w, h + m.h, (ImDataType)type);
    else
        dst.create_type(w, h + m.h, c, (ImDataType)type);
    
    memcpy(dst.data, data, total() * elemsize);
    memcpy((int8_t *)dst.data + total() * elemsize, m.data, m.total() * m.elemsize);

    return dst;
}

// hconcat
inline ImMat ImMat::hconcat(const ImMat& m)
{
    assert(device == IM_DD_CPU);
    assert(m.h == h);
    assert(m.c == c);
    assert(m.type == type);
    ImMat dst;
    if (c == 1 && h == 1)
        dst.create_type(w + m.w, (ImDataType)type);
    else if (c == 1)
        dst.create_type(w + m.w, h, (ImDataType)type);
    else
        dst.create_type(w + m.w, h, c, (ImDataType)type);
    if (dims == 1)
    {
        for (int _w = 0; _w < w + m.w; _w++)
        {
            switch (type)
            {
                case IM_DT_INT8:    dst.at<int8_t>(_w)  = _w < w ? at<int8_t>(_w)   : m.at<int8_t>(_w - w);  break;
                case IM_DT_INT16:   dst.at<int16_t>(_w) = _w < w ? at<int16_t>(_w)  : m.at<int16_t>(_w - w); break;
                case IM_DT_INT32:   dst.at<int32_t>(_w) = _w < w ? at<int32_t>(_w)  : m.at<int32_t>(_w - w); break;
                case IM_DT_INT64:   dst.at<int64_t>(_w) = _w < w ? at<int64_t>(_w)  : m.at<int64_t>(_w - w); break;
                case IM_DT_FLOAT32: dst.at<float>(_w)   = _w < w ? at<float>(_w)    : m.at<float>(_w - w);   break;
                case IM_DT_FLOAT64: dst.at<double>(_w)  = _w < w ? at<double>(_w)   : m.at<double>(_w - w);  break;
                case IM_DT_FLOAT16: dst.at<int16_t>(_w) = _w < w ? at<int16_t>(_w)  : m.at<int16_t>(_w - w); break; 
                default: break;
            }
        }
    }
    else if (dims == 2)
    {
        for (int _h = 0; _h < h; _h++)
        {
            for (int _w = 0; _w < w + m.w; _w++)
            {
                switch (type)
                {
                    case IM_DT_INT8:    dst.at<int8_t>(_w, _h)  = _w < w ? at<int8_t>(_w, _h)   : m.at<int8_t>(_w - w, _h);  break;
                    case IM_DT_INT16:   dst.at<int16_t>(_w, _h) = _w < w ? at<int16_t>(_w, _h)  : m.at<int16_t>(_w - w, _h); break;
                    case IM_DT_INT32:   dst.at<int32_t>(_w, _h) = _w < w ? at<int32_t>(_w, _h)  : m.at<int32_t>(_w - w, _h); break;
                    case IM_DT_INT64:   dst.at<int64_t>(_w, _h) = _w < w ? at<int64_t>(_w, _h)  : m.at<int64_t>(_w - w, _h); break;
                    case IM_DT_FLOAT32: dst.at<float>(_w, _h)   = _w < w ? at<float>(_w, _h)    : m.at<float>(_w - w, _h);   break;
                    case IM_DT_FLOAT64: dst.at<double>(_w, _h)  = _w < w ? at<double>(_w, _h)   : m.at<double>(_w - w, _h);  break;
                    case IM_DT_FLOAT16: dst.at<int16_t>(_w, _h) = _w < w ? at<int16_t>(_w, _h)  : m.at<int16_t>(_w - w, _h); break; 
                    default: break;
                }
            }
        }
    }
    else if (dims == 3)
    {
        for (int _c = 0; _c < c; _c++)
        {
            for (int _h = 0; _h < h; _h++)
            {
                for (int _w = 0; _w < w + m.w; _w++)
                {
                    switch (type)
                    {
                        case IM_DT_INT8:    dst.at<int8_t>(_w, _h, _c)  = _w < w ? at<int8_t>(_w, _h, _c)   : m.at<int8_t>(_w - w, _h, _c);  break;
                        case IM_DT_INT16:   dst.at<int16_t>(_w, _h, _c) = _w < w ? at<int16_t>(_w, _h, _c)  : m.at<int16_t>(_w - w, _h, _c); break;
                        case IM_DT_INT32:   dst.at<int32_t>(_w, _h, _c) = _w < w ? at<int32_t>(_w, _h, _c)  : m.at<int32_t>(_w - w, _h, _c); break;
                        case IM_DT_INT64:   dst.at<int64_t>(_w, _h, _c) = _w < w ? at<int64_t>(_w, _h, _c)  : m.at<int64_t>(_w - w, _h, _c); break;
                        case IM_DT_FLOAT32: dst.at<float>(_w, _h, _c)   = _w < w ? at<float>(_w, _h, _c)    : m.at<float>(_w - w, _h, _c);   break;
                        case IM_DT_FLOAT64: dst.at<double>(_w, _h, _c)  = _w < w ? at<double>(_w, _h, _c)   : m.at<double>(_w - w, _h, _c);  break;
                        case IM_DT_FLOAT16: dst.at<int16_t>(_w, _h, _c) = _w < w ? at<int16_t>(_w, _h, _c)  : m.at<int16_t>(_w - w, _h, _c); break; 
                        default: break;
                    }
                }
            }
        }
    }
    return dst;
}
// convert type
inline ImMat ImMat::convert(ImDataType t, float scale) const
{
    assert(device == IM_DD_CPU);
    assert(total() > 0);
    ImMat m;
    m.create_type(w, h, c, t);
    m.elempack = elempack;
    for (int i = 0; i < total(); i++)
    {
        double value = 0;
        switch (type)
        {
            case IM_DT_INT8: value = ((uint8_t*)data)[i]; break;
            case IM_DT_INT16: value = ((uint16_t*)data)[i]; break;
            case IM_DT_INT32: value = ((int32_t*)data)[i]; break;
            case IM_DT_INT64: value = ((int64_t*)data)[i]; break;
            case IM_DT_FLOAT32: value = ((float*)data)[i]; break;
            case IM_DT_FLOAT64: value = ((double*)data)[i]; break;
            case IM_DT_FLOAT16:  value = im_float16_to_float32(((int16_t*)data)[i]); break;
            default: break;
        }
        value *= scale;
        switch (t)
        {
            case IM_DT_INT8: ((uint8_t*)m.data)[i] = value; break;
            case IM_DT_INT16: ((uint16_t*)m.data)[i] = value; break;
            case IM_DT_INT32: ((int32_t*)m.data)[i] = value; break;
            case IM_DT_INT64:  ((int64_t*)m.data)[i] = value; break;
            case IM_DT_FLOAT32: ((float*)m.data)[i] = value; break;
            case IM_DT_FLOAT64: ((double*)m.data)[i] = value; break;
            case IM_DT_FLOAT16: ((int16_t*)m.data)[i] = im_float32_to_float16(value); break;
            default: break;
        }
    }
    return m;
}

} // namespace ImGui 

// mat utils
namespace ImGui
{
// Kalman filter
class IMMAT_API ImKalman
{
public:
    ImKalman() {};
    ImKalman(int state_size, int mea_size);
    ~ImKalman() {};

public:
    void initiate(int state_size, int mea_size);
    void covariance(float noise_covariance, float measurement_noise_covariance);
    void update(ImMat& Y);
    ImMat& predicted();

public:
    ImMat statePre;            //预测状态矩阵(x'(k)) x(k) = A*x(k - 1) + B * u(k)
    ImMat statePost;           //状态估计修正矩阵(x(k)) x(k) = x'(k) + K(k)*(z(k) - H * x'(k)) ： 1 * 8
    ImMat transitionMatrix;    //转移矩阵(A)  ： 8 * 8
    ImMat controMatrix;        //控制矩阵(B)
    ImMat measurementMatrix;   //测量矩阵(H) ：4 * 8
    ImMat processNoiseCov;     //预测模型噪声协方差矩阵(Q) ：8 * 8
    ImMat measurementNoiseCov; //测量噪声协方差矩阵(R)  ： 4 * 4
    ImMat errorCovPre;         //转移噪声矩阵(P'(k)) p'(k) = A * p(k - 1) * At + Q 
    ImMat K;                   //kalman增益矩阵 K = p'(k) * Ht * inv(H * p'(k) * Ht + R)
    ImMat errorCovPost;        //转移噪声修正矩阵(p(k)) p(k) = (I - K(k) * H) * p'(k)  ： 8 * 8
};

IMMAT_API ImMat getPerspectiveTransform(const ImPoint src[], const ImPoint dst[]);
IMMAT_API ImMat getPerspectiveTransform(const ImMat src, const ImMat dst);
IMMAT_API ImMat getAffineTransform(const ImPoint src[], const ImPoint dst[]);
IMMAT_API ImMat getAffineTransform(int sw, int sh, int dw, int dh, float x_offset, float y_offset, float x_scale, float y_scale, float angle);
IMMAT_API void  getAffineParam(const ImMat& M, float& x_offset, float& y_offset, float& x_scale, float& y_scale, float& angle);
IMMAT_API ImMat similarTransform(const ImMat& src, const ImMat& dst);
IMMAT_API ImMat calcCovarMatrix(std::vector<ImPoint>& vertices, ImPoint& avgPos);
IMMAT_API ImMat calcCovarMatrix(std::vector<ImPoint3D>& vertices, ImPoint3D& avgPos);
IMMAT_API void jacobiSolver(ImMat M, std::vector<float> &eVal, std::vector<ImPoint> &eVec, float precision = 1e-5, float iteration = 1e4);
IMMAT_API void jacobiSolver(ImMat M, std::vector<float> &eVal, std::vector<ImPoint3D> &eVec, float precision = 1e-5, float iteration = 1e4);
IMMAT_API void schmidtOrthogonal(ImPoint &u, ImPoint &v);
IMMAT_API void schmidtOrthogonal(ImPoint3D &u, ImPoint3D &v, ImPoint3D &w);
IMMAT_API void calcCenterDimension(std::vector<ImPoint> &vertices, std::vector<ImPoint> &axis, ImPoint &center, ImPoint &halfDimension);
IMMAT_API void calcCenterDimension(std::vector<ImPoint3D> &vertices, std::vector<ImPoint3D> &axis, ImPoint3D &center, ImPoint3D &halfDimension);
IMMAT_API void calcOrientedBoundingBox(std::vector<ImPoint> &vertices, std::vector<ImPoint> &axis, ImPoint &center, ImPoint &halfDimension);
IMMAT_API void calcOrientedBoundingBox(std::vector<ImPoint3D> &vertices, std::vector<ImPoint3D> &axis, ImPoint3D &center, ImPoint3D &halfDimension);
IMMAT_API void findContours(const ImMat& src, std::vector<std::vector<ImPoint>>& contours);

// draw utils
IMMAT_API ImMat MatResize(const ImMat& mat, const ImSize size, float sw = 1.0, float sh = 1.0);
IMMAT_API ImMat MatRotate(const ImMat& mat, float angle);
IMMAT_API ImMat MatWarpAffine(const ImMat& mat, const ImMat& M, ImSize dsize);
IMMAT_API ImMat MatWarpPerspective(const ImMat& src, const ImMat& M, ImSize dsize, ImInterpolateMode mode = IM_INTERPOLATE_NEAREST);
IMMAT_API ImMat GrayToImage(const ImMat& mat);
IMMAT_API ImMat GrayInfernoMap(const ImMat& mat);
IMMAT_API ImMat CreateTextMat(const char* str, const ImPixel& color, const ImPixel& bk_color = ImPixel(0,0,0,0), float scale = 1.f, bool square = false);
IMMAT_API void  DrawTextToMat(ImMat& mat, const ImPoint pos, const char* str, const ImPixel& color, float scale = 1.f);
IMMAT_API void  ImageMatCopyTo(const ImMat& src, ImMat& dst, ImPoint pos);
} // namespace ImGui

// fast-global-smooth filter
namespace ImGui
{
IMMAT_API void prepareBLFKernel(std::vector<float>& BLFKernel, const double sigma);
IMMAT_API void solve_tridiagonal_in_place_destructive(float x[], const size_t N, const float a[], const float b[], float c[]);
IMMAT_API void FGS_simple(float* image, float* joint_image, const int width, const int height, const int nChannels, 
                const int nChannels_guide, const float sigma, const float lambda, const int solver_iteration, const float solver_attenuation);
} // namespace ImGui

#endif /* __IMMAT_H__ */