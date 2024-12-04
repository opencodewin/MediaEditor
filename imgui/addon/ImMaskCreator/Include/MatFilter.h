#pragma once
#include <cstdint>
#include <memory>
#include "immat.h"
#include "MatUtilsVecTypeDef.h"

namespace MatUtils
{
struct RowFilter
{
    int ksize;
    int anchor;
    using Holder = std::shared_ptr<RowFilter>;

    virtual void operator()(const uint8_t* src, uint8_t* dst, int width, int cn) = 0;
};

struct ColumnFilter
{
    int ksize;
    int anchor;
    using Holder = std::shared_ptr<ColumnFilter>;

    virtual void operator()(const uint8_t** src, uint8_t* dst, int dststep, int dstcount, int width) = 0;
};

struct MatFilter
{
    Size2i ksize;
    Point2i anchor;
    using Holder = std::shared_ptr<MatFilter>;

    virtual void operator()(const uint8_t** src, uint8_t* dst, int dststep, int dstcount, int width, int cn) = 0;
};

enum BorderTypes {
    BORDER_CONSTANT    = 0, //!< `iiiiii|abcdefgh|iiiiiii`  with some specified `i`
    BORDER_REPLICATE   = 1, //!< `aaaaaa|abcdefgh|hhhhhhh`
    BORDER_REFLECT     = 2, //!< `fedcba|abcdefgh|hgfedcb`
    BORDER_WRAP        = 3, //!< `cdefgh|abcdefgh|abcdefg`
    BORDER_REFLECT_101 = 4, //!< `gfedcb|abcdefgh|gfedcba`
    BORDER_TRANSPARENT = 5, //!< `uvwxyz|abcdefgh|ijklmno`

    BORDER_REFLECT101  = BORDER_REFLECT_101, //!< same as BORDER_REFLECT_101
    BORDER_DEFAULT     = BORDER_REFLECT_101, //!< same as BORDER_REFLECT_101
    BORDER_ISOLATED    = 16 //!< do not look outside of ROI
};

struct FilterEngine
{
    using Holder = std::shared_ptr<FilterEngine>;
    static Holder CreateInstance();

    virtual void Init(MatFilter::Holder hMatFilter, RowFilter::Holder hRowFilter, ColumnFilter::Holder hColumnFilter,
            ImDataType eSrcDtype, ImDataType eDstDtype, ImDataType eBufDtype, int iCh,
            BorderTypes eRowBorderType = BORDER_CONSTANT, BorderTypes eColumnBorderType = BORDER_CONSTANT, const ImGui::ImMat& mBorderValue = ImGui::ImMat()) = 0;
    virtual int Start(const Size2i& szWholeSize, const Size2i& szSize, const Point2i& ptOfs) = 0;
    virtual int Proceed(const uint8_t* p8uSrc, int iSrcStep, int iSrcCount, uint8_t* p8uDst, int iDstStep) = 0;
    virtual ImGui::ImMat Apply(const ImGui::ImMat& mSrc, const Size2i& szSize, const Point2i& ptOfs) = 0;
};

int GetElementSize(ImDataType type);
int CountNonZero(const ImGui::ImMat& m);
void Preprocess2DKernel(const ImGui::ImMat& kernel, std::vector<Point2i>& coords, std::vector<uint8_t>& coeffs);
int BorderInterpolate(int p, int iLen, BorderTypes eBorderType);
void FillMatWithValue(ImGui::ImMat& mDst, const ImGui::ImMat& mVal, const Recti& rFillArea);

enum MorphShape
{
    MORPH_RECT = 0,
    MORPH_CROSS,
    MORPH_ELLIPSE,
};
ImGui::ImMat GetStructuringElement(MorphShape eShape, const Size2i& szKsize, const Point2i& ptAnchor = Point2i(-1, -1));

ImGui::ImMat Erode(const ImGui::ImMat& mInput, const ImGui::ImMat& mKernel, const Point2i& ptAnchor, int iIterations = 1);
ImGui::ImMat Dilate(const ImGui::ImMat& mInput, const ImGui::ImMat& mKernel, const Point2i& ptAnchor, int iIterations = 1);

ImGui::ImMat Blur(const ImGui::ImMat& mInput, const Size2i& szKsize, const Point2i& ptAnchor = Point2i(-1, -1));
}
