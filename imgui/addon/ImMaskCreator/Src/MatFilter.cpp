#include <list>
#include <functional>
#include <future>
#include <algorithm>
#include <cassert>
#include <iostream>
#include "MatFilter.h"

using namespace std;

namespace MatUtils
{
static const int MAX_PARALLEL_COUNT = 32;

static int _CountNonZeroSlice_int8(const ImGui::ImMat& m, int y0, int y1)
{
    int n = 0;
    const int w = m.w;
    const int8_t* p = (const int8_t*)m.data+y0*w*m.elemsize;
    int lineSize = w*m.elemsize;
    for (int i = y0; i < y1; i++)
    {
        const int8_t* pLine = p;
        for (int j = 0; j < w; j++)
            if (pLine[j] != 0) n++;
        p += lineSize;
    }
    return n;
}

static int _CountNonZeroSlice_int16(const ImGui::ImMat& m, int y0, int y1)
{
    int n = 0;
    const int w = m.w;
    const int8_t* p = (const int8_t*)m.data+y0*w*m.elemsize;
    int lineSize = w*m.elemsize;
    for (int i = y0; i < y1; i++)
    {
        const int16_t* pLine = (const int16_t*)p;
        for (int j = 0; j < w; j++)
            if (pLine[j] != 0) n++;
        p += lineSize;
    }
    return n;
}

static int _CountNonZeroSlice_int32(const ImGui::ImMat& m, int y0, int y1)
{
    int n = 0;
    const int w = m.w;
    const int8_t* p = (const int8_t*)m.data+y0*w*m.elemsize;
    int lineSize = w*m.elemsize;
    for (int i = y0; i < y1; i++)
    {
        const int32_t* pLine = (const int32_t*)p;
        for (int j = 0; j < w; j++)
            if (pLine[j] != 0) n++;
        p += lineSize;
    }
    return n;
}

static int _CountNonZeroSlice_int64(const ImGui::ImMat& m, int y0, int y1)
{
    int n = 0;
    const int w = m.w;
    const int8_t* p = (const int8_t*)m.data+y0*w*m.elemsize;
    int lineSize = w*m.elemsize;
    for (int i = y0; i < y1; i++)
    {
        const int64_t* pLine = (const int64_t*)p;
        for (int j = 0; j < w; j++)
            if (pLine[j] != 0) n++;
        p += lineSize;
    }
    return n;
}

static int _CountNonZeroSlice_float(const ImGui::ImMat& m, int y0, int y1)
{
    int n = 0;
    const int w = m.w;
    const int8_t* p = (const int8_t*)m.data+y0*w*m.elemsize;
    int lineSize = w*m.elemsize;
    for (int i = y0; i < y1; i++)
    {
        const float* pLine = (const float*)p;
        for (int j = 0; j < w; j++)
            if (pLine[j] != 0) n++;
        p += lineSize;
    }
    return n;
}

static int _CountNonZeroSlice_double(const ImGui::ImMat& m, int y0, int y1)
{
    int n = 0;
    const int w = m.w;
    const int8_t* p = (const int8_t*)m.data+y0*w*m.elemsize;
    int lineSize = w*m.elemsize;
    for (int i = y0; i < y1; i++)
    {
        const double* pLine = (const double*)p;
        for (int j = 0; j < w; j++)
            if (pLine[j] != 0) n++;
        p += lineSize;
    }
    return n;
}

int CountNonZero(const ImGui::ImMat& m)
{
    if (m.empty())
        return 0;
    assert(m.c == 1);

    function<int(const ImGui::ImMat&,int,int)> taskFunc;
    switch (m.type)
    {
    case IM_DT_INT8:
        taskFunc = _CountNonZeroSlice_int8;
        break;
    case IM_DT_INT16:
    case IM_DT_INT16_BE:
        taskFunc = _CountNonZeroSlice_int16;
        break;
    case IM_DT_INT32:
        taskFunc = _CountNonZeroSlice_int32;
        break;
    case IM_DT_INT64:
        taskFunc = _CountNonZeroSlice_int64;
        break;
    case IM_DT_FLOAT32:
        taskFunc = _CountNonZeroSlice_float;
        break;
    case IM_DT_FLOAT64:
        taskFunc = _CountNonZeroSlice_double;
        break;
    default:
        throw runtime_error("UNSUPPORTED data type!");
    }

    int sliceStep = (int)ceil((float)m.h/MAX_PARALLEL_COUNT);
    if (sliceStep <= 0) sliceStep = 1;
    int sliceY0 = 0;
    list<future<int>> countTasks;
    while (sliceY0 < m.h)
    {
        int sliceY1 = sliceY0+sliceStep;
        if (sliceY1 > m.h) sliceY1 = m.h;
        countTasks.push_back(async(taskFunc, m, sliceY0, sliceY1));
        sliceY0 = sliceY1;
    }
    int n = 0;
    for (auto& t : countTasks)
        n += t.get();
    return n;
}

void Preprocess2DKernel(const ImGui::ImMat& kernel, std::vector<Point2i>& coords, std::vector<uint8_t>& coeffs)
{
    int i, j, k, nz = CountNonZero(kernel);
    if(nz == 0)
        nz = 1;
    auto dataType = kernel.type;
    assert(dataType == IM_DT_INT8 || dataType == IM_DT_INT32 || dataType == IM_DT_FLOAT32 || dataType == IM_DT_FLOAT64);
    coords.resize(nz);
    coeffs.resize(nz*kernel.elemsize);
    uint8_t* _coeffs = &coeffs[0];

    for (i = k = 0; i < kernel.h; i++ )
    {
        const uint8_t* krow = (uint8_t*)kernel.data+i*kernel.w*kernel.elemsize;
        for (j = 0; j < kernel.w; j++ )
        {
            if (dataType == IM_DT_INT8)
            {
                uint8_t val = krow[j];
                if (val == 0)
                    continue;
                coords[k] = Point2i(j, i);
                _coeffs[k++] = val;
            }
            else if (dataType == IM_DT_INT32)
            {
                int32_t val = ((const int32_t*)krow)[j];
                if (val == 0)
                    continue;
                coords[k] = Point2i(j, i);
                ((int32_t*)_coeffs)[k++] = val;
            }
            else if (dataType == IM_DT_FLOAT32)
            {
                float val = ((const float*)krow)[j];
                if (val == 0)
                    continue;
                coords[k] = Point2i(j, i);
                ((float*)_coeffs)[k++] = val;
            }
            else
            {
                double val = ((const double*)krow)[j];
                if (val == 0)
                    continue;
                coords[k] = Point2i(j, i);
                ((double*)_coeffs)[k++] = val;
            }
        }
    }
}

static void _FillSlice_int8(ImGui::ImMat& m, const Recti& r, const void* pColor)
{
    const int x0 = r.leftTop.x, x1 = x0+r.size.x;
    const int y0 = r.leftTop.y, y1 = y0+r.size.y;
    const int c = m.c;
    const int step = m.elemsize*m.c;
    const int linesize = m.w*step;
    uint8_t* pDstLine = (uint8_t*)m.data+y0*linesize+x0*step;
    int copysize = (x1-x0)*step;
    for (int i = y0; i < y1; i++)
    {
        uint8_t* pDst = pDstLine;
        for (int j = x0; j < x1; j++)
        {
            const uint8_t* pSrc = (const uint8_t*)pColor;
            for (int k = 0; k < c; k++)
                *pDst++ = *pSrc++;
        }
        pDstLine += linesize;
    }
}

static void _FillSlice_int16(ImGui::ImMat& m, const Recti& r, const void* pColor)
{
    const int x0 = r.leftTop.x, x1 = x0+r.size.x;
    const int y0 = r.leftTop.y, y1 = y0+r.size.y;
    const int c = m.c;
    const int step = m.elemsize*m.c;
    const int linesize = m.w*step;
    uint8_t* pDstLine = (uint8_t*)m.data+y0*linesize+x0*step;
    int copysize = (x1-x0)*step;
    for (int i = y0; i < y1; i++)
    {
        uint16_t* pDst = (uint16_t*)pDstLine;
        for (int j = x0; j < x1; j++)
        {
            const uint16_t* pSrc = (const uint16_t*)pColor;
            for (int k = 0; k < c; k++)
                *pDst++ = *pSrc++;
        }
        pDstLine += linesize;
    }
}

static void _FillSlice_int32(ImGui::ImMat& m, const Recti& r, const void* pColor)
{
    const int x0 = r.leftTop.x, x1 = x0+r.size.x;
    const int y0 = r.leftTop.y, y1 = y0+r.size.y;
    const int c = m.c;
    const int step = m.elemsize*m.c;
    const int linesize = m.w*step;
    uint8_t* pDstLine = (uint8_t*)m.data+y0*linesize+x0*step;
    int copysize = (x1-x0)*step;
    for (int i = y0; i < y1; i++)
    {
        uint32_t* pDst = (uint32_t*)pDstLine;
        for (int j = x0; j < x1; j++)
        {
            const uint32_t* pSrc = (const uint32_t*)pColor;
            for (int k = 0; k < c; k++)
                *pDst++ = *pSrc++;
        }
        pDstLine += linesize;
    }
}

static void _FillSlice_int64(ImGui::ImMat& m, const Recti& r, const void* pColor)
{
    const int x0 = r.leftTop.x, x1 = x0+r.size.x;
    const int y0 = r.leftTop.y, y1 = y0+r.size.y;
    const int c = m.c;
    const int step = m.elemsize*m.c;
    const int linesize = m.w*step;
    uint8_t* pDstLine = (uint8_t*)m.data+y0*linesize+x0*step;
    int copysize = (x1-x0)*step;
    for (int i = y0; i < y1; i++)
    {
        uint64_t* pDst = (uint64_t*)pDstLine;
        for (int j = x0; j < x1; j++)
        {
            const uint64_t* pSrc = (const uint64_t*)pColor;
            for (int k = 0; k < c; k++)
                *pDst++ = *pSrc++;
        }
        pDstLine += linesize;
    }
}

void FillMatWithValue(ImGui::ImMat& mDst, const ImGui::ImMat& mVal, const Recti& rFillArea)
{
    assert(!mDst.empty() && !mVal.empty());
    assert(mVal.type == mDst.type && mVal.c >= mDst.c && (mDst.elempack > 1 || mDst.c == 1));
    if (rFillArea.leftTop.x >= mDst.w || rFillArea.leftTop.y >= mDst.h)
        return;
    if (rFillArea.leftTop.x < 0)
    {
        rFillArea.size.x += rFillArea.leftTop.x;
        rFillArea.leftTop.x = 0;
    }
    if (rFillArea.leftTop.y < 0)
    {
        rFillArea.size.y += rFillArea.leftTop.y;
        rFillArea.leftTop.y = 0;
    }
    if (rFillArea.size.x <= 0 || rFillArea.size.y <= 0)
        return;
    switch (mDst.type)
    {
    case IM_DT_INT8:
        _FillSlice_int8(mDst, rFillArea, mVal.data);
        break;
    case IM_DT_INT16:
    case IM_DT_INT16_BE:
    case IM_DT_FLOAT16:
        _FillSlice_int16(mDst, rFillArea, mVal.data);
        break;
    case IM_DT_INT32:
    case IM_DT_FLOAT32:
        _FillSlice_int32(mDst, rFillArea, mVal.data);
        break;
    case IM_DT_INT64:
    case IM_DT_FLOAT64:
        _FillSlice_int64(mDst, rFillArea, mVal.data);
        break;
    default:
        throw runtime_error("UNSUPPORTED data type!");
    }
}

int GetElementSize(ImDataType type)
{
    int elemSize = 0;
    switch (type)
    {
    case IM_DT_INT8:
        elemSize = 1;
        break;
    case IM_DT_INT16:
    case IM_DT_INT16_BE:
    case IM_DT_FLOAT16:
        elemSize = 2;
        break;
    case IM_DT_INT32:
    case IM_DT_FLOAT32:
        elemSize = 4;
        break;
    case IM_DT_INT64:
    case IM_DT_FLOAT64:
        elemSize = 8;
        break;
    default:
        throw runtime_error("UNSUPPORTED data type!");
    }
    return elemSize;
}

template<typename _Tp> static inline _Tp* AlignPtr(_Tp* ptr, int n=(int)sizeof(_Tp))
{
    assert((n & (n - 1)) == 0); // n is a power of 2
    return (_Tp*)(((size_t)ptr + n-1) & -n);
}

static inline size_t AlignSize(size_t sz, int n)
{
    assert((n & (n - 1)) == 0); // n is a power of 2
    return (sz + n-1) & -n;
}

int BorderInterpolate(int p, int iLen, BorderTypes eBorderType)
{
    assert(iLen > 0);

    if ((uint32_t)p < (uint32_t)iLen)
        ;
    else if (eBorderType == BORDER_REPLICATE)
        p = p < 0 ? 0 : iLen-1;
    else if (eBorderType == BORDER_REFLECT || eBorderType == BORDER_REFLECT_101)
    {
        int delta = eBorderType == BORDER_REFLECT_101;
        if (iLen == 1)
            return 0;
        do
        {
            if( p < 0 )
                p = -p-1+delta;
            else
                p = iLen-1-(p-iLen)-delta;
        }
        while ((uint32_t)p >= (uint32_t)iLen);
    }
    else if (eBorderType == BORDER_WRAP)
    {
        if (p < 0)
            p -= ((p-iLen+1)/iLen)*iLen;
        if (p >= iLen)
            p %= iLen;
    }
    else if (eBorderType == BORDER_CONSTANT)
        p = -1;
    else
        throw runtime_error("Unknown/unsupported border type");
    return p;
}


#define VEC_ALIGN 64

class FilterEngineImpl : public FilterEngine
{
public:
    FilterEngineImpl()
        : m_eSrcDtype(IM_DT_UNDEFINED), m_iCh(0)
        , m_iMaxWidth(0), m_szWholeSize(-1, -1), m_iDx1(0), m_iDx2(0)
        , m_eRowBorderType(BORDER_CONSTANT), m_eColumnBorderType(BORDER_CONSTANT)
        , m_iBorderElemSize(0), m_iBufStep(0), m_iStartY(0), m_iStartY0(0), m_iEndY(0), m_iRowCount(0), m_iDstY(0)
    {
    }

    ~FilterEngineImpl()
    {
    }

    void Init(MatFilter::Holder hMatFilter, RowFilter::Holder hRowFilter, ColumnFilter::Holder hColumnFilter,
            ImDataType eSrcDtype, ImDataType eDstDtype, ImDataType eBufDtype, int iCh,
            BorderTypes eRowBorderType, BorderTypes eColumnBorderType, const ImGui::ImMat& mBorderValue) override
    {
        m_eSrcDtype = eSrcDtype;
        m_eDstDtype = eDstDtype;
        m_eBufDtype = eBufDtype;
        m_iCh = iCh;
        int srcElemSize = GetElementSize(eSrcDtype);

        m_hMatFilter = hMatFilter;
        m_hRowFilter = hRowFilter;
        m_hColumnFilter = hColumnFilter;

        if (eColumnBorderType < 0)
            eColumnBorderType = eRowBorderType;
        m_eRowBorderType = eRowBorderType;
        m_eColumnBorderType = eColumnBorderType;

        if (IsSeparable())
        {
            m_szKsize = Size2i(m_hRowFilter->ksize, m_hColumnFilter->ksize);
            m_ptAnchor = Point2i(m_hRowFilter->anchor, m_hColumnFilter->anchor);
        }
        else
        {
            assert(eBufDtype == eSrcDtype);
            m_szKsize = m_hMatFilter->ksize;
            m_ptAnchor = m_hMatFilter->anchor;
        }

        assert(0 <= m_ptAnchor.x && m_ptAnchor.x < m_szKsize.x && 0 <= m_ptAnchor.y && m_ptAnchor.y < m_szKsize.y);

        m_iBorderElemSize = GetElementSize(m_eSrcDtype);
        int borderLength = std::max(m_szKsize.x-1, 1);
        m_aiBorderTab.resize(borderLength*m_iBorderElemSize);

        m_iMaxWidth = m_iBufStep = 0;
        m_au8ConstBorderRow.clear();

        if (m_eRowBorderType == BORDER_CONSTANT || m_eColumnBorderType == BORDER_CONSTANT)
        {
            m_mConstBorderValue.create_type(borderLength, 1, m_iCh, m_eSrcDtype);
            FillMatWithValue(m_mConstBorderValue, mBorderValue, Recti(0, 0, borderLength, 1));
        }

        m_szWholeSize = Size2i(-1, -1);
    }

    int Start(const Size2i& szWholeSize, const Size2i& szSize, const Point2i& ptOfs) override
    {
        int i, j;

        m_szWholeSize = szWholeSize;
        m_rRoi = Recti(ptOfs, szSize);
        assert(m_rRoi.leftTop.x >= 0 && m_rRoi.leftTop.y >= 0 && m_rRoi.size.x >= 0 && m_rRoi.size.y >= 0 &&
                m_rRoi.rightBottom().x <= szWholeSize.x && m_rRoi.rightBottom().y <= szWholeSize.y);

        int esz = GetElementSize(m_eSrcDtype);
        int bufElemSize = GetElementSize(m_eBufDtype);
        const uint8_t* constVal = !m_mConstBorderValue.empty() ? (const uint8_t*)m_mConstBorderValue.data : nullptr;

        int _maxBufRows = std::max(
                szWholeSize.y+(m_szKsize.y-1)*2,
                std::max(m_ptAnchor.y, m_szKsize.y-m_ptAnchor.y-1)*2+1);

        if (m_iMaxWidth < m_rRoi.size.x || _maxBufRows != (int)m_apu8Rows.size())
        {
            m_apu8Rows.resize(_maxBufRows);
            m_iMaxWidth = std::max(m_iMaxWidth, m_rRoi.size.x);
            m_au8SrcRow.resize(esz*(m_iMaxWidth + m_szKsize.x-1));
            if (m_eColumnBorderType == BORDER_CONSTANT)
            {
                assert(constVal != NULL);
                m_au8ConstBorderRow.resize(GetElementSize(m_eBufDtype)*(m_iMaxWidth + m_szKsize.x-1 + VEC_ALIGN));
                uint8_t *dst = AlignPtr(&m_au8ConstBorderRow[0], VEC_ALIGN);
                int n = m_mConstBorderValue.w;
                int N = (m_iMaxWidth+m_szKsize.x-1)*esz;
                uint8_t *tdst = IsSeparable() ? &m_au8SrcRow[0] : dst;

                for (i = 0; i < N; i += n)
                {
                    n = std::min(n, N - i);
                    for(j = 0; j < n; j++)
                        tdst[i+j] = constVal[j];
                }

                if (IsSeparable())
                    (*m_hRowFilter)(&m_au8SrcRow[0], dst, m_iMaxWidth, m_iCh);
            }

            int maxBufStep = bufElemSize*(int)AlignSize(m_iMaxWidth+(!IsSeparable() ? m_szKsize.x-1 : 0), VEC_ALIGN);
            m_au8RingBuf.resize(maxBufStep*m_apu8Rows.size()+VEC_ALIGN);
        }

        // adjust bufstep so that the used part of the ring buffer stays compact in memory
        m_iBufStep = bufElemSize*(int)AlignSize(m_rRoi.size.x + (!IsSeparable() ? m_szKsize.x-1 : 0), VEC_ALIGN);

        m_iDx1 = std::max(m_ptAnchor.x-m_rRoi.leftTop.x, 0);
        m_iDx2 = std::max(m_szKsize.x-m_ptAnchor.x-1+m_rRoi.leftTop.x+m_rRoi.size.x-m_szWholeSize.x, 0);

        // recompute border tables
        if (m_iDx1 > 0 || m_iDx2 > 0)
        {
            if (m_eRowBorderType == BORDER_CONSTANT)
            {
                assert(constVal != NULL);
                int nr = IsSeparable() ? 1 : m_apu8Rows.size();
                for( i = 0; i < nr; i++ )
                {
                    uint8_t* dst = IsSeparable() ? &m_au8SrcRow[0] : AlignPtr(&m_au8RingBuf[0], VEC_ALIGN) + m_iBufStep*i;
                    memcpy(dst, constVal, m_iDx1*esz);
                    memcpy(dst + (m_rRoi.size.x+m_szKsize.x-1-m_iDx2)*esz, constVal, m_iDx2*esz);
                }
            }
            else
            {
                int xofs1 = std::min(m_rRoi.leftTop.x, m_ptAnchor.x)-m_rRoi.leftTop.x;

                int btab_esz = m_iBorderElemSize, wholeWidth = m_szWholeSize.x;
                int* btab = (int*)&m_aiBorderTab[0];

                for( i = 0; i < m_iDx1; i++ )
                {
                    int p0 = (BorderInterpolate(i-m_iDx1, wholeWidth, m_eRowBorderType) + xofs1)*btab_esz;
                    for( j = 0; j < btab_esz; j++ )
                        btab[i*btab_esz + j] = p0 + j;
                }

                for( i = 0; i < m_iDx2; i++ )
                {
                    int p0 = (BorderInterpolate(wholeWidth + i, wholeWidth, m_eRowBorderType) + xofs1)*btab_esz;
                    for( j = 0; j < btab_esz; j++ )
                        btab[(i + m_iDx1)*btab_esz + j] = p0 + j;
                }
            }
        }

        m_iRowCount = m_iDstY = 0;
        m_iStartY = m_iStartY0 = std::max(m_rRoi.leftTop.y-m_ptAnchor.y, 0);
        m_iEndY = std::min(m_rRoi.leftTop.y+m_rRoi.size.y+m_szKsize.y-m_ptAnchor.y-1, m_szWholeSize.y);

        return m_iStartY;
    }

    int Proceed(const uint8_t* p8uSrc, int iSrcStep, int iSrcCount, uint8_t* p8uDst, int iDstStep) override
    {
        assert(m_szWholeSize.x > 0 && m_szWholeSize.y > 0);
        
        const int *btab = &m_aiBorderTab[0];
        int esz = (int)GetElementSize(m_eSrcDtype), btab_esz = m_iBorderElemSize;
        uint8_t** brows = &m_apu8Rows[0];
        int bufRows = (int)m_apu8Rows.size();
        int cn = m_iCh;
        int width = m_rRoi.size.x, kwidth = m_szKsize.x;
        int kheight = m_szKsize.y, ay = m_ptAnchor.y;
        int _dx1 = m_iDx1, _dx2 = m_iDx2;
        int width1 = m_rRoi.size.x+kwidth-1;
        int xofs1 = std::min(m_rRoi.leftTop.x, m_ptAnchor.x);
        bool isSep = IsSeparable();
        bool makeBorder = (_dx1 > 0 || _dx2 > 0) && m_eRowBorderType != BORDER_CONSTANT;
        int dy = 0, i = 0;

        p8uSrc -= xofs1*esz;
        iSrcCount = std::min(iSrcCount, RemainingInputRows());

        assert(p8uSrc && p8uDst && iSrcCount > 0);
        for (;; p8uDst += iDstStep*i, dy += i)
        {
            int dcount = bufRows-ay-m_iStartY-m_iRowCount+m_rRoi.leftTop.y;
            dcount = dcount > 0 ? dcount : bufRows-kheight+1;
            dcount = std::min(dcount, iSrcCount);
            iSrcCount -= dcount;
            for (; dcount-- > 0; p8uSrc += iSrcStep)
            {
                int bi = (m_iStartY-m_iStartY0+m_iRowCount)%bufRows;
                uint8_t* brow = AlignPtr(&m_au8RingBuf[0], VEC_ALIGN)+bi*m_iBufStep;
                uint8_t* row = isSep ? &m_au8SrcRow[0] : brow;

                if (++m_iRowCount > bufRows)
                {
                    --m_iRowCount;
                    ++m_iStartY;
                }

                memcpy(row+_dx1*esz, p8uSrc, (width1-_dx2-_dx1)*esz);

                if (makeBorder)
                {
                    if (btab_esz*(int)sizeof(int) == esz)
                    {
                        const int* isrc = (const int*)p8uSrc;
                        int* irow = (int*)row;

                        for( i = 0; i < _dx1*btab_esz; i++ )
                            irow[i] = isrc[btab[i]];
                        for( i = 0; i < _dx2*btab_esz; i++ )
                            irow[i+(width1-_dx2)*btab_esz] = isrc[btab[i+_dx1*btab_esz]];
                    }
                    else
                    {
                        for( i = 0; i < _dx1*esz; i++ )
                            row[i] = p8uSrc[btab[i]];
                        for( i = 0; i < _dx2*esz; i++ )
                            row[i+(width1-_dx2)*esz] = p8uSrc[btab[i+_dx1*esz]];
                    }
                }

                if (isSep)
                    (*m_hRowFilter)(row, brow, width, m_iCh);
            }

            int max_i = std::min(bufRows, m_rRoi.size.y-(m_iDstY+dy)+(kheight-1));
            for (i = 0; i < max_i; i++)
            {
                int srcY = BorderInterpolate(m_iDstY+dy+i+m_rRoi.leftTop.y-ay, m_szWholeSize.y, m_eColumnBorderType);
                if (srcY < 0) // can happen only with constant border type
                    brows[i] = AlignPtr(&m_au8ConstBorderRow[0], VEC_ALIGN);
                else
                {
                    assert(srcY >= m_iStartY);
                    if (srcY >= m_iStartY+m_iRowCount)
                        break;
                    int bi = (srcY-m_iStartY0)%bufRows;
                    brows[i] = AlignPtr(&m_au8RingBuf[0], VEC_ALIGN) + bi*m_iBufStep;
                }
            }
            if (i < kheight)
                break;
            i -= kheight - 1;
            if (isSep)
                (*m_hColumnFilter)((const uint8_t**)brows, p8uDst, iDstStep, i, m_rRoi.size.x*cn);
            else
                (*m_hMatFilter)((const uint8_t**)brows, p8uDst, iDstStep, i, m_rRoi.size.x, cn);
        }

        m_iDstY += dy;
        assert(m_iDstY <= m_rRoi.size.y);
        return dy;
    }

    ImGui::ImMat Apply(const ImGui::ImMat& mSrc, const Size2i& szSize, const Point2i& ptOfs) override
    {
        ImGui::ImMat mDst;
        mDst.create_type(mSrc.w, mSrc.h, mSrc.c, m_eDstDtype);

        Start(szSize, Size2i(mSrc.w, mSrc.h), ptOfs);
        int y = m_iStartY-ptOfs.y;
        int srcLineSize = mSrc.w*mSrc.elemsize;
        if (mSrc.elempack > 1)
            srcLineSize *= mSrc.c;
        int dstLineSize = mDst.w*mDst.elemsize;
        if (mDst.elempack > 1)
            dstLineSize *= mDst.c;
        Proceed((const uint8_t*)mSrc.data+y*srcLineSize, srcLineSize, m_iEndY-m_iStartY,
                (uint8_t*)mDst.data, dstLineSize);
        return mDst;
    }

private:
    bool IsSeparable() const
    {
        return !m_hMatFilter;
    }

    int RemainingInputRows() const
    {
        return m_iEndY-m_iStartY-m_iRowCount;
    }

    int RemainingOutputRows() const
    {
        return m_rRoi.size.y-m_iDstY;
    }

private:
    ImDataType m_eSrcDtype, m_eDstDtype, m_eBufDtype;
    int m_iCh;
    Size2i m_szKsize;
    Point2i m_ptAnchor;
    int m_iMaxWidth;
    Size2i m_szWholeSize;
    Recti m_rRoi;
    int m_iDx1, m_iDx2;
    BorderTypes m_eRowBorderType;
    BorderTypes m_eColumnBorderType;
    std::vector<int> m_aiBorderTab;
    int m_iBorderElemSize;
    std::vector<uint8_t> m_au8RingBuf;
    std::vector<uint8_t> m_au8SrcRow;
    ImGui::ImMat m_mConstBorderValue;
    std::vector<uint8_t> m_au8ConstBorderRow;
    int m_iBufStep;
    int m_iStartY;
    int m_iStartY0;
    int m_iEndY;
    int m_iRowCount;
    int m_iDstY;
    std::vector<uint8_t*> m_apu8Rows;

    MatFilter::Holder m_hMatFilter;
    RowFilter::Holder m_hRowFilter;
    ColumnFilter::Holder m_hColumnFilter;
};

FilterEngine::Holder FilterEngine::CreateInstance()
{
    return FilterEngine::Holder(new FilterEngineImpl(), [] (FilterEngine* p) {
        FilterEngineImpl* ptr = dynamic_cast<FilterEngineImpl*>(p);
        delete ptr;
    });
}
}
