#pragma once
#include <cstdint>
#include <memory>
#include <mutex>

namespace SysUtils
{
enum class CpuFeature
{
    NONE,
    MMX,
    SSE,
    SSE2,
    SSE3,
    SSSE3,
    SSE4_1,
    SSE4_2,
    POPCNT,
    FP16,
    AVX,
    AVX2,
    FMA3,
    AVX_512F,
    AVX_512BW,
    AVX_512CD,
    AVX_512DQ,
    AVX_512ER,
    AVX_512IFMA,
    AVX_512PF,
    AVX_512VBMI,
    AVX_512VL,
    AVX_512VBMI2,
    AVX_512VNNI,
    AVX_512BITALG,
    AVX_512VPOPCNTDQ,
    AVX_5124VNNIW,
    AVX_5124FMAPS,
    AVX512_COMMON,
    AVX512_KNL,
    AVX512_KNM,
    AVX512_SKX,
    AVX512_CNL,
    AVX512_CLX,
    AVX512_ICL,
    NEON,
    MSA,
    RISCVV,
    VSX,
    VSX3,
    RVV,
};

struct CpuChecker
{
    static bool HasFeature(CpuFeature fea);
};
}