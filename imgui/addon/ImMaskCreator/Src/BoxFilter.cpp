#include <cassert>
#include <vector>
#include "MathUtils.h"
#include "MatFilter.h"
#include "CpuUtils.h"

#if SIMD_ARCH_X86
#include "SimdOpt.h"
#define INTRIN_MODE AVX2
#undef USE_AVX2
#define USE_AVX2
#include "BoxFilter.Simd.h"
#undef USE_AVX2
#undef INTRIN_MODE

#define INTRIN_MODE AVX
#include "BoxFilter.Simd.h"
#undef INTRIN_MODE

#define INTRIN_MODE SSE4_1
#undef USE_SSE4_1
#define USE_SSE4_1
#include "BoxFilter.Simd.h"
#undef USE_SSE4_1
#undef INTRIN_MODE

#define INTRIN_MODE SSSE3
#undef USE_SSSE3
#define USE_SSSE3
#include "BoxFilter.Simd.h"
#undef USE_SSSE3
#undef INTRIN_MODE

#define INTRIN_MODE SSE3
#undef USE_SSE3
#define USE_SSE3
#include "BoxFilter.Simd.h"
#undef USE_SSE3
#undef INTRIN_MODE

#define INTRIN_MODE SSE
#include "BoxFilter.Simd.h"
#undef INTRIN_MODE

#endif // ~SIMD_ARCH_X86

#if !defined(__EMSCRIPTEN__)
#include "SimdOpt.h"
#define INTRIN_MODE NONE
#include "BoxFilter.Simd.h"
#undef INTRIN_MODE
#endif

using namespace std;
using namespace MathUtils;
using namespace SysUtils;

namespace MatUtils
{
RowFilter::Holder GetRowSumFilter(ImDataType eSrcType, ImDataType eSumType, int ksize, int anchor)
{
    if (anchor < 0)
        anchor = ksize/2;

    #define GET_SIMD_ROW_SUM_FILTER(srcType, sumType, ksize, anchor) \
        if (srcType == IM_DT_INT8) { \
            if (sumType == IM_DT_INT16) { \
                using RowSumFilterSimd = RowSum<uint8_t, uint16_t>; \
                return RowFilter::Holder(new RowSumFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    RowSumFilterSimd* ptr = dynamic_cast<RowSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            else if (sumType == IM_DT_INT32) { \
                using RowSumFilterSimd = RowSum<uint8_t, int32_t>; \
                return RowFilter::Holder(new RowSumFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    RowSumFilterSimd* ptr = dynamic_cast<RowSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            else if (sumType == IM_DT_FLOAT64) { \
                using RowSumFilterSimd = RowSum<uint8_t, double>; \
                return RowFilter::Holder(new RowSumFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    RowSumFilterSimd* ptr = dynamic_cast<RowSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
        } \
        else if (srcType == IM_DT_INT16) { \
            if (sumType == IM_DT_INT32) { \
                using RowSumFilterSimd = RowSum<uint16_t, int32_t>; \
                return RowFilter::Holder(new RowSumFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    RowSumFilterSimd* ptr = dynamic_cast<RowSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            else if (sumType == IM_DT_FLOAT64) { \
                using RowSumFilterSimd = RowSum<uint16_t, double>; \
                return RowFilter::Holder(new RowSumFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    RowSumFilterSimd* ptr = dynamic_cast<RowSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
        } \
        else if (srcType == IM_DT_INT32) { \
            if (sumType == IM_DT_INT32) { \
                using RowSumFilterSimd = RowSum<int32_t, int32_t>; \
                return RowFilter::Holder(new RowSumFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    RowSumFilterSimd* ptr = dynamic_cast<RowSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            else if (sumType == IM_DT_FLOAT64) { \
                using RowSumFilterSimd = RowSum<int32_t, double>; \
                return RowFilter::Holder(new RowSumFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    RowSumFilterSimd* ptr = dynamic_cast<RowSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
        } \
        else if (srcType == IM_DT_FLOAT32) { \
            if (sumType == IM_DT_FLOAT64) { \
                using RowSumFilterSimd = RowSum<float, double>; \
                return RowFilter::Holder(new RowSumFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    RowSumFilterSimd* ptr = dynamic_cast<RowSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
        } \
        else if (srcType == IM_DT_FLOAT64) { \
            if (sumType == IM_DT_FLOAT64) { \
                using RowSumFilterSimd = RowSum<double, double>; \
                return RowFilter::Holder(new RowSumFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    RowSumFilterSimd* ptr = dynamic_cast<RowSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
        }

#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_SIMD_ROW_SUM_FILTER(eSrcType, eSumType, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_SIMD_ROW_SUM_FILTER(eSrcType, eSumType, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_SIMD_ROW_SUM_FILTER(eSrcType, eSumType, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_SIMD_ROW_SUM_FILTER(eSrcType, eSumType, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_SIMD_ROW_SUM_FILTER(eSrcType, eSumType, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_SIMD_ROW_SUM_FILTER(eSrcType, eSumType, ksize, anchor);
        }
        else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_SIMD_ROW_SUM_FILTER(eSrcType, eSumType, ksize, anchor);
        }
#endif
    return nullptr;
}

ColumnFilter::Holder GetColumnSumFilter(int eSumType, int eDstType, int ksize, int anchor, double scale)
{
    if (anchor < 0)
        anchor = ksize/2;

    #define GET_SIMD_COLUMN_SUM_FILTER(sumType, dstType, ksize, anchor, scale) \
        if (dstType == IM_DT_INT8) { \
            if (sumType == IM_DT_INT16) { \
                using ColumnSumFilterSimd = ColumnSum<uint16_t, uint8_t>; \
                return ColumnFilter::Holder(new ColumnSumFilterSimd(ksize, anchor, scale), [] (ColumnFilter* p) { \
                    ColumnSumFilterSimd* ptr = dynamic_cast<ColumnSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            else if (sumType == IM_DT_INT32) { \
                using ColumnSumFilterSimd = ColumnSum<int32_t, uint8_t>; \
                return ColumnFilter::Holder(new ColumnSumFilterSimd(ksize, anchor, scale), [] (ColumnFilter* p) { \
                    ColumnSumFilterSimd* ptr = dynamic_cast<ColumnSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            else if (sumType == IM_DT_FLOAT64) { \
                using ColumnSumFilterSimd = ColumnSum<double, uint8_t>; \
                return ColumnFilter::Holder(new ColumnSumFilterSimd(ksize, anchor, scale), [] (ColumnFilter* p) { \
                    ColumnSumFilterSimd* ptr = dynamic_cast<ColumnSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
        } \
        else if (dstType == IM_DT_INT16) { \
            if (sumType == IM_DT_INT32) { \
                using ColumnSumFilterSimd = ColumnSum<int32_t, uint16_t>; \
                return ColumnFilter::Holder(new ColumnSumFilterSimd(ksize, anchor, scale), [] (ColumnFilter* p) { \
                    ColumnSumFilterSimd* ptr = dynamic_cast<ColumnSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            else if (sumType == IM_DT_FLOAT64) { \
                using ColumnSumFilterSimd = ColumnSum<double, uint16_t>; \
                return ColumnFilter::Holder(new ColumnSumFilterSimd(ksize, anchor, scale), [] (ColumnFilter* p) { \
                    ColumnSumFilterSimd* ptr = dynamic_cast<ColumnSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
        } \
        else if (dstType == IM_DT_INT32) { \
            if (sumType == IM_DT_INT32) { \
                using ColumnSumFilterSimd = ColumnSum<int32_t, int32_t>; \
                return ColumnFilter::Holder(new ColumnSumFilterSimd(ksize, anchor, scale), [] (ColumnFilter* p) { \
                    ColumnSumFilterSimd* ptr = dynamic_cast<ColumnSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            else if (sumType == IM_DT_FLOAT64) { \
                using ColumnSumFilterSimd = ColumnSum<double, int32_t>; \
                return ColumnFilter::Holder(new ColumnSumFilterSimd(ksize, anchor, scale), [] (ColumnFilter* p) { \
                    ColumnSumFilterSimd* ptr = dynamic_cast<ColumnSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
        } \
        else if (dstType == IM_DT_FLOAT32) { \
            if (sumType == IM_DT_FLOAT64) { \
                using ColumnSumFilterSimd = ColumnSum<double, float>; \
                return ColumnFilter::Holder(new ColumnSumFilterSimd(ksize, anchor, scale), [] (ColumnFilter* p) { \
                    ColumnSumFilterSimd* ptr = dynamic_cast<ColumnSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
        } \
        else if (dstType == IM_DT_FLOAT64) { \
            if (sumType == IM_DT_FLOAT64) { \
                using ColumnSumFilterSimd = ColumnSum<double, double>; \
                return ColumnFilter::Holder(new ColumnSumFilterSimd(ksize, anchor, scale), [] (ColumnFilter* p) { \
                    ColumnSumFilterSimd* ptr = dynamic_cast<ColumnSumFilterSimd*>(p); \
                    delete ptr; }); \
            } \
        }

#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_SIMD_COLUMN_SUM_FILTER(eSumType, eDstType, ksize, anchor, scale);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_SIMD_COLUMN_SUM_FILTER(eSumType, eDstType, ksize, anchor, scale);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_SIMD_COLUMN_SUM_FILTER(eSumType, eDstType, ksize, anchor, scale);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_SIMD_COLUMN_SUM_FILTER(eSumType, eDstType, ksize, anchor, scale);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_SIMD_COLUMN_SUM_FILTER(eSumType, eDstType, ksize, anchor, scale);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_SIMD_COLUMN_SUM_FILTER(eSumType, eDstType, ksize, anchor, scale);
        }
        else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_SIMD_COLUMN_SUM_FILTER(eSumType, eDstType, ksize, anchor, scale);
        }
#endif
    return nullptr;
}

FilterEngine::Holder CreateBoxFilter(ImDataType eSrcType, ImDataType eDstType, int iCh,
        const Size2i& szKsize, const Point2i& ptAnchor, bool bNormalize,
        BorderTypes eBorderType, const ImGui::ImMat& _mBorderValue)
{
    ImGui::ImMat mBorderValue;
    if (_mBorderValue.empty())
    {
        mBorderValue.create_type(1, 1, iCh, eSrcType);
        if (eSrcType == IM_DT_INT8)
            mBorderValue.fill<uint8_t>(0);
        else if (eSrcType == IM_DT_INT16 || eSrcType == IM_DT_INT16_BE)
            mBorderValue.fill<uint16_t>(0);
        else if (eSrcType == IM_DT_INT32)
            mBorderValue.fill<int32_t>(0);
        else if (eSrcType == IM_DT_INT64)
            mBorderValue.fill<int64_t>(0);
        else if (eSrcType == IM_DT_FLOAT32)
            mBorderValue.fill<float>(0);
        else if (eSrcType == IM_DT_FLOAT64)
            mBorderValue.fill<double>(0);
        else
            throw runtime_error("UNSUPPORTED data type!");
    }
    else
    {
        mBorderValue = _mBorderValue;
    }

    ImDataType eSumType = IM_DT_FLOAT64;
    const int srcDepth = IM_DEPTH(eSrcType);
    const bool bIsSrcFloat = eSrcType == IM_DT_FLOAT16 || eSrcType == IM_DT_FLOAT32 || eSrcType == IM_DT_FLOAT64;
    if (srcDepth == 8 && IM_DEPTH(eDstType) == 8 && szKsize.x*szKsize.y <= 256 )
        eSumType = IM_DT_INT16;
    else if (srcDepth <= 32 && !bIsSrcFloat && (!bNormalize
        || szKsize.x*szKsize.y <= (srcDepth == 8 ? (1<<23) : srcDepth == 16 ? (1 << 15) : (1 << 16))))
        eSumType = IM_DT_INT32;
    auto hRowFilter = GetRowSumFilter(eSrcType, eSumType, szKsize.x, ptAnchor.x);
    auto hColumnFilter = GetColumnSumFilter(eSumType, eDstType, szKsize.y, ptAnchor.y,
            bNormalize ? 1./(szKsize.x*szKsize.y) : 1);

    FilterEngine::Holder hFilterEngine = FilterEngine::CreateInstance();
    hFilterEngine->Init(nullptr, hRowFilter, hColumnFilter, eSrcType, eDstType, eSumType, iCh, eBorderType, eBorderType, mBorderValue);
    return hFilterEngine;
}

ImGui::ImMat Blur(const ImGui::ImMat& mInput, const Size2i& szKsize, const Point2i& ptAnchor)
{
    auto hBoxFilter = CreateBoxFilter(mInput.type, mInput.type, mInput.c, szKsize, ptAnchor, true, BorderTypes::BORDER_CONSTANT, ImGui::ImMat());
    Point2i ptOfs(0, 0);
    Size2i szSize(mInput.w, mInput.h);
    auto mOutput = hBoxFilter->Apply(mInput, szSize, ptOfs);
    return mOutput;
}
}