#pragma once
#include <stddef.h>

#if (defined _WIN32 && !(defined __MINGW32__))
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#if defined __ANDROID__ || defined __linux__
#include <sched.h> // cpu_set_t
#endif
#include <imgui.h>

#ifndef EXPORT_API
#ifdef _WIN32
#define EXPORT_API IMGUI_API
#else
#define EXPORT_API
#endif
#endif

namespace ImGui 
{
class EXPORT_API CpuSet
{
public:
    CpuSet();
    void enable(int cpu);
    void disable(int cpu);
    void disable_all();
    bool is_enabled(int cpu) const;
    int num_enabled() const;

public:
#if (defined _WIN32 && !(defined __MINGW32__))
    ULONG_PTR mask;
#endif
#if defined __ANDROID__ || defined __linux__
    cpu_set_t cpu_set;
#endif
#if __APPLE__
    unsigned int policy;
#endif
};

// test optional cpu features
// edsp = armv7 edsp
EXPORT_API int cpu_support_arm_edsp();
// neon = armv7 neon or aarch64 asimd
EXPORT_API int cpu_support_arm_neon();
// vfpv4 = armv7 fp16 + fma
EXPORT_API int cpu_support_arm_vfpv4();
// asimdhp = aarch64 asimd half precision
EXPORT_API int cpu_support_arm_asimdhp();
// cpuid = aarch64 cpuid info
EXPORT_API int cpu_support_arm_cpuid();
// asimddp = aarch64 asimd dot product
EXPORT_API int cpu_support_arm_asimddp();
// asimdfhm = aarch64 asimd fhm
EXPORT_API int cpu_support_arm_asimdfhm();
// bf16 = aarch64 bf16
EXPORT_API int cpu_support_arm_bf16();
// i8mm = aarch64 i8mm
EXPORT_API int cpu_support_arm_i8mm();
// sve = aarch64 sve
EXPORT_API int cpu_support_arm_sve();
// sve2 = aarch64 sve2
EXPORT_API int cpu_support_arm_sve2();
// svebf16 = aarch64 svebf16
EXPORT_API int cpu_support_arm_svebf16();
// svei8mm = aarch64 svei8mm
EXPORT_API int cpu_support_arm_svei8mm();
// svef32mm = aarch64 svef32mm
EXPORT_API int cpu_support_arm_svef32mm();

// avx = x86 sse3
EXPORT_API int cpu_support_x86_sse3();
// avx = x86 ssse3
EXPORT_API int cpu_support_x86_ssse3();
// avx = x86 sse4.1
EXPORT_API int cpu_support_x86_sse41();
// avx = x86 sse4.2
EXPORT_API int cpu_support_x86_sse42();
// avx = x86 avx
EXPORT_API int cpu_support_x86_avx();
// fma = x86 fma
EXPORT_API int cpu_support_x86_fma();
// xop = x86 xop
EXPORT_API int cpu_support_x86_xop();
// f16c = x86 f16c
EXPORT_API int cpu_support_x86_f16c();
// avx2 = x86 avx2 + fma + f16c
EXPORT_API int cpu_support_x86_avx2();
// avx_vnni = x86 avx vnni
EXPORT_API int cpu_support_x86_avx_vnni();
// avx512 = x86 avx512f + avx512cd + avx512bw + avx512dq + avx512vl
EXPORT_API int cpu_support_x86_avx512();
// avx512_vnni = x86 avx512 vnni
EXPORT_API int cpu_support_x86_avx512_vnni();
// avx512_bf16 = x86 avx512 bf16
EXPORT_API int cpu_support_x86_avx512_bf16();
// avx512_fp16 = x86 avx512 fp16
EXPORT_API int cpu_support_x86_avx512_fp16();

// lsx = loongarch lsx
EXPORT_API int cpu_support_loongarch_lsx();
// lasx = loongarch lasx
EXPORT_API int cpu_support_loongarch_lasx();

// msa = mips mas
EXPORT_API int cpu_support_mips_msa();
// mmi = loongson mmi
EXPORT_API int cpu_support_loongson_mmi();

// v = riscv vector
EXPORT_API int cpu_support_riscv_v();
// zfh = riscv half-precision float
EXPORT_API int cpu_support_riscv_zfh();
// vlenb = riscv vector length in bytes
EXPORT_API int cpu_riscv_vlenb();

// cpu info
EXPORT_API int get_cpu_count();
EXPORT_API int get_little_cpu_count();
EXPORT_API int get_big_cpu_count();

EXPORT_API int get_physical_cpu_count();
EXPORT_API int get_physical_little_cpu_count();
EXPORT_API int get_physical_big_cpu_count();

// cpu l2 varies from 64k to 1M, but l3 can be zero
EXPORT_API int get_cpu_level2_cache_size();
EXPORT_API int get_cpu_level3_cache_size();

// bind all threads on little clusters if powersave enabled
// affects HMP arch cpu like ARM big.LITTLE
// only implemented on android at the moment
// switching powersave is expensive and not thread-safe
// 0 = all cores enabled(default)
// 1 = only little clusters enabled
// 2 = only big clusters enabled
// return 0 if success for setter function
EXPORT_API int get_cpu_powersave();
EXPORT_API int set_cpu_powersave(int powersave);

// convenient wrapper
EXPORT_API const CpuSet& get_cpu_thread_affinity_mask(int powersave);

// set explicit thread affinity
EXPORT_API int set_cpu_thread_affinity(const CpuSet& thread_affinity_mask);

// runtime thread affinity info
EXPORT_API int is_current_thread_running_on_a53_a55();

// misc function wrapper for openmp routines
EXPORT_API int get_omp_num_threads();
EXPORT_API void set_omp_num_threads(int num_threads);

EXPORT_API int get_omp_dynamic();
EXPORT_API void set_omp_dynamic(int dynamic);

EXPORT_API int get_omp_thread_num();

EXPORT_API int get_kmp_blocktime();
EXPORT_API void set_kmp_blocktime(int time_ms);

#if IMGUI_BUILD_EXAMPLE
EXPORT_API void CPUInfo();
#endif
} // namespace ImGui

