#pragma once
#include <cstdint>
#include <algorithm>
#include <functional>
#if SIMD_ARCH_X86
#include <immintrin.h>
#endif
#include "CpuUtils.h"

namespace SimdOpt
{
void Initialize();

struct GlobalVars
{
    static bool HAS_FP16;
    static bool HAS_FMA3;
};

uint32_t GetSimdPtrAlignmentWidth(SysUtils::CpuFeature eFea);

enum class StoreMode
{
    ADDR_UNALIGNED = 0,
    ADDR_ALIGNED = 1,
    ADDR_ALIGNED_NOCACHE = 2
};

union int32_suf
{
    int32_t  i;
    uint32_t u;
    float    f;
};

class float16_t
{
public:
#if SIMD_COMPILER_HAS_FP16_TYPE
    float16_t() : h(0) {}
    explicit float16_t(float x) { h = (__fp16)x; }
    operator float() const { return (float)h; }
    static float16_t fromBits(uint16_t w)
    {
        Cv16suf u;
        u.u = w;
        float16_t result;
        result.h = u.h;
        return result;
    }
    static float16_t zero()
    {
        float16_t result;
        result.h = (__fp16)0;
        return result;
    }
    uint16_t bits() const
    {
        Cv16suf u;
        u.h = h;
        return u.u;
    }
protected:
    __fp16 h;

#else
    float16_t();
    explicit float16_t(float x);

    operator float() const
    { return storef32Func(w); }

    static float16_t fromBits(uint16_t b)
    {
        float16_t result;
        result.w = b;
        return result;
    }

    static float16_t zero()
    {
        float16_t result;
        result.w = (uint16_t)0;
        return result;
    }

    uint16_t bits() const { return w; }

protected:
    static uint16_t loadf32_intrin(float x);
    static uint16_t loadf32_c(float x);
    static float storef32_intrin(uint16_t x);
    static float storef32_c(uint16_t x);
    static std::function<uint16_t(float)> loadf32Func;
    static std::function<float(uint16_t)> storef32Func;

protected:
    uint16_t w;

#endif
};

inline uint32_t trailingZeros32(uint32_t value)
{
#if defined(_MSC_VER)
#if (_MSC_VER < 1700) || defined(_M_ARM) || defined(_M_ARM64)
    uint32_t long index = 0;
    _BitScanForward(&index, value);
    return (uint32_t int)index;
#elif defined(__clang__)
    // clang-cl doesn't export _tzcnt_u32 for non BMI systems
    return value ? __builtin_ctz(value) : 32;
#else
    return _tzcnt_u32(value);
#endif
#elif defined(__GNUC__) || defined(__GNUG__)
    return __builtin_ctz(value);
#elif defined(__ICC) || defined(__INTEL_COMPILER)
    return _bit_scan_forward(value);
#elif defined(__clang__)
    return llvm.cttz.i32(value, true);
#else
    static const int MultiplyDeBruijnBitPosition[32] = {
        0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
        31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9 };
    return MultiplyDeBruijnBitPosition[((uint32_t)((value & -value) * 0x077CB531U)) >> 27];
#endif
}
}

#define SIMD_NOP(a) (a)

#define SIMD_CAT(x, y) x ## y

#ifdef __GNUC__
#  define DECL_ALIGNED(x) __attribute__ ((aligned(x)))
#elif defined _MSC_VER
#  define DECL_ALIGNED(x) __declspec(align(x))
#else
#  define DECL_ALIGNED(x)
#endif

#define SIMD_SCOPE_BEGIN(mode) namespace mode {
#define SIMD_SCOPE_END }
#define SIMD_DEFINE_SCOPVARS(mode) \
    struct ScopeVars { \
        static const SysUtils::CpuFeature CPU_FEATURE {SysUtils::CpuFeature::mode}; \
    };


#if SIMD_ARCH_X86

#undef VXPREFIX
#define VXPREFIX(func) v256##func

#define INTRIN_MODE AVX2
#undef USE_AVX2
#define USE_AVX2
#include "SimdIntrinsic/IntrinAvx.h"
#include "SimdIntrinsic/IntrinVxFuncDef.h"
#undef USE_AVX2
#undef INTRIN_MODE

#define INTRIN_MODE AVX
#include "SimdIntrinsic/IntrinAvx.h"
#include "SimdIntrinsic/IntrinVxFuncDef.h"
#undef INTRIN_MODE

#undef VXPREFIX
#define VXPREFIX(func) v##func

#define INTRIN_MODE SSE4_1
#undef USE_SSE4_1
#define USE_SSE4_1
#include "SimdIntrinsic/IntrinSseEm.h"
#include "SimdIntrinsic/IntrinSse.hpp"
#include "SimdIntrinsic/IntrinVxFuncDef.h"
#undef USE_SSE4_1
#undef INTRIN_MODE

#define INTRIN_MODE SSSE3
#undef USE_SSSE3
#define USE_SSSE3
#include "SimdIntrinsic/IntrinSseEm.h"
#include "SimdIntrinsic/IntrinSse.hpp"
#include "SimdIntrinsic/IntrinVxFuncDef.h"
#undef USE_SSSE3
#undef INTRIN_MODE

#define INTRIN_MODE SSE3
#undef USE_SSE3
#define USE_SSE3
#include "SimdIntrinsic/IntrinSseEm.h"
#include "SimdIntrinsic/IntrinSse.hpp"
#include "SimdIntrinsic/IntrinVxFuncDef.h"
#undef USE_SSE3
#undef INTRIN_MODE

#define INTRIN_MODE SSE
#include "SimdIntrinsic/IntrinSseEm.h"
#include "SimdIntrinsic/IntrinSse.hpp"
#include "SimdIntrinsic/IntrinVxFuncDef.h"
#undef INTRIN_MODE

#endif // ~SIMD_ARCH_X86

#undef VXPREFIX
#define VXPREFIX(func) v##func
#define INTRIN_MODE NONE
#include "SimdIntrinsic/IntrinCpp.h"
#include "SimdIntrinsic/IntrinVxFuncDef.h"
#undef INTRIN_MODE
