#include "SimdOpt.h"

using namespace SysUtils;

namespace SimdOpt
{
void Initialize()
{
    GlobalVars::HAS_FP16 = CpuChecker::HasFeature(CpuFeature::FP16);
    GlobalVars::HAS_FMA3 = CpuChecker::HasFeature(CpuFeature::FMA3);
}

uint32_t GetSimdPtrAlignmentWidth(CpuFeature eFea)
{
    uint32_t alignment = 1;
    switch (eFea)
    {
    case CpuFeature::AVX_512F:
    case CpuFeature::AVX_512BW:
    case CpuFeature::AVX_512CD:
    case CpuFeature::AVX_512DQ:
    case CpuFeature::AVX_512ER:
    case CpuFeature::AVX_512IFMA:
    case CpuFeature::AVX_512PF:
    case CpuFeature::AVX_512VBMI:
    case CpuFeature::AVX_512VL:
    case CpuFeature::AVX_512VBMI2:
    case CpuFeature::AVX_512VNNI:
    case CpuFeature::AVX_512BITALG:
    case CpuFeature::AVX_512VPOPCNTDQ:
    case CpuFeature::AVX_5124VNNIW:
    case CpuFeature::AVX_5124FMAPS:
    case CpuFeature::AVX512_COMMON:
    case CpuFeature::AVX512_KNL:
    case CpuFeature::AVX512_KNM:
    case CpuFeature::AVX512_SKX:
    case CpuFeature::AVX512_CNL:
    case CpuFeature::AVX512_CLX:
    case CpuFeature::AVX512_ICL:
        alignment = 64;
        break;

    case CpuFeature::AVX:
    case CpuFeature::AVX2:
    case CpuFeature::FMA3:
        alignment = 32;
        break;

    case CpuFeature::MMX:
    case CpuFeature::SSE:
    case CpuFeature::SSE2:
    case CpuFeature::SSE3:
    case CpuFeature::SSSE3:
    case CpuFeature::SSE4_1:
    case CpuFeature::SSE4_2:
    case CpuFeature::FP16:
        alignment = 16;
        break;

    default:
        break;
    }
    return alignment;
}

#if !SIMD_COMPILER_HAS_FP16_TYPE
float16_t::float16_t()
{
    if (!loadf32Func || !storef32Func)
    {
#if SIMD_ARCH_X86
        const bool bCpuHasFp16 = CpuChecker::HasFeature(CpuFeature::FP16);
        loadf32Func = bCpuHasFp16 ? float16_t::loadf32_intrin : float16_t::loadf32_c;
        storef32Func = bCpuHasFp16 ? float16_t::storef32_intrin : float16_t::storef32_c;
#else
        loadf32Func = float16_t::loadf32_c;
        storef32Func = float16_t::storef32_c;
#endif
    }
}

float16_t::float16_t(float x)
{
    if (!loadf32Func || !storef32Func)
    {
#if SIMD_ARCH_X86
        const bool bCpuHasFp16 = CpuChecker::HasFeature(CpuFeature::FP16);
        loadf32Func = bCpuHasFp16 ? float16_t::loadf32_intrin : float16_t::loadf32_c;
        storef32Func = bCpuHasFp16 ? float16_t::storef32_intrin : float16_t::storef32_c;
#else
        loadf32Func = float16_t::loadf32_c;
        storef32Func = float16_t::storef32_c;
#endif
    }
    w = loadf32Func(x);
}

#if SIMD_ARCH_X86
uint16_t float16_t::loadf32_intrin(float x)
{
    __m128 v = _mm_load_ss(&x);
    return (uint16_t)_mm_cvtsi128_si32(_mm_cvtps_ph(v, 0));
}
#endif

uint16_t float16_t::loadf32_c(float x)
{
    int32_suf in;
    in.f = x;
    uint32_t sign = in.u & 0x80000000;
    in.u ^= sign;

    uint16_t v;
    if( in.u >= 0x47800000 )
        v = (uint16_t)(in.u > 0x7f800000 ? 0x7e00 : 0x7c00);
    else
    {
        if (in.u < 0x38800000)
        {
            in.f += 0.5f;
            v = (uint16_t)(in.u - 0x3f000000);
        }
        else
        {
            uint32_t t = in.u + 0xc8000fff;
            v = (uint16_t)((t + ((in.u >> 13) & 1)) >> 13);
        }
    }
    return (uint16_t)(v | (sign >> 16));
}

#if SIMD_ARCH_X86
float float16_t::storef32_intrin(uint16_t x)
{
    float f;
    _mm_store_ss(&f, _mm_cvtph_ps(_mm_cvtsi32_si128(x)));
    return f;
}
#endif

float float16_t::storef32_c(uint16_t x)
{
    int32_suf out;
    uint32_t t = ((x & 0x7fff) << 13) + 0x38000000;
    uint32_t sign = (x & 0x8000) << 16;
    uint32_t e = x & 0x7c00;
    out.u = t + (1 << 23);
    out.u = (e >= 0x7c00 ? t + 0x38000000 :
            e == 0 ? (static_cast<void>(out.f -= 6.103515625e-05f), out.u) : t) | sign;
    return out.f;
}

std::function<uint16_t(float)> float16_t::loadf32Func;
std::function<float(uint16_t)> float16_t::storef32Func;
#endif
}

bool SimdOpt::GlobalVars::HAS_FP16 = false;
bool SimdOpt::GlobalVars::HAS_FMA3 = false;
