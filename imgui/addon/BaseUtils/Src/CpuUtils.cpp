#include <unordered_map>
#include <mutex>
#include <string>
#include <iostream>
#include <sstream>
#include <list>
#include <thread>
#include "CpuUtils.h"
#include "SimdOpt.h"

using namespace std;

#if defined __ANDROID__ || defined __unix__ || defined __FreeBSD__ || defined __OpenBSD__ || defined __HAIKU__ || defined __Fuchsia__
#  include <unistd.h>
#  include <fcntl.h>
#if defined __QNX__
#  include <sys/elf.h>
#else
#  include <elf.h>
#endif
#if defined __ANDROID__ || defined __linux__
#  include <linux/auxvec.h>
#endif
#endif

#if defined __ANDROID__ && defined HAVE_CPUFEATURES
#  include <cpu-features.h>
#endif

#if (defined __ppc64__ || defined __PPC64__) && defined __unix__
# include "sys/auxv.h"
# ifndef AT_HWCAP2
#   define AT_HWCAP2 26
# endif
# ifndef PPC_FEATURE2_ARCH_2_07
#   define PPC_FEATURE2_ARCH_2_07 0x80000000
# endif
# ifndef PPC_FEATURE2_ARCH_3_00
#   define PPC_FEATURE2_ARCH_3_00 0x00800000
# endif
# ifndef PPC_FEATURE_HAS_VSX
#   define PPC_FEATURE_HAS_VSX 0x00000080
# endif
#endif

#if defined __MACH__ && defined __APPLE__
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif


namespace SysUtils
{
#if defined _MSC_VER && (defined _M_IX86 || defined _M_X64)
#   if _MSC_VER >= 1400
        #include <intrin.h>
        #define X86_64_CPUID __cpuidex
#   else
        #error "Required MSVS 2005+"
#   endif
#elif defined __GNUC__ && (defined __i386__ || defined __x86_64__)
    static void _cpuid(int* cpuidData, int reg_eax, int reg_ecx)
    {
        int __eax = reg_eax, __ebx = 0, __ecx = reg_ecx, __edx = 0;
#   if !defined(__PIC__) \
        || defined(__x86_64__) || __GNUC__ >= 5 \
        || defined(__clang__) || defined(__INTEL_COMPILER)
        __asm__("cpuid\n\t"
                : "+a" (__eax), "=b" (__ebx), "+c" (__ecx), "=d" (__edx)
        );
#   elif defined(__i386__)
        __asm__("xchg{l}\t{%%}ebx, %1\n\t"
                "cpuid\n\t"
                "xchg{l}\t{%%}ebx, %1\n\t"
                : "+a" (__eax), "=&r" (__ebx), "+c" (__ecx), "=d" (__edx)
        );
#   else
        #error "Configuration error"
#   endif
        cpuidData[0] = __eax; cpuidData[1] = __ebx; cpuidData[2] = __ecx; cpuidData[3] = __edx;
    }
    #define X86_64_CPUID _cpuid
#endif

static bool _CPU_FEATURE_TABLE_INITED = false;
static mutex _CHECK_CPU_FEATURE_LOCK;
struct _CpuFeatureInfo
{
    _CpuFeatureInfo() : name(""), enable(false) {}
    _CpuFeatureInfo(const string& _name) : name(_name), enable(false) {}
    string name;
    bool enable;
};
static unordered_map<CpuFeature, _CpuFeatureInfo> _CPU_FEATURE_TABLE;

static void InitializeCpuFeatureTable()
{
    if (_CPU_FEATURE_TABLE_INITED)
        return;

    _CPU_FEATURE_TABLE.emplace(CpuFeature::MMX,             "MMX");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::SSE,             "SSE");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::SSE2,            "SSE2");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::SSE3,            "SSE3");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::SSSE3,           "SSSE3");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::SSE4_1,          "SSE4_1");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::SSE4_2,          "SSE4_2");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::POPCNT,          "POPCNT");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::FP16,            "FP16");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX,             "AVX");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX2,            "AVX2");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::FMA3,            "FMA3");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_512F,        "AVX_512F");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_512BW,       "AVX_512BW");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_512CD,       "AVX_512CD");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_512DQ,       "AVX_512DQ");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_512ER,       "AVX_512ER");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_512IFMA,     "AVX_512IFMA");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_512PF,       "AVX_512PF");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_512VBMI,     "AVX_512VBMI");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_512VL,       "AVX_512VL");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_512VBMI2,    "AVX_512VBMI2");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_512VNNI,     "AVX_512VNNI");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_512BITALG,   "AVX_512BITALG");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_512VPOPCNTDQ,"AVX_512VPOPCNTDQ");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_5124VNNIW,   "AVX_5124VNNIW");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX_5124FMAPS,   "AVX_5124FMAPS");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX512_COMMON,   "AVX512_COMMON");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX512_KNL,      "AVX512_KNL");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX512_KNM,      "AVX512_KNM");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX512_SKX,      "AVX512_SKX");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX512_CNL,      "AVX512_CNL");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX512_CLX,      "AVX512_CLX");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::AVX512_ICL,      "AVX512_ICL");

    _CPU_FEATURE_TABLE.emplace(CpuFeature::NEON,            "NEON");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::MSA,             "MSA");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::RISCVV,          "RISCVV");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::VSX,             "VSX");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::VSX3,            "VSX3");
    _CPU_FEATURE_TABLE.emplace(CpuFeature::RVV,             "RVV");

#ifdef X86_64_CPUID
    int cpuidData[4] = { 0, 0, 0, 0 };
    int cpuidDataEx[4] = { 0, 0, 0, 0 };
    X86_64_CPUID(cpuidData, 1, 0);
    int x86Family = (cpuidData[0]>>8) & 15;
    if (x86Family >= 6)
    {
        _CPU_FEATURE_TABLE[CpuFeature::MMX]             .enable = (cpuidData[3] & (1<<23)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::SSE]             .enable = (cpuidData[3] & (1<<25)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::SSE2]            .enable = (cpuidData[3] & (1<<26)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::SSE3]            .enable = (cpuidData[2] & (1<<0)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::SSSE3]           .enable = (cpuidData[2] & (1<<9)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::FMA3]            .enable = (cpuidData[2] & (1<<12)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::SSE4_1]          .enable = (cpuidData[2] & (1<<19)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::SSE4_2]          .enable = (cpuidData[2] & (1<<20)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::POPCNT]          .enable = (cpuidData[2] & (1<<23)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX]             .enable = (cpuidData[2] & (1<<28)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::FP16]            .enable = (cpuidData[2] & (1<<29)) != 0;

        X86_64_CPUID(cpuidDataEx, 7, 0);
        _CPU_FEATURE_TABLE[CpuFeature::AVX2]            .enable = (cpuidDataEx[1] & (1<<5)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_512F]        .enable = (cpuidDataEx[1] & (1<<16)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_512DQ]       .enable = (cpuidDataEx[1] & (1<<17)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_512IFMA]     .enable = (cpuidDataEx[1] & (1<<21)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_512PF]       .enable = (cpuidDataEx[1] & (1<<26)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_512ER]       .enable = (cpuidDataEx[1] & (1<<27)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_512CD]       .enable = (cpuidDataEx[1] & (1<<28)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_512BW]       .enable = (cpuidDataEx[1] & (1<<30)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_512VL]       .enable = (cpuidDataEx[1] & (1<<31)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_512VBMI]     .enable = (cpuidDataEx[2] & (1<<1))  != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_512VBMI2]    .enable = (cpuidDataEx[2] & (1<<6))  != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_512VNNI]     .enable = (cpuidDataEx[2] & (1<<11)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_512BITALG]   .enable = (cpuidDataEx[2] & (1<<12)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_512VPOPCNTDQ].enable = (cpuidDataEx[2] & (1<<14)) != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_5124VNNIW]   .enable = (cpuidDataEx[3] & (1<<2))  != 0;
        _CPU_FEATURE_TABLE[CpuFeature::AVX_5124FMAPS]   .enable = (cpuidDataEx[3] & (1<<3))  != 0;

        bool hasAvxSupport = true;
        bool hasAvx512Support = true;
        if (!(cpuidData[2] & (1<<27)))
            hasAvxSupport = false;
        else
        {
            int xcr0 = 0;
        #ifdef _XCR_XFEATURE_ENABLED_MASK // requires immintrin.h
            xcr0 = (int)_xgetbv(_XCR_XFEATURE_ENABLED_MASK);
        #elif defined __GNUC__ && (defined __i386__ || defined __x86_64__)
            __asm__ ("xgetbv\n\t" : "=a" (xcr0) : "c" (0) : "%edx" );
        #endif
            if ((xcr0 & 0x6) != 0x6)
                hasAvxSupport = false; // YMM registers
            if ((xcr0 & 0xe6) != 0xe6)
                hasAvx512Support = false; // ZMM registers
        }

        if (!hasAvxSupport)
        {
            _CPU_FEATURE_TABLE[CpuFeature::AVX] .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::FP16].enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX2].enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::FMA3].enable = false;
        }
        if (!hasAvxSupport || !hasAvx512Support)
        {
            _CPU_FEATURE_TABLE[CpuFeature::AVX_512F]        .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_512BW]       .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_512CD]       .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_512DQ]       .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_512ER]       .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_512IFMA]     .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_512PF]       .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_512VBMI]     .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_512VL]       .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_512VBMI2]    .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_512VNNI]     .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_512BITALG]   .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_512VPOPCNTDQ].enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_5124VNNIW]   .enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX_5124FMAPS]   .enable = false;
        }

        _CPU_FEATURE_TABLE[CpuFeature::AVX512_COMMON].enable = _CPU_FEATURE_TABLE[CpuFeature::AVX_512F].enable && _CPU_FEATURE_TABLE[CpuFeature::AVX_512CD].enable;
        if (_CPU_FEATURE_TABLE[CpuFeature::AVX512_COMMON].enable)
        {
            _CPU_FEATURE_TABLE[CpuFeature::AVX512_KNL].enable = _CPU_FEATURE_TABLE[CpuFeature::AVX_512ER].enable  && _CPU_FEATURE_TABLE[CpuFeature::AVX_512PF].enable;
            _CPU_FEATURE_TABLE[CpuFeature::AVX512_KNM].enable = _CPU_FEATURE_TABLE[CpuFeature::AVX512_KNL].enable && _CPU_FEATURE_TABLE[CpuFeature::AVX_5124FMAPS].enable
                    && _CPU_FEATURE_TABLE[CpuFeature::AVX_5124VNNIW].enable && _CPU_FEATURE_TABLE[CpuFeature::AVX_512VPOPCNTDQ].enable;
            _CPU_FEATURE_TABLE[CpuFeature::AVX512_SKX].enable = _CPU_FEATURE_TABLE[CpuFeature::AVX_512BW].enable && _CPU_FEATURE_TABLE[CpuFeature::AVX_512DQ].enable
                    && _CPU_FEATURE_TABLE[CpuFeature::AVX_512VL].enable;
            _CPU_FEATURE_TABLE[CpuFeature::AVX512_CNL].enable = _CPU_FEATURE_TABLE[CpuFeature::AVX512_SKX].enable && _CPU_FEATURE_TABLE[CpuFeature::AVX_512IFMA].enable
                    && _CPU_FEATURE_TABLE[CpuFeature::AVX_512VBMI].enable;
            _CPU_FEATURE_TABLE[CpuFeature::AVX512_CLX].enable = _CPU_FEATURE_TABLE[CpuFeature::AVX512_SKX].enable && _CPU_FEATURE_TABLE[CpuFeature::AVX_512VNNI].enable;
            _CPU_FEATURE_TABLE[CpuFeature::AVX512_ICL].enable = _CPU_FEATURE_TABLE[CpuFeature::AVX512_SKX].enable && _CPU_FEATURE_TABLE[CpuFeature::AVX_512IFMA].enable
                    && _CPU_FEATURE_TABLE[CpuFeature::AVX_512VBMI].enable && _CPU_FEATURE_TABLE[CpuFeature::AVX_512VNNI].enable && _CPU_FEATURE_TABLE[CpuFeature::AVX_512VBMI2].enable
                    && _CPU_FEATURE_TABLE[CpuFeature::AVX_512BITALG].enable && _CPU_FEATURE_TABLE[CpuFeature::AVX_512VPOPCNTDQ].enable;
        }
        else
        {
            _CPU_FEATURE_TABLE[CpuFeature::AVX512_KNL].enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX512_KNM].enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX512_SKX].enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX512_CNL].enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX512_CLX].enable = false;
            _CPU_FEATURE_TABLE[CpuFeature::AVX512_ICL].enable = false;
        }
    }
#endif // ~X86_64_CPUID

#if defined __linux__ || defined __FreeBSD__ || defined __QNX__
#  ifdef __aarch64__
    _CPU_FEATURE_TABLE[CpuFeature::NEON].enable = true;
    _CPU_FEATURE_TABLE[CpuFeature::FP16].enable = true;
#  elif defined __arm__ && !defined __FreeBSD__
    int cpufile = open("/proc/self/auxv", O_RDONLY);
    if (cpufile >= 0)
    {
        Elf32_auxv_t auxv;
        const size_t size_auxv_t = sizeof(auxv);
        while ((size_t)read(cpufile, &auxv, size_auxv_t) == size_auxv_t)
        {
            if (auxv.a_type == AT_HWCAP)
            {
                _CPU_FEATURE_TABLE[CpuFeature::NEON].enable = (auxv.a_un.a_val & 4096) != 0;
                _CPU_FEATURE_TABLE[CpuFeature::FP16].enable = (auxv.a_un.a_val & 2) != 0;
                break;
            }
        }
        close(cpufile);
    }
#  endif
#elif (defined __clang__ || defined __APPLE__)
#  if (defined __ARM_NEON__ || (defined __ARM_NEON && defined __aarch64__))
    _CPU_FEATURE_TABLE[CpuFeature::NEON].enable = true;
#  endif
#  if (defined __ARM_FP  && (((__ARM_FP & 0x2) != 0) && defined __ARM_NEON__))
    _CPU_FEATURE_TABLE[CpuFeature::FP16].enable = true;
#  endif
#endif
#if defined _M_ARM64
    _CPU_FEATURE_TABLE[CpuFeature::NEON].enable = true;
#endif
#ifdef __riscv_vector
    _CPU_FEATURE_TABLE[CpuFeature::RISCVV].enable = true;
#endif
#ifdef __mips_msa
    _CPU_FEATURE_TABLE[CpuFeature::MSA].enable = true;
#endif

#if (defined __ppc64__ || defined __PPC64__) && defined __linux__
    unsigned int hwcap = getauxval(AT_HWCAP);
    if (hwcap & PPC_FEATURE_HAS_VSX) {
        hwcap = getauxval(AT_HWCAP2);
        if (hwcap & PPC_FEATURE2_ARCH_3_00) {
            _CPU_FEATURE_TABLE[CpuFeature::VSX].enable = _CPU_FEATURE_TABLE[CpuFeature::VSX3].enable = true;
        } else {
            _CPU_FEATURE_TABLE[CpuFeature::VSX].enable = (hwcap & PPC_FEATURE2_ARCH_2_07) != 0;
        }
    }
#elif (defined __ppc64__ || defined __PPC64__) && defined __FreeBSD__
    unsigned long hwcap = 0;
    elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
    if (hwcap & PPC_FEATURE_HAS_VSX) {
        elf_aux_info(AT_HWCAP2, &hwcap, sizeof(hwcap));
        if (hwcap & PPC_FEATURE2_ARCH_3_00) {
            _CPU_FEATURE_TABLE[CpuFeature::VSX].enable = _CPU_FEATURE_TABLE[CpuFeature::VSX3].enable = true;
        } else {
            _CPU_FEATURE_TABLE[CpuFeature::VSX].enable = (hwcap & PPC_FEATURE2_ARCH_2_07) != 0;
        }
    }
#endif

#if defined __riscv && defined __riscv_vector
    _CPU_FEATURE_TABLE[CpuFeature::RVV].enable = true;
#endif

    cout << "Cpu feature table initialized, enabled features:" << endl;
    for (auto& elem : _CPU_FEATURE_TABLE)
    {
        if (!elem.second.enable)
            continue;
        cout << " " << elem.second.name << ",";
    }
    cout << endl;
    _CPU_FEATURE_TABLE_INITED = true;
}

bool CpuChecker::HasFeature(CpuFeature fea)
{
    lock_guard<mutex> lk(_CHECK_CPU_FEATURE_LOCK);
    if (!_CPU_FEATURE_TABLE_INITED)
        InitializeCpuFeatureTable();
    auto iter = _CPU_FEATURE_TABLE.find(fea);
    if (iter == _CPU_FEATURE_TABLE.end())
        return false;
    return iter->second.enable;
}
}