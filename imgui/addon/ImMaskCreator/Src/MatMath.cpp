#include <cassert>
#include <sstream>
#include "CpuUtils.h"
#include "MathUtils.h"
#include "MatMath.h"

#if SIMD_ARCH_X86
#include "SimdOpt.h"
#define INTRIN_MODE AVX2
#undef USE_AVX2
#define USE_AVX2
#include "MatMath.Simd.h"
#undef USE_AVX2
#undef INTRIN_MODE

#define INTRIN_MODE AVX
#include "MatMath.Simd.h"
#undef INTRIN_MODE

#define INTRIN_MODE SSE4_1
#undef USE_SSE4_1
#define USE_SSE4_1
#include "MatMath.Simd.h"
#undef USE_SSE4_1
#undef INTRIN_MODE

#define INTRIN_MODE SSSE3
#undef USE_SSSE3
#define USE_SSSE3
#include "MatMath.Simd.h"
#undef USE_SSSE3
#undef INTRIN_MODE

#define INTRIN_MODE SSE3
#undef USE_SSE3
#define USE_SSE3
#include "MatMath.Simd.h"
#undef USE_SSE3
#undef INTRIN_MODE

#define INTRIN_MODE SSE
#include "MatMath.Simd.h"
#undef INTRIN_MODE

#endif // ~SIMD_ARCH_X86

#if !defined(__EMSCRIPTEN__)
#include "SimdOpt.h"
#define INTRIN_MODE NONE
#include "MatMath.Simd.h"
#undef INTRIN_MODE
#endif

using namespace std;
using namespace MathUtils;
using namespace SysUtils;

namespace MatUtils
{
void Max(ImGui::ImMat& dst, const ImGui::ImMat& src)
{
    assert(dst.type == src.type && dst.w == src.w && dst.h == src.h && dst.c == src.c);

    int iLineSize = src.w*src.c*src.elemsize;
    const auto type = src.type;
    MatOp2::Holder hOp2;

    #define GET_MAT_MAX_OP(type) \
        if (type == IM_DT_INT8) { \
            using MatMaxOp = MatMax<MaxOp<uint8_t>, RowVecOpS2D1<VMax<v_uint8>>>; \
            hOp2 = MatOp2::Holder(new MatMaxOp(), [] (MatOp2* p) { \
                MatMaxOp* ptr = dynamic_cast<MatMaxOp*>(p); \
                delete ptr; }); \
        } \
        else if (type == IM_DT_INT16) { \
            using MatMaxOp = MatMax<MaxOp<uint16_t>, RowVecOpS2D1<VMax<v_uint16>>>; \
            hOp2 = MatOp2::Holder(new MatMaxOp(), [] (MatOp2* p) { \
                MatMaxOp* ptr = dynamic_cast<MatMaxOp*>(p); \
                delete ptr; }); \
        } \
        else if (type == IM_DT_INT32) { \
            using MatMaxOp = MatMax<MaxOp<int32_t>, RowVecOpS2D1<VMax<v_int32>>>; \
            hOp2 = MatOp2::Holder(new MatMaxOp(), [] (MatOp2* p) { \
                MatMaxOp* ptr = dynamic_cast<MatMaxOp*>(p); \
                delete ptr; }); \
        } \
        else if (type == IM_DT_FLOAT32) { \
            using MatMaxOp = MatMax<MaxOp<float>, RowVecOpS2D1<VMax<v_float32>>>; \
            hOp2 = MatOp2::Holder(new MatMaxOp(), [] (MatOp2* p) { \
                MatMaxOp* ptr = dynamic_cast<MatMaxOp*>(p); \
                delete ptr; }); \
        } \
        else if (type == IM_DT_FLOAT64) { \
            using MatMaxOp = MatMax<MaxOp<double>, RowVecOpS2D1<VMax<v_float64>>>; \
            hOp2 = MatOp2::Holder(new MatMaxOp(), [] (MatOp2* p) { \
                MatMaxOp* ptr = dynamic_cast<MatMaxOp*>(p); \
                delete ptr; }); \
        } \
        else throw std::runtime_error("UNSUPPORTED data type!");
#if SIMD_ARCH_X86
    if (CpuChecker::HasFeature(CpuFeature::AVX2))
    {
        using namespace SimdOpt::AVX2;
        GET_MAT_MAX_OP(type);
    }
    else if (CpuChecker::HasFeature(CpuFeature::AVX))
    {
        using namespace SimdOpt::AVX;
        GET_MAT_MAX_OP(type);
    }
    else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
    {
        using namespace SimdOpt::SSE4_1;
        GET_MAT_MAX_OP(type);
    }
    else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
    {
        using namespace SimdOpt::SSSE3;
        GET_MAT_MAX_OP(type);
    }
    else if (CpuChecker::HasFeature(CpuFeature::SSE3))
    {
        using namespace SimdOpt::SSE3;
        GET_MAT_MAX_OP(type);
    }
    else if (CpuChecker::HasFeature(CpuFeature::SSE))
    {
        using namespace SimdOpt::SSE;
        GET_MAT_MAX_OP(type);
    }
    else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
    {
        using namespace SimdOpt::NONE;
        GET_MAT_MAX_OP(type);
    }
#endif
    if (hOp2)
        (*hOp2)(src, dst, dst);
    else
        throw runtime_error("No simd MAX operator is available!");
}

void Copy(ImGui::ImMat& dst, const ImGui::ImMat& src)
{
    MatOp1::Holder hOp1;
    const auto srcType = src.type;

    #define GET_MAT_COPY_OP(type) \
        if (type == IM_DT_INT8) { \
            using MatCopyOp = MatCopy<uint8_t, RowVecCopyVal<v_uint8>>; \
            hOp1 = MatOp1::Holder(new MatCopyOp(), [] (MatOp1* p) { \
                MatCopyOp* ptr = dynamic_cast<MatCopyOp*>(p); \
                delete ptr; }); \
        } \
        else if (type == IM_DT_INT16 || type == IM_DT_INT16_BE || type == IM_DT_FLOAT16) { \
            using MatCopyOp = MatCopy<uint16_t, RowVecCopyVal<v_uint16>>; \
            hOp1 = MatOp1::Holder(new MatCopyOp(), [] (MatOp1* p) { \
                MatCopyOp* ptr = dynamic_cast<MatCopyOp*>(p); \
                delete ptr; }); \
        } \
        else if (type == IM_DT_INT32 || type == IM_DT_FLOAT32) { \
            using MatCopyOp = MatCopy<uint32_t, RowVecCopyVal<v_uint32>>; \
            hOp1 = MatOp1::Holder(new MatCopyOp(), [] (MatOp1* p) { \
                MatCopyOp* ptr = dynamic_cast<MatCopyOp*>(p); \
                delete ptr; }); \
        } \
        else if (type == IM_DT_INT64 || type == IM_DT_FLOAT64) { \
            using MatCopyOp = MatCopy<uint64_t, RowVecCopyVal<v_uint64>>; \
            hOp1 = MatOp1::Holder(new MatCopyOp(), [] (MatOp1* p) { \
                MatCopyOp* ptr = dynamic_cast<MatCopyOp*>(p); \
                delete ptr; }); \
        } \
        else throw std::runtime_error("UNSUPPORTED data type!");
#if SIMD_ARCH_X86
    if (CpuChecker::HasFeature(CpuFeature::AVX2))
    {
        using namespace SimdOpt::AVX2;
        GET_MAT_COPY_OP(srcType);
    }
    else if (CpuChecker::HasFeature(CpuFeature::AVX))
    {
        using namespace SimdOpt::AVX;
        GET_MAT_COPY_OP(srcType);
    }
    else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
    {
        using namespace SimdOpt::SSE4_1;
        GET_MAT_COPY_OP(srcType);
    }
    else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
    {
        using namespace SimdOpt::SSSE3;
        GET_MAT_COPY_OP(srcType);
    }
    else if (CpuChecker::HasFeature(CpuFeature::SSE3))
    {
        using namespace SimdOpt::SSE3;
        GET_MAT_COPY_OP(srcType);
    }
    else if (CpuChecker::HasFeature(CpuFeature::SSE))
    {
        using namespace SimdOpt::SSE;
        GET_MAT_COPY_OP(srcType);
    }
    else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
    {
        using namespace SimdOpt::NONE;
        GET_MAT_COPY_OP(srcType);
    }
#endif
    if (hOp1)
        (*hOp1)(src, dst);
}

void Convert(ImGui::ImMat& dst, const ImGui::ImMat& src, DataTypeConvType convType)
{
    MatOp1::Holder hOp1;
    const auto srcType = src.type;
    const auto dstType = dst.type;
    const auto isSrcInteger = srcType==IM_DT_INT8 || srcType==IM_DT_INT16 || srcType==IM_DT_INT16_BE || srcType==IM_DT_INT32 || srcType==IM_DT_INT64;
    const auto isDstInteger = dstType==IM_DT_INT8 || dstType==IM_DT_INT16 || dstType==IM_DT_INT16_BE || dstType==IM_DT_INT32 || dstType==IM_DT_INT64;
    const auto srcElemSize = src.elemsize;
    const auto dstElemSize = dst.elemsize;

    if (srcType == dstType)
        Copy(dst, src);
    else if (srcElemSize == dstElemSize)
    {
        if (isSrcInteger)
        {
            #define GET_MAT_CONV_TO_FLOAT_OP(type1, type2) \
                if (type1 == IM_DT_INT32) { \
                    using MatConvertOp = MatConvert<SatCastOp<int32_t, float>, RowVecConvert<VCvtF32<v_int32>>>; \
                    hOp1 = MatOp1::Holder(new MatConvertOp(), [] (MatOp1* p) { \
                        MatConvertOp* ptr = dynamic_cast<MatConvertOp*>(p); \
                        delete ptr; }); \
                } \
                else if (type1 == IM_DT_INT64) { \
                    using MatConvertOp = MatConvert<SatCastOp<int64_t, double>, RowVecConvert<VCvtF64<v_int64>>>; \
                    hOp1 = MatOp1::Holder(new MatConvertOp(), [] (MatOp1* p) { \
                        MatConvertOp* ptr = dynamic_cast<MatConvertOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("UNSUPPORTED data type!");
#if SIMD_ARCH_X86
            if (CpuChecker::HasFeature(CpuFeature::AVX2))
            {
                using namespace SimdOpt::AVX2;
                GET_MAT_CONV_TO_FLOAT_OP(srcType, dstType);
            }
            else if (CpuChecker::HasFeature(CpuFeature::AVX))
            {
                using namespace SimdOpt::AVX;
                GET_MAT_CONV_TO_FLOAT_OP(srcType, dstType);
            }
            else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
            {
                using namespace SimdOpt::SSE4_1;
                GET_MAT_CONV_TO_FLOAT_OP(srcType, dstType);
            }
            else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
            {
                using namespace SimdOpt::SSSE3;
                GET_MAT_CONV_TO_FLOAT_OP(srcType, dstType);
            }
            else if (CpuChecker::HasFeature(CpuFeature::SSE3))
            {
                using namespace SimdOpt::SSE3;
                GET_MAT_CONV_TO_FLOAT_OP(srcType, dstType);
            }
            else if (CpuChecker::HasFeature(CpuFeature::SSE))
            {
                using namespace SimdOpt::SSE;
                GET_MAT_CONV_TO_FLOAT_OP(srcType, dstType);
            }
            else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
            {
                using namespace SimdOpt::NONE;
                GET_MAT_CONV_TO_FLOAT_OP(srcType, dstType);
            }
#endif
        }
        else if (convType == DataTypeConvType_Round)
        {
            #define GET_MAT_ROUND_OP(type1, type2) \
                if (type1 == IM_DT_FLOAT32) { \
                    using MatRoundOp = MatConvert<SatCastOp<float, int32_t>, RowVecConvert<VRound<v_float32, v_int32>>>; \
                    hOp1 = MatOp1::Holder(new MatRoundOp(), [] (MatOp1* p) { \
                        MatRoundOp* ptr = dynamic_cast<MatRoundOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("UNSUPPORTED data type!");
#if SIMD_ARCH_X86
            if (CpuChecker::HasFeature(CpuFeature::AVX2))
            {
                using namespace SimdOpt::AVX2;
                GET_MAT_ROUND_OP(srcType, dstType);
            }
            else if (CpuChecker::HasFeature(CpuFeature::AVX))
            {
                using namespace SimdOpt::AVX;
                GET_MAT_ROUND_OP(srcType, dstType);
            }
            else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
            {
                using namespace SimdOpt::SSE4_1;
                GET_MAT_ROUND_OP(srcType, dstType);
            }
            else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
            {
                using namespace SimdOpt::SSSE3;
                GET_MAT_ROUND_OP(srcType, dstType);
            }
            else if (CpuChecker::HasFeature(CpuFeature::SSE3))
            {
                using namespace SimdOpt::SSE3;
                GET_MAT_ROUND_OP(srcType, dstType);
            }
            else if (CpuChecker::HasFeature(CpuFeature::SSE))
            {
                using namespace SimdOpt::SSE;
                GET_MAT_ROUND_OP(srcType, dstType);
            }
            else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
            {
                using namespace SimdOpt::NONE;
                GET_MAT_ROUND_OP(srcType, dstType);
            }
#endif
        }
    }
    else if (srcElemSize < dstElemSize) // int->int or float->float, size expand
    {
        #define GET_MAT_EXPAND_OP(type1, type2) \
            if (type1 == IM_DT_INT8) { \
                if (type2 == IM_DT_INT16) { \
                    using MatExpandOp = MatConvert<ExpandOp<uint8_t, uint16_t>, RowVecExpand<VExpandSizeX2<v_uint8, v_uint16>>>; \
                    hOp1 = MatOp1::Holder(new MatExpandOp(), [] (MatOp1* p) { \
                        MatExpandOp* ptr = dynamic_cast<MatExpandOp*>(p); \
                        delete ptr; }); \
                } \
                else if (type2 == IM_DT_INT32) { \
                    using MatExpandOp = MatConvert<ExpandOp<uint8_t, uint32_t>, RowVecExpand<VExpandSizeX4<v_uint8, v_uint16, v_uint32>>>; \
                    hOp1 = MatOp1::Holder(new MatExpandOp(), [] (MatOp1* p) { \
                        MatExpandOp* ptr = dynamic_cast<MatExpandOp*>(p); \
                        delete ptr; }); \
                } \
                else if (type2 == IM_DT_INT64) { \
                    using MatExpandOp = MatConvert<ExpandOp<uint8_t, uint64_t>, RowVecExpand<VExpandSizeX8<v_uint8, v_uint16, v_uint32, v_uint64>>>; \
                    hOp1 = MatOp1::Holder(new MatExpandOp(), [] (MatOp1* p) { \
                        MatExpandOp* ptr = dynamic_cast<MatExpandOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("INVALID code branch!"); \
            } \
            else if (type1 == IM_DT_INT16) { \
                if (type2 == IM_DT_INT32) { \
                    using MatExpandOp = MatConvert<ExpandOp<uint16_t, uint32_t>, RowVecExpand<VExpandSizeX2<v_uint16, v_uint32>>>; \
                    hOp1 = MatOp1::Holder(new MatExpandOp(), [] (MatOp1* p) { \
                        MatExpandOp* ptr = dynamic_cast<MatExpandOp*>(p); \
                        delete ptr; }); \
                } \
                else if (type2 == IM_DT_INT64) { \
                    using MatExpandOp = MatConvert<ExpandOp<uint16_t, uint64_t>, RowVecExpand<VExpandSizeX4<v_uint16, v_uint32, v_uint64>>>; \
                    hOp1 = MatOp1::Holder(new MatExpandOp(), [] (MatOp1* p) { \
                        MatExpandOp* ptr = dynamic_cast<MatExpandOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("INVALID code branch!"); \
            } \
            else if (type1 == IM_DT_INT32) { \
                if (type2 == IM_DT_INT64) { \
                    using MatExpandOp = MatConvert<ExpandOp<int32_t, int64_t>, RowVecExpand<VExpandSizeX2<v_int32, v_int64>>>; \
                    hOp1 = MatOp1::Holder(new MatExpandOp(), [] (MatOp1* p) { \
                        MatExpandOp* ptr = dynamic_cast<MatExpandOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("INVALID code branch!"); \
            } \
            else if (type1 == IM_DT_FLOAT32) { \
                if (type2 == IM_DT_FLOAT64) { \
                    using MatExpandOp = MatConvert<ExpandOp<float, double>, RowVecExpand<VExpandSizeX2<v_float32, v_float64>>>; \
                    hOp1 = MatOp1::Holder(new MatExpandOp(), [] (MatOp1* p) { \
                        MatExpandOp* ptr = dynamic_cast<MatExpandOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("INVALID code branch!"); \
            } \
            else throw std::runtime_error("UNSUPPORTED data type!");
#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_MAT_EXPAND_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_MAT_EXPAND_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_MAT_EXPAND_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_MAT_EXPAND_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_MAT_EXPAND_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_MAT_EXPAND_OP(srcType, dstType);
        }
        else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_MAT_EXPAND_OP(srcType, dstType);
        }
#endif
    }
    else // int->int or float->float, size pack
    {
        #define GET_MAT_PACK_OP(type1, type2) \
            if (type1 == IM_DT_INT64) { \
                if (type2 == IM_DT_INT32) { \
                    using MatPackOp = MatConvert<SatCastOp<int64_t, int32_t>, RowVecPack<VPackSizeX2<v_int64, v_int32>>>; \
                    hOp1 = MatOp1::Holder(new MatPackOp(), [] (MatOp1* p) { \
                        MatPackOp* ptr = dynamic_cast<MatPackOp*>(p); \
                        delete ptr; }); \
                } \
                else if (type2 == IM_DT_INT16) { \
                    using MatPackOp = MatConvert<SatCastOp<int64_t, int16_t>, RowVecPack<VPackSizeX4<v_int64, v_int32, v_int16>>>; \
                    hOp1 = MatOp1::Holder(new MatPackOp(), [] (MatOp1* p) { \
                        MatPackOp* ptr = dynamic_cast<MatPackOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("INVALID code branch!"); \
            } \
            else if (type1 == IM_DT_INT32) { \
                if (type2 == IM_DT_INT16) { \
                    using MatPackOp = MatConvert<SatCastOp<int32_t, int16_t>, RowVecPack<VPackSizeX2<v_int32, v_int16>>>; \
                    hOp1 = MatOp1::Holder(new MatPackOp(), [] (MatOp1* p) { \
                        MatPackOp* ptr = dynamic_cast<MatPackOp*>(p); \
                        delete ptr; }); \
                } \
                else if (type2 == IM_DT_INT8) { \
                    using MatPackOp = MatConvert<SatCastOp<int32_t, int8_t>, RowVecPack<VPackSizeX4<v_int32, v_int16, v_int8>>>; \
                    hOp1 = MatOp1::Holder(new MatPackOp(), [] (MatOp1* p) { \
                        MatPackOp* ptr = dynamic_cast<MatPackOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("INVALID code branch!"); \
            } \
            else if (type1 == IM_DT_INT16) { \
                if (type2 == IM_DT_INT8) { \
                    using MatPackOp = MatConvert<SatCastOp<uint16_t, uint8_t>, RowVecPack<VPackSizeX2<v_uint16, v_uint8>>>; \
                    hOp1 = MatOp1::Holder(new MatPackOp(), [] (MatOp1* p) { \
                        MatPackOp* ptr = dynamic_cast<MatPackOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("INVALID code branch!"); \
            } \
            else if (type1 == IM_DT_FLOAT64) { \
                if (type2 == IM_DT_FLOAT32) { \
                    using MatPackOp = MatConvert<SatCastOp<double, float>, RowVecPack<VPackSizeX2<v_float64, v_float32>>>; \
                    hOp1 = MatOp1::Holder(new MatPackOp(), [] (MatOp1* p) { \
                        MatPackOp* ptr = dynamic_cast<MatPackOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("INVALID code branch!"); \
            } \
            else throw std::runtime_error("UNSUPPORTED data type!");
#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_MAT_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_MAT_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_MAT_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_MAT_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_MAT_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_MAT_PACK_OP(srcType, dstType);
        }
        else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_MAT_PACK_OP(srcType, dstType);
        }
#endif
    }

    if (hOp1)
        (*hOp1)(src, dst);
}

void ConvertColorDepth(ImGui::ImMat& dst, const ImGui::ImMat& src)
{
    MatOp1::Holder hOp1;
    const auto srcType = src.type;
    const auto dstType = dst.type;
    const auto srcElemSize = src.elemsize;
    const auto dstElemSize = dst.elemsize;

    if (srcType == dstType)
        Copy(dst, src);
    else if (srcElemSize < dstElemSize) // u8->u16, u8->f32, u16->f32
    {
        #define GET_MAT_COLDEP_EXPAND_OP(type1, type2) \
            if (type1 == IM_DT_INT8) { \
                if (type2 == IM_DT_INT16) { \
                    using MatCvtColDepOp = MatConvert<ColDepExpandU16<uint8_t>, RowVecExpand<VColDepExpandU16<v_uint8>>>; \
                    hOp1 = MatOp1::Holder(new MatCvtColDepOp(), [] (MatOp1* p) { \
                        MatCvtColDepOp* ptr = dynamic_cast<MatCvtColDepOp*>(p); \
                        delete ptr; }); \
                } \
                if (type2 == IM_DT_FLOAT32) { \
                    using MatCvtColDepOp = MatConvert<ColDepExpandF32<uint8_t>, RowVecExpand<VColDepExpandF32<v_uint8>>>; \
                    hOp1 = MatOp1::Holder(new MatCvtColDepOp(), [] (MatOp1* p) { \
                        MatCvtColDepOp* ptr = dynamic_cast<MatCvtColDepOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("INVALID code branch!"); \
            } \
            else if (type1 == IM_DT_INT16) { \
                if (type2 == IM_DT_FLOAT32) { \
                    using MatCvtColDepOp = MatConvert<ColDepExpandF32<uint16_t>, RowVecExpand<VColDepExpandF32<v_uint16>>>; \
                    hOp1 = MatOp1::Holder(new MatCvtColDepOp(), [] (MatOp1* p) { \
                        MatCvtColDepOp* ptr = dynamic_cast<MatCvtColDepOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("INVALID code branch!"); \
            } \
            else throw std::runtime_error("UNSUPPORTED data type!");
#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_MAT_COLDEP_EXPAND_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_MAT_COLDEP_EXPAND_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_MAT_COLDEP_EXPAND_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_MAT_COLDEP_EXPAND_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_MAT_COLDEP_EXPAND_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_MAT_COLDEP_EXPAND_OP(srcType, dstType);
        }
        else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_MAT_COLDEP_EXPAND_OP(srcType, dstType);
        }
#endif
    }
    else if (dstElemSize < srcElemSize) // f32->u16, f32->u8, u16->u8
    {
        #define GET_MAT_COLDEP_PACK_OP(type1, type2) \
            if (type2 == IM_DT_INT8) { \
                if (type1 == IM_DT_INT16) { \
                    using MatPackOp = MatConvert<ColDepPackU8<uint16_t>, RowVecPack<VColDepPack<v_uint16, v_uint8>>>; \
                    hOp1 = MatOp1::Holder(new MatPackOp(), [] (MatOp1* p) { \
                        MatPackOp* ptr = dynamic_cast<MatPackOp*>(p); \
                        delete ptr; }); \
                } \
                else if (type1 == IM_DT_FLOAT32) { \
                    using MatPackOp = MatConvert<ColDepPackU8<float>, RowVecPack<VColDepPack<v_float32, v_uint8>>>; \
                    hOp1 = MatOp1::Holder(new MatPackOp(), [] (MatOp1* p) { \
                        MatPackOp* ptr = dynamic_cast<MatPackOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("INVALID code branch!"); \
            } \
            else if (type2 == IM_DT_INT16) { \
                if (type1 == IM_DT_FLOAT32) { \
                    using MatPackOp = MatConvert<ColDepPackU16<float>, RowVecPack<VColDepPack<v_float32, v_uint16>>>; \
                    hOp1 = MatOp1::Holder(new MatPackOp(), [] (MatOp1* p) { \
                        MatPackOp* ptr = dynamic_cast<MatPackOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("INVALID code branch!"); \
            } \
            else throw std::runtime_error("UNSUPPORTED data type!");
#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_MAT_COLDEP_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_MAT_COLDEP_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_MAT_COLDEP_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_MAT_COLDEP_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_MAT_COLDEP_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_MAT_COLDEP_PACK_OP(srcType, dstType);
        }
        else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_MAT_COLDEP_PACK_OP(srcType, dstType);
        }
#endif
    }
    else
    {
        throw std::runtime_error("UNSUPPORTED data type!");
    }

    if (hOp1)
        (*hOp1)(src, dst);
}

void GrayToRgba(ImGui::ImMat& dst, const ImGui::ImMat& src, double alphaVal)
{
    assert(!src.empty() && src.c == 1);

    MatOp1::Holder hOp1;
    const auto srcType = src.type;
    const auto dstType = dst.type;
    const auto srcCh = src.c;
    const auto dstCh = dst.c;
    const auto isSrcInteger = srcType==IM_DT_INT8 || srcType==IM_DT_INT16 || srcType==IM_DT_INT16_BE || srcType==IM_DT_INT32 || srcType==IM_DT_INT64;
    const auto isDstInteger = dstType==IM_DT_INT8 || dstType==IM_DT_INT16 || dstType==IM_DT_INT16_BE || dstType==IM_DT_INT32 || dstType==IM_DT_INT64;
    const auto srcElemSize = src.elemsize;
    const auto dstElemSize = dst.elemsize;

    if (srcType == dstType)
    {
        #define GET_MAT_GRAY_TO_RGBA_COPY_OP(type) \
            if (type == IM_DT_INT8) { \
                const auto vecAlpha = vx_setall_u8((uint8_t)alphaVal); \
                using MatGray2RgbaOp = MatGrayToRgba<CopyOp<uint8_t>, RowVecGrayToRgbaCopy<v_uint8>>; \
                hOp1 = MatOp1::Holder(new MatGray2RgbaOp(vecAlpha), [] (MatOp1* p) { \
                    MatGray2RgbaOp* ptr = dynamic_cast<MatGray2RgbaOp*>(p); \
                    delete ptr; }); \
            } \
            else if (type == IM_DT_INT16) { \
                const auto vecAlpha = vx_setall_u16((uint16_t)alphaVal); \
                using MatGray2RgbaOp = MatGrayToRgba<CopyOp<uint16_t>, RowVecGrayToRgbaCopy<v_uint16>>; \
                hOp1 = MatOp1::Holder(new MatGray2RgbaOp(vecAlpha), [] (MatOp1* p) { \
                    MatGray2RgbaOp* ptr = dynamic_cast<MatGray2RgbaOp*>(p); \
                    delete ptr; }); \
            } \
            else if (type == IM_DT_INT32) { \
                const auto vecAlpha = vx_setall_s32((int32_t)alphaVal); \
                using MatGray2RgbaOp = MatGrayToRgba<CopyOp<int32_t>, RowVecGrayToRgbaCopy<v_int32>>; \
                hOp1 = MatOp1::Holder(new MatGray2RgbaOp(vecAlpha), [] (MatOp1* p) { \
                    MatGray2RgbaOp* ptr = dynamic_cast<MatGray2RgbaOp*>(p); \
                    delete ptr; }); \
            } \
            else if (type == IM_DT_FLOAT32) { \
                const auto vecAlpha = vx_setall_f32((float)alphaVal); \
                using MatGray2RgbaOp = MatGrayToRgba<CopyOp<float>, RowVecGrayToRgbaCopy<v_float32>>; \
                hOp1 = MatOp1::Holder(new MatGray2RgbaOp(vecAlpha), [] (MatOp1* p) { \
                    MatGray2RgbaOp* ptr = dynamic_cast<MatGray2RgbaOp*>(p); \
                    delete ptr; }); \
            } \
            else throw std::runtime_error("UNSUPPORTED data type!");
#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_MAT_GRAY_TO_RGBA_COPY_OP(srcType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_MAT_GRAY_TO_RGBA_COPY_OP(srcType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_MAT_GRAY_TO_RGBA_COPY_OP(srcType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_MAT_GRAY_TO_RGBA_COPY_OP(srcType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_MAT_GRAY_TO_RGBA_COPY_OP(srcType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_MAT_GRAY_TO_RGBA_COPY_OP(srcType);
        }
        else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_MAT_GRAY_TO_RGBA_COPY_OP(srcType);
        }
#endif
    }
    else if (dstElemSize < srcElemSize)
    {
        #define GET_MAT_GRAY_TO_RGBA_PACK_OP(type1, type2) \
            if (type2 == IM_DT_INT8) { \
                const auto vecAlpha = vx_setall_u8((uint8_t)alphaVal); \
                if (type1 == IM_DT_INT16) { \
                    using MatGray2RgbaOp = MatGrayToRgba<ColDepPackU8<uint16_t>, RowVecGrayToRgbaPack<v_uint16, v_uint8>>; \
                    hOp1 = MatOp1::Holder(new MatGray2RgbaOp(vecAlpha), [] (MatOp1* p) { \
                        MatGray2RgbaOp* ptr = dynamic_cast<MatGray2RgbaOp*>(p); \
                        delete ptr; }); \
                } \
                else if (type1 == IM_DT_FLOAT32) { \
                    using MatGray2RgbaOp = MatGrayToRgba<ColDepPackU8<float>, RowVecGrayToRgbaPack<v_float32, v_uint8>>; \
                    hOp1 = MatOp1::Holder(new MatGray2RgbaOp(vecAlpha), [] (MatOp1* p) { \
                        MatGray2RgbaOp* ptr = dynamic_cast<MatGray2RgbaOp*>(p); \
                        delete ptr; }); \
                } \
                else throw std::runtime_error("INVALID code branch!"); \
            } \
            else throw std::runtime_error("UNSUPPORTED data type!");
#if SIMD_ARCH_X86
        if (CpuChecker::HasFeature(CpuFeature::AVX2))
        {
            using namespace SimdOpt::AVX2;
            GET_MAT_GRAY_TO_RGBA_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::AVX))
        {
            using namespace SimdOpt::AVX;
            GET_MAT_GRAY_TO_RGBA_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE4_1))
        {
            using namespace SimdOpt::SSE4_1;
            GET_MAT_GRAY_TO_RGBA_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSSE3))
        {
            using namespace SimdOpt::SSSE3;
            GET_MAT_GRAY_TO_RGBA_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE3))
        {
            using namespace SimdOpt::SSE3;
            GET_MAT_GRAY_TO_RGBA_PACK_OP(srcType, dstType);
        }
        else if (CpuChecker::HasFeature(CpuFeature::SSE))
        {
            using namespace SimdOpt::SSE;
            GET_MAT_GRAY_TO_RGBA_PACK_OP(srcType, dstType);
        }
        else
#endif // ~SIMD_ARCH_X86
#if !defined(__EMSCRIPTEN__)
        {
            using namespace SimdOpt::NONE;
            GET_MAT_GRAY_TO_RGBA_PACK_OP(srcType, dstType);
        }
#endif
    }
    else
    {
        throw std::runtime_error("UNSUPPORTED data type!");
    }

    if (hOp1)
        (*hOp1)(src, dst);
}
}