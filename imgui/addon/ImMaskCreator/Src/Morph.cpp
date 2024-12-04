#include <cassert>
#include <list>
#include "MathUtils.h"
#include "ThreadUtils.h"
#include "MatFilter.h"

#if SIMD_ARCH_X86
#include "SimdOpt.h"
#define INTRIN_MODE AVX2
#undef USE_AVX2
#define USE_AVX2
#include "Morph.Simd.h"
#undef USE_AVX2
#undef INTRIN_MODE

#define INTRIN_MODE AVX
#include "Morph.Simd.h"
#undef INTRIN_MODE

#define INTRIN_MODE SSE4_1
#undef USE_SSE4_1
#define USE_SSE4_1
#include "Morph.Simd.h"
#undef USE_SSE4_1
#undef INTRIN_MODE

#define INTRIN_MODE SSSE3
#undef USE_SSSE3
#define USE_SSSE3
#include "Morph.Simd.h"
#undef USE_SSSE3
#undef INTRIN_MODE

#define INTRIN_MODE SSE3
#undef USE_SSE3
#define USE_SSE3
#include "Morph.Simd.h"
#undef USE_SSE3
#undef INTRIN_MODE

#define INTRIN_MODE SSE
#include "Morph.Simd.h"
#undef INTRIN_MODE

#endif // ~SIMD_ARCH_X86

#if !defined(__EMSCRIPTEN__)
#include "SimdOpt.h"
#define INTRIN_MODE NONE
#include "Morph.Simd.h"
#undef INTRIN_MODE
#endif

using namespace std;
using namespace MathUtils;
using namespace SysUtils;

namespace MatUtils
{
template<typename U, typename V>
static bool IsRectContainsPonit(const Rect<U>& rt, const Point<V>& pt)
{
    const auto rightBottom = rt.rightBottom();
    return rt.leftTop.x <= pt.x && pt.x < rightBottom.x && rt.leftTop.y <= pt.y && pt.y < rightBottom.y;
}

template<typename U, typename V>
static inline Point<U> NormalizeAnchor(const Point<U>& anchor, const Size<V>& ksize)
{
    Point<U> result;
    result.x = anchor.x==-1 ? ksize.x/2 : anchor.x;
    result.y = anchor.y==-1 ? ksize.y/2 : anchor.y;
    assert(IsRectContainsPonit(Recti(0, 0, ksize.x, ksize.y), result));
    return result;
}

ImGui::ImMat GetStructuringElement(MorphShape eShape, const Size2i& szKsize, const Point2i& _ptAnchor)
{
    int i, j;
    int r = 0, c = 0;
    double inv_r2 = 0;

    Point2i ptAnchor = NormalizeAnchor(_ptAnchor, szKsize);

    if (szKsize == Size2i(1, 1))
        eShape = MORPH_RECT;

    if (eShape == MORPH_ELLIPSE)
    {
        r = szKsize.y/2;
        c = szKsize.x/2;
        inv_r2 = r ? 1./((double)r*r) : 0;
    }

    ImGui::ImMat elem;
    elem.create_type(szKsize.x, szKsize.y, IM_DT_INT8);
    for (i = 0; i < szKsize.y; i++)
    {
        uint8_t* ptr = (uint8_t*)elem.data+i*elem.w*elem.elemsize;
        int j1 = 0, j2 = 0;

        if (eShape == MORPH_RECT || (eShape == MORPH_CROSS && i == ptAnchor.y))
            j2 = szKsize.x;
        else if (eShape == MORPH_CROSS)
            j1 = ptAnchor.x, j2 = j1 + 1;
        else
        {
            int32_t dy = i - r;
            if (std::abs(dy) <= r)
            {
                int32_t dx = SaturateCast<int32_t>(c*std::sqrt((r*r - dy*dy)*inv_r2));
                j1 = std::max(c - dx, 0);
                j2 = std::min(c + dx + 1, szKsize.x);
            }
        }

        for (j = 0; j < j1; j++)
            ptr[j] = 0;
        for (; j < j2; j++)
            ptr[j] = 1;
        for (; j < szKsize.x; j++)
            ptr[j] = 0;
    }

    return elem;
}



RowFilter::Holder GetMorphologyRowFilter(int op, ImDataType type, int ksize, int anchor)
{
    if (anchor < 0)
        anchor = ksize/2;

    if (op == 0 /*MORPH_ERODE*/)
    {
        #define GET_SIMD_ERODE_ROW_FILTER(type, ksize, anchor) \
            if (type == IM_DT_INT8) { \
                using ErodeRowVecSimd = MorphRowVec<VMin<v_uint8>>; \
                using MorphRowFilterSimd = MorphRowFilter<MinOp<uint8_t>, ErodeRowVecSimd>; \
                return RowFilter::Holder(new MorphRowFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    MorphRowFilterSimd* ptr = dynamic_cast<MorphRowFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_INT16) { \
                using ErodeRowVecSimd = MorphRowVec<VMin<v_uint16>>; \
                using MorphRowFilterSimd = MorphRowFilter<MinOp<uint16_t>, ErodeRowVecSimd>; \
                return RowFilter::Holder(new MorphRowFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    MorphRowFilterSimd* ptr = dynamic_cast<MorphRowFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_INT32) { \
                using ErodeRowVecSimd = MorphRowVec<VMin<v_int32>>; \
                using MorphRowFilterSimd = MorphRowFilter<MinOp<int32_t>, ErodeRowVecSimd>; \
                return RowFilter::Holder(new MorphRowFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    MorphRowFilterSimd* ptr = dynamic_cast<MorphRowFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_FLOAT32) { \
                using ErodeRowVecSimd = MorphRowVec<VMin<v_float32>>; \
                using MorphRowFilterSimd = MorphRowFilter<MinOp<float>, ErodeRowVecSimd>; \
                return RowFilter::Holder(new MorphRowFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    MorphRowFilterSimd* ptr = dynamic_cast<MorphRowFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_FLOAT64) { \
                using ErodeRowVecSimd = MorphRowVec<VMin<v_float64>>; \
                using MorphRowFilterSimd = MorphRowFilter<MinOp<double>, ErodeRowVecSimd>; \
                return RowFilter::Holder(new MorphRowFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    MorphRowFilterSimd* ptr = dynamic_cast<MorphRowFilterSimd*>(p); \
                    delete ptr; }); \
            }

#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_SIMD_ERODE_ROW_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_SIMD_ERODE_ROW_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_SIMD_ERODE_ROW_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_SIMD_ERODE_ROW_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_SIMD_ERODE_ROW_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_SIMD_ERODE_ROW_FILTER(type, ksize, anchor);
        }
        else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_SIMD_ERODE_ROW_FILTER(type, ksize, anchor);
        }
#endif
    }
    else
    {
        #define GET_SIMD_DILATE_ROW_FILTER(type, ksize, anchor) \
            if (type == IM_DT_INT8) { \
                using DilateRowVecSimd = MorphRowVec<VMax<v_uint8>>; \
                using MorphRowFilterSimd = MorphRowFilter<MaxOp<uint8_t>, DilateRowVecSimd>; \
                return RowFilter::Holder(new MorphRowFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    MorphRowFilterSimd* ptr = dynamic_cast<MorphRowFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_INT16) { \
                using DilateRowVecSimd = MorphRowVec<VMax<v_uint16>>; \
                using MorphRowFilterSimd = MorphRowFilter<MaxOp<uint16_t>, DilateRowVecSimd>; \
                return RowFilter::Holder(new MorphRowFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    MorphRowFilterSimd* ptr = dynamic_cast<MorphRowFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_INT32) { \
                using DilateRowVecSimd = MorphRowVec<VMax<v_int32>>; \
                using MorphRowFilterSimd = MorphRowFilter<MaxOp<int32_t>, DilateRowVecSimd>; \
                return RowFilter::Holder(new MorphRowFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    MorphRowFilterSimd* ptr = dynamic_cast<MorphRowFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_FLOAT32) { \
                using DilateRowVecSimd = MorphRowVec<VMax<v_float32>>; \
                using MorphRowFilterSimd = MorphRowFilter<MaxOp<float>, DilateRowVecSimd>; \
                return RowFilter::Holder(new MorphRowFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    MorphRowFilterSimd* ptr = dynamic_cast<MorphRowFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_FLOAT64) { \
                using DilateRowVecSimd = MorphRowVec<VMax<v_float64>>; \
                using MorphRowFilterSimd = MorphRowFilter<MaxOp<double>, DilateRowVecSimd>; \
                return RowFilter::Holder(new MorphRowFilterSimd(ksize, anchor), [] (RowFilter* p) { \
                    MorphRowFilterSimd* ptr = dynamic_cast<MorphRowFilterSimd*>(p); \
                    delete ptr; }); \
            }

#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_SIMD_DILATE_ROW_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_SIMD_DILATE_ROW_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_SIMD_DILATE_ROW_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_SIMD_DILATE_ROW_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_SIMD_DILATE_ROW_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_SIMD_DILATE_ROW_FILTER(type, ksize, anchor);
        }
        else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_SIMD_DILATE_ROW_FILTER(type, ksize, anchor);
        }
#endif
    }
    return nullptr;
}

ColumnFilter::Holder GetMorphologyColumnFilter(int op, ImDataType type, int ksize, int anchor)
{
    if (anchor < 0)
        anchor = ksize/2;

    if (op == 0 /*MORPH_ERODE*/)
    {
        #define GET_SIMD_ERODE_COLUMN_FILTER(type, ksize, anchor) \
            if (type == IM_DT_INT8) { \
                using ErodeColumnVecSimd = MorphColumnVec<VMin<v_uint8>>; \
                using MorphColumnFilterSimd = MorphColumnFilter<MinOp<uint8_t>, ErodeColumnVecSimd>; \
                return ColumnFilter::Holder(new MorphColumnFilterSimd(ksize, anchor), [] (ColumnFilter* p) { \
                    MorphColumnFilterSimd* ptr = dynamic_cast<MorphColumnFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_INT16) { \
                using ErodeColumnVecSimd = MorphColumnVec<VMin<v_uint16>>; \
                using MorphColumnFilterSimd = MorphColumnFilter<MinOp<uint16_t>, ErodeColumnVecSimd>; \
                return ColumnFilter::Holder(new MorphColumnFilterSimd(ksize, anchor), [] (ColumnFilter* p) { \
                    MorphColumnFilterSimd* ptr = dynamic_cast<MorphColumnFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_INT32) { \
                using ErodeColumnVecSimd = MorphColumnVec<VMin<v_int32>>; \
                using MorphColumnFilterSimd = MorphColumnFilter<MinOp<int32_t>, ErodeColumnVecSimd>; \
                return ColumnFilter::Holder(new MorphColumnFilterSimd(ksize, anchor), [] (ColumnFilter* p) { \
                    MorphColumnFilterSimd* ptr = dynamic_cast<MorphColumnFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_FLOAT32) { \
                using ErodeColumnVecSimd = MorphColumnVec<VMin<v_float32>>; \
                using MorphColumnFilterSimd = MorphColumnFilter<MinOp<float>, ErodeColumnVecSimd>; \
                return ColumnFilter::Holder(new MorphColumnFilterSimd(ksize, anchor), [] (ColumnFilter* p) { \
                    MorphColumnFilterSimd* ptr = dynamic_cast<MorphColumnFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_FLOAT64) { \
                using ErodeColumnVecSimd = MorphColumnVec<VMin<v_float64>>; \
                using MorphColumnFilterSimd = MorphColumnFilter<MinOp<double>, ErodeColumnVecSimd>; \
                return ColumnFilter::Holder(new MorphColumnFilterSimd(ksize, anchor), [] (ColumnFilter* p) { \
                    MorphColumnFilterSimd* ptr = dynamic_cast<MorphColumnFilterSimd*>(p); \
                    delete ptr; }); \
            }

#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_SIMD_ERODE_COLUMN_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_SIMD_ERODE_COLUMN_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_SIMD_ERODE_COLUMN_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_SIMD_ERODE_COLUMN_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_SIMD_ERODE_COLUMN_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_SIMD_ERODE_COLUMN_FILTER(type, ksize, anchor);
        }
        else
#endif
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_SIMD_ERODE_COLUMN_FILTER(type, ksize, anchor);
        }
#endif
    }
    else
    {
        #define GET_SIMD_DILATE_COLUMN_FILTER(type, ksize, anchor) \
            if (type == IM_DT_INT8) { \
                using DilateColumnVecSimd = MorphColumnVec<VMax<v_uint8>>; \
                using MorphColumnFilterSimd = MorphColumnFilter<MaxOp<uint8_t>, DilateColumnVecSimd>; \
                return ColumnFilter::Holder(new MorphColumnFilterSimd(ksize, anchor), [] (ColumnFilter* p) { \
                    MorphColumnFilterSimd* ptr = dynamic_cast<MorphColumnFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_INT16) { \
                using DilateColumnVecSimd = MorphColumnVec<VMax<v_uint16>>; \
                using MorphColumnFilterSimd = MorphColumnFilter<MaxOp<uint16_t>, DilateColumnVecSimd>; \
                return ColumnFilter::Holder(new MorphColumnFilterSimd(ksize, anchor), [] (ColumnFilter* p) { \
                    MorphColumnFilterSimd* ptr = dynamic_cast<MorphColumnFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_INT32) { \
                using DilateColumnVecSimd = MorphColumnVec<VMax<v_int32>>; \
                using MorphColumnFilterSimd = MorphColumnFilter<MaxOp<int32_t>, DilateColumnVecSimd>; \
                return ColumnFilter::Holder(new MorphColumnFilterSimd(ksize, anchor), [] (ColumnFilter* p) { \
                    MorphColumnFilterSimd* ptr = dynamic_cast<MorphColumnFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_FLOAT32) { \
                using DilateColumnVecSimd = MorphColumnVec<VMax<v_float32>>; \
                using MorphColumnFilterSimd = MorphColumnFilter<MaxOp<float>, DilateColumnVecSimd>; \
                return ColumnFilter::Holder(new MorphColumnFilterSimd(ksize, anchor), [] (ColumnFilter* p) { \
                    MorphColumnFilterSimd* ptr = dynamic_cast<MorphColumnFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_FLOAT64) { \
                using DilateColumnVecSimd = MorphColumnVec<VMax<v_float64>>; \
                using MorphColumnFilterSimd = MorphColumnFilter<MaxOp<double>, DilateColumnVecSimd>; \
                return ColumnFilter::Holder(new MorphColumnFilterSimd(ksize, anchor), [] (ColumnFilter* p) { \
                    MorphColumnFilterSimd* ptr = dynamic_cast<MorphColumnFilterSimd*>(p); \
                    delete ptr; }); \
            }

#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_SIMD_DILATE_COLUMN_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_SIMD_DILATE_COLUMN_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_SIMD_DILATE_COLUMN_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_SIMD_DILATE_COLUMN_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_SIMD_DILATE_COLUMN_FILTER(type, ksize, anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_SIMD_DILATE_COLUMN_FILTER(type, ksize, anchor);
        }
        else
#endif
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_SIMD_DILATE_COLUMN_FILTER(type, ksize, anchor);
        }
#endif
    }
    return nullptr;
}

MatFilter::Holder GetMorphologyMatFilter(int op, ImDataType type, const ImGui::ImMat& _kernel, const Point2i& _anchor)
{
    if (op == 0 /*MORPH_ERODE*/)
    {
        #define GET_SIMD_ERODE_MAT_FILTER(kernel, anchor) \
            if (type == IM_DT_INT8) { \
                using ErodeMatVecSimd = MorphVec<VMin<v_uint8>>; \
                using MorphFilterSimd = MorphFilter<MinOp<uint8_t>, ErodeMatVecSimd>; \
                return MatFilter::Holder(new MorphFilterSimd(kernel, anchor), [] (MatFilter* p) { \
                    MorphFilterSimd* ptr = dynamic_cast<MorphFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_INT16) { \
                using ErodeMatVecSimd = MorphVec<VMin<v_uint16>>; \
                using MorphFilterSimd = MorphFilter<MinOp<uint16_t>, ErodeMatVecSimd>; \
                return MatFilter::Holder(new MorphFilterSimd(kernel, anchor), [] (MatFilter* p) { \
                    MorphFilterSimd* ptr = dynamic_cast<MorphFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_INT32) { \
                using ErodeMatVecSimd = MorphVec<VMin<v_int32>>; \
                using MorphFilterSimd = MorphFilter<MinOp<int32_t>, ErodeMatVecSimd>; \
                return MatFilter::Holder(new MorphFilterSimd(kernel, anchor), [] (MatFilter* p) { \
                    MorphFilterSimd* ptr = dynamic_cast<MorphFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_FLOAT32) { \
                using ErodeMatVecSimd = MorphVec<VMin<v_float32>>; \
                using MorphFilterSimd = MorphFilter<MinOp<float>, ErodeMatVecSimd>; \
                return MatFilter::Holder(new MorphFilterSimd(kernel, anchor), [] (MatFilter* p) { \
                    MorphFilterSimd* ptr = dynamic_cast<MorphFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_FLOAT64) { \
                using ErodeMatVecSimd = MorphVec<VMin<v_float64>>; \
                using MorphFilterSimd = MorphFilter<MinOp<double>, ErodeMatVecSimd>; \
                return MatFilter::Holder(new MorphFilterSimd(kernel, anchor), [] (MatFilter* p) { \
                    MorphFilterSimd* ptr = dynamic_cast<MorphFilterSimd*>(p); \
                    delete ptr; }); \
            }

#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_SIMD_ERODE_MAT_FILTER(_kernel, _anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_SIMD_ERODE_MAT_FILTER(_kernel, _anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_SIMD_ERODE_MAT_FILTER(_kernel, _anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_SIMD_ERODE_MAT_FILTER(_kernel, _anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_SIMD_ERODE_MAT_FILTER(_kernel, _anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_SIMD_ERODE_MAT_FILTER(_kernel, _anchor);
        }
        else
#endif
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_SIMD_ERODE_MAT_FILTER(_kernel, _anchor);
        }
#endif
    }
    else
    {
        #define GET_SIMD_DILATE_MAT_FILTER(kernel, anchor) \
            if (type == IM_DT_INT8) { \
                using DilateMatVecSimd = MorphVec<VMax<v_uint8>>; \
                using MorphFilterSimd = MorphFilter<MaxOp<uint8_t>, DilateMatVecSimd>; \
                return MatFilter::Holder(new MorphFilterSimd(kernel, anchor), [] (MatFilter* p) { \
                    MorphFilterSimd* ptr = dynamic_cast<MorphFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_INT16) { \
                using DilateMatVecSimd = MorphVec<VMax<v_uint16>>; \
                using MorphFilterSimd = MorphFilter<MaxOp<uint16_t>, DilateMatVecSimd>; \
                return MatFilter::Holder(new MorphFilterSimd(kernel, anchor), [] (MatFilter* p) { \
                    MorphFilterSimd* ptr = dynamic_cast<MorphFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_INT32) { \
                using DilateMatVecSimd = MorphVec<VMax<v_int32>>; \
                using MorphFilterSimd = MorphFilter<MaxOp<int32_t>, DilateMatVecSimd>; \
                return MatFilter::Holder(new MorphFilterSimd(kernel, anchor), [] (MatFilter* p) { \
                    MorphFilterSimd* ptr = dynamic_cast<MorphFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_FLOAT32) { \
                using DilateMatVecSimd = MorphVec<VMax<v_float32>>; \
                using MorphFilterSimd = MorphFilter<MaxOp<float>, DilateMatVecSimd>; \
                return MatFilter::Holder(new MorphFilterSimd(kernel, anchor), [] (MatFilter* p) { \
                    MorphFilterSimd* ptr = dynamic_cast<MorphFilterSimd*>(p); \
                    delete ptr; }); \
            } \
            if (type == IM_DT_FLOAT64) { \
                using DilateMatVecSimd = MorphVec<VMax<v_float64>>; \
                using MorphFilterSimd = MorphFilter<MaxOp<double>, DilateMatVecSimd>; \
                return MatFilter::Holder(new MorphFilterSimd(kernel, anchor), [] (MatFilter* p) { \
                    MorphFilterSimd* ptr = dynamic_cast<MorphFilterSimd*>(p); \
                    delete ptr; }); \
            }

#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_SIMD_DILATE_MAT_FILTER(_kernel, _anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_SIMD_DILATE_MAT_FILTER(_kernel, _anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_SIMD_DILATE_MAT_FILTER(_kernel, _anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_SIMD_DILATE_MAT_FILTER(_kernel, _anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_SIMD_DILATE_MAT_FILTER(_kernel, _anchor);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_SIMD_DILATE_MAT_FILTER(_kernel, _anchor);
        }
        else
#endif
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_SIMD_DILATE_MAT_FILTER(_kernel, _anchor);
        }
#endif
    }
    return nullptr;
}

FilterEngine::Holder CreateMorphologyFilter(
        int op, ImDataType dtype, int iCh, const ImGui::ImMat& mKernel,
        Point2i ptAnchor, BorderTypes eRowBorderType, BorderTypes eColumnBorderType,
        const ImGui::ImMat& _mBorderValue)
{
    const Size2i szKsize(mKernel.w, mKernel.h);
    ptAnchor = NormalizeAnchor(ptAnchor, szKsize);

    RowFilter::Holder hRowFilter;
    ColumnFilter::Holder hColumnFilter;
    MatFilter::Holder hMatFilter;

    if (CountNonZero(mKernel) == mKernel.w*mKernel.h)
    {
        // rectangular structuring element
        hRowFilter = GetMorphologyRowFilter(op, dtype, mKernel.w, ptAnchor.x);
        hColumnFilter = GetMorphologyColumnFilter(op, dtype, mKernel.h, ptAnchor.y);
    }
    else
    {
        hMatFilter = GetMorphologyMatFilter(op, dtype, mKernel, ptAnchor);
    }

    ImGui::ImMat mBorderValue;
    if (_mBorderValue.empty() && (eRowBorderType == BORDER_CONSTANT || eColumnBorderType == BORDER_CONSTANT))
    {
        mBorderValue.create_type(1, 1, iCh, dtype);
        if (op == 0)
        {
            if (dtype == IM_DT_INT8)
                mBorderValue.fill<uint8_t>(UINT8_MAX);
            else if (dtype == IM_DT_INT16 || dtype == IM_DT_INT16_BE)
                mBorderValue.fill<uint16_t>(UINT16_MAX);
            else if (dtype == IM_DT_INT32)
                mBorderValue.fill<int32_t>(INT32_MAX);
            else if (dtype == IM_DT_INT64)
                mBorderValue.fill<int64_t>(INT64_MAX);
            else if (dtype == IM_DT_FLOAT32)
                mBorderValue.fill<float>(numeric_limits<float>::max());
            else if (dtype == IM_DT_FLOAT64)
                mBorderValue.fill<double>(numeric_limits<double>::max());
            else
                throw runtime_error("UNSUPPORTED data type!");
        }
        else
        {
            if (dtype == IM_DT_INT8)
                mBorderValue.fill<uint8_t>(0);
            else if (dtype == IM_DT_INT16 || dtype == IM_DT_INT16_BE)
                mBorderValue.fill<uint16_t>(0);
            else if (dtype == IM_DT_INT32)
                mBorderValue.fill<int32_t>(INT32_MIN);
            else if (dtype == IM_DT_INT64)
                mBorderValue.fill<int64_t>(INT64_MIN);
            else if (dtype == IM_DT_FLOAT32)
                mBorderValue.fill<float>(numeric_limits<float>::min());
            else if (dtype == IM_DT_FLOAT64)
                mBorderValue.fill<double>(numeric_limits<double>::min());
            else
                throw runtime_error("UNSUPPORTED data type!");
        }
    }
    else
    {
        mBorderValue = _mBorderValue;
    }

    FilterEngine::Holder hFilterEngine = FilterEngine::CreateInstance();
    hFilterEngine->Init(hMatFilter, hRowFilter, hColumnFilter, dtype, dtype, dtype, iCh, eRowBorderType, eColumnBorderType, mBorderValue);
    return hFilterEngine;
}

ImGui::ImMat Erode(const ImGui::ImMat& mInput, const ImGui::ImMat& _mKernel, const Point2i& _ptAnchor, int iIterations)
{
    Size2i szKnSize = !_mKernel.empty() ? Size2i(_mKernel.w, _mKernel.h) : Size2i(3, 3);
    Point2i ptAnchor = NormalizeAnchor(_ptAnchor, szKnSize);

    if (iIterations == 0 || _mKernel.w*_mKernel.h == 1)
        return mInput.clone();

    ImGui::ImMat mKernel(_mKernel);
    if (_mKernel.empty())
    {
        mKernel = GetStructuringElement(MORPH_RECT, Size2i(1+iIterations*2, 1+iIterations*2));
        ptAnchor = Point2i(iIterations, iIterations);
        iIterations = 1;
    }
    else if (iIterations > 1 && CountNonZero(mKernel) == mKernel.w*mKernel.h)
    {
        ptAnchor = Point2i(ptAnchor.x*iIterations, ptAnchor.y*iIterations);
        mKernel = GetStructuringElement(MORPH_RECT,
                Size2i(szKnSize.x+(iIterations-1)*(szKnSize.x-1), szKnSize.y+(iIterations-1)*(szKnSize.y-1)), ptAnchor);
        iIterations = 1;
    }

    Point2i ptOfs(0, 0);
    Size2i szSize(mInput.w, mInput.h);
    auto hMorphFilter = CreateMorphologyFilter(0, mInput.type, mInput.c, mKernel, ptAnchor, BORDER_CONSTANT, BORDER_CONSTANT, ImGui::ImMat());
    ImGui::ImMat mOutput = hMorphFilter->Apply(mInput, szSize, ptOfs);
    for (int i = 1; i < iIterations; i++)
        mOutput = hMorphFilter->Apply(mOutput, szSize, ptOfs);
    return mOutput;
}

ImGui::ImMat Dilate(const ImGui::ImMat& mInput, const ImGui::ImMat& _mKernel, const Point2i& _ptAnchor, int iIterations)
{
    Size2i szKnSize = !_mKernel.empty() ? Size2i(_mKernel.w, _mKernel.h) : Size2i(3, 3);
    Point2i ptAnchor = NormalizeAnchor(_ptAnchor, szKnSize);

    if (iIterations == 0 || _mKernel.w*_mKernel.h == 1)
        return mInput.clone();

    ImGui::ImMat mKernel(_mKernel);
    if (_mKernel.empty())
    {
        mKernel = GetStructuringElement(MORPH_RECT, Size2i(1+iIterations*2, 1+iIterations*2));
        ptAnchor = Point2i(iIterations, iIterations);
        iIterations = 1;
    }
    else if (iIterations > 1 && CountNonZero(mKernel) == mKernel.w*mKernel.h)
    {
        ptAnchor = Point2i(ptAnchor.x*iIterations, ptAnchor.y*iIterations);
        mKernel = GetStructuringElement(MORPH_RECT,
                Size2i(szKnSize.x+(iIterations-1)*(szKnSize.x-1), szKnSize.y+(iIterations-1)*(szKnSize.y-1)), ptAnchor);
        iIterations = 1;
    }

    Point2i ptOfs(0, 0);
    Size2i szSize(mInput.w, mInput.h);
    auto hMorphFilter = CreateMorphologyFilter(1, mInput.type, mInput.c, mKernel, ptAnchor, BORDER_CONSTANT, BORDER_CONSTANT, ImGui::ImMat());
    ImGui::ImMat mOutput = hMorphFilter->Apply(mInput, szSize, ptOfs);
    for (int i = 1; i < iIterations; i++)
        mOutput = hMorphFilter->Apply(mOutput, szSize, ptOfs);
    return mOutput;
}
}