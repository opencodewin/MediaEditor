#include <climits>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <functional>
#include "Contour2Mask.h"
#include "MatMath.h"
#include "MatFilter.h"
#include "MatUtilsImVecHelper.h"

using namespace std;

namespace MatUtils
{
static bool ClipLine(const Size2l& img_size, Point2l& pt1, Point2l& pt2)
{
    int c1, c2;
    int64_t right = img_size.x-1, bottom = img_size.y-1;
    if (img_size.x <= 0 || img_size.y <= 0)
        return false;

    int64_t &x1 = pt1.x, &y1 = pt1.y, &x2 = pt2.x, &y2 = pt2.y;
    c1 = (x1 < 0) + (x1 > right) * 2 + (y1 < 0) * 4 + (y1 > bottom) * 8;
    c2 = (x2 < 0) + (x2 > right) * 2 + (y2 < 0) * 4 + (y2 > bottom) * 8;
    if ((c1&c2) == 0 && (c1|c2) != 0)
    {
        int64_t a;
        if (c1&12)
        {
            a = c1 < 8 ? 0 : bottom;
            x1 += (int64_t)((double)(a - y1) * (x2 - x1) / (y2 - y1));
            y1 = a;
            c1 = (x1 < 0) + (x1 > right) * 2;
        }
        if (c2&12)
        {
            a = c2 < 8 ? 0 : bottom;
            x2 += (int64_t)((double)(a - y2) * (x2 - x1) / (y2 - y1));
            y2 = a;
            c2 = (x2 < 0) + (x2 > right) * 2;
        }
        if ((c1&c2) == 0 && (c1|c2) != 0)
        {
            if (c1)
            {
                a = c1 == 1 ? 0 : right;
                y1 += (int64_t)((double)(a - x1) * (y2 - y1) / (x2 - x1));
                x1 = a;
                c1 = 0;
            }
            if (c2)
            {
                a = c2 == 1 ? 0 : right;
                y2 += (int64_t)((double)(a - x2) * (y2 - y1) / (x2 - x1));
                x2 = a;
                c2 = 0;
            }
        }
        assert((c1&c2) != 0 || (x1|y1|x2|y2) >= 0);
    }
    return (c1|c2) == 0;
}

static bool ClipLine(const Size2i& imgSize, Point2i& pt1, Point2i& pt2)
{
    Point2l p1(pt1);
    Point2l p2(pt2);
    bool inside = ClipLine(Size2l(imgSize.x, imgSize.y), p1, p2);
    pt1.x = (int32_t)p1.x;
    pt1.y = (int32_t)p1.y;
    pt2.x = (int32_t)p2.x;
    pt2.y = (int32_t)p2.y;
    return inside;
}

class LineIterator
{
public:
    LineIterator(const ImGui::ImMat& img, const Point2i& pt1, const Point2i& pt2,
            int connectivity = 8, bool leftToRight = false)
    {
        init(&img, Recti(0, 0, img.w, img.h), pt1, pt2, connectivity, leftToRight);
        ptmode = false;
    }
    LineIterator(const Point2i& pt1, const Point2i& pt2,
            int connectivity = 8, bool leftToRight = false)
    {
        init(nullptr, Recti(
                    std::min(pt1.x, pt2.x),
                    std::min(pt1.y, pt2.y),
                    std::max(pt1.x, pt2.x) - std::min(pt1.x, pt2.x) + 1,
                    std::max(pt1.y, pt2.y) - std::min(pt1.y, pt2.y) + 1),
            pt1, pt2, connectivity, leftToRight);
        ptmode = true;
    }
    LineIterator(const Size2i& boundingAreaSize, const Point2i& pt1, const Point2i& pt2,
            int connectivity = 8, bool leftToRight = false )
    {
        init(nullptr, Recti(0, 0, boundingAreaSize.x, boundingAreaSize.y),
                pt1, pt2, connectivity, leftToRight);
        ptmode = true;
    }
    LineIterator(const Recti& boundingAreaRect, const Point2i& pt1, const Point2i& pt2,
            int connectivity = 8, bool leftToRight = false)
    {
        init(nullptr, boundingAreaRect, pt1, pt2, connectivity, leftToRight);
        ptmode = true;
    }

    void init(const ImGui::ImMat* img, const Recti& rect, const Point2i& pt1_, const Point2i& pt2_, int connectivity, bool leftToRight)
    {
        assert(connectivity == 8 || connectivity == 4);

        count = -1;
        p = Point2i(0, 0);
        ptr0 = ptr = 0;
        step = elemSize = 0;
        ptmode = !img;

        Point2i pt1 = pt1_-rect.leftTop;
        Point2i pt2 = pt2_-rect.leftTop;

        if( (unsigned)pt1.x >= (unsigned)(rect.size.x) ||
            (unsigned)pt2.x >= (unsigned)(rect.size.x) ||
            (unsigned)pt1.y >= (unsigned)(rect.size.y) ||
            (unsigned)pt2.y >= (unsigned)(rect.size.y) )
        {
            if(!ClipLine(Size2i(rect.size.x, rect.size.y), pt1, pt2))
            {
                err = plusDelta = minusDelta = plusStep = minusStep = plusShift = minusShift = count = 0;
                return;
            }
        }

        pt1 += rect.leftTop;
        pt2 += rect.leftTop;

        int delta_x = 1, delta_y = 1;
        int dx = pt2.x - pt1.x;
        int dy = pt2.y - pt1.y;

        if( dx < 0 )
        {
            if( leftToRight )
            {
                dx = -dx;
                dy = -dy;
                pt1 = pt2;
            }
            else
            {
                dx = -dx;
                delta_x = -1;
            }
        }

        if( dy < 0 )
        {
            dy = -dy;
            delta_y = -1;
        }

        bool vert = dy > dx;
        if( vert )
        {
            std::swap(dx, dy);
            std::swap(delta_x, delta_y);
        }

        assert( dx >= 0 && dy >= 0 );

        if( connectivity == 8 )
        {
            err = dx - (dy + dy);
            plusDelta = dx + dx;
            minusDelta = -(dy + dy);
            minusShift = delta_x;
            plusShift = 0;
            minusStep = 0;
            plusStep = delta_y;
            count = dx + 1;
        }
        else /* connectivity == 4 */
        {
            err = 0;
            plusDelta = (dx + dx) + (dy + dy);
            minusDelta = -(dy + dy);
            minusShift = delta_x;
            plusShift = -delta_x;
            minusStep = 0;
            plusStep = delta_y;
            count = dx + dy + 1;
        }

        if( vert )
        {
            std::swap(plusStep, plusShift);
            std::swap(minusStep, minusShift);
        }

        p = pt1;
        if (!ptmode)
        {
            ptr0 = (const uint8_t*)img->data;
            step = (int)(img->w*img->elemsize);
            elemSize = (int)img->elemsize;
            ptr = (uint8_t*)ptr0+(size_t)p.y*step+(size_t)p.x*elemSize;
            plusStep = plusStep*step + plusShift*elemSize;
            minusStep = minusStep*step + minusShift*elemSize;
        }
    }

    uint8_t* operator *()
    {
        return ptmode ? 0 : ptr;
    }

    LineIterator& operator ++()
    {
        int mask = err < 0 ? -1 : 0;
        err += minusDelta + (plusDelta & mask);
        if(!ptmode)
        {
            ptr += minusStep + (plusStep & mask);
        }
        else
        {
            p.x += minusShift + (plusShift & mask);
            p.y += minusStep + (plusStep & mask);
        }
        return *this;
    }

    LineIterator operator ++(int)
    {
        LineIterator it = *this;
        ++(*this);
        return it;
    }

    Point2i pos() const
    {
        if(!ptmode)
        {
            size_t offset = (size_t)(ptr - ptr0);
            int y = (int)(offset/step);
            int x = (int)((offset - (size_t)y*step)/elemSize);
            return Point2i(x, y);
        }
        return p;
    }

    uint8_t* ptr;
    const uint8_t* ptr0;
    int step, elemSize;
    int err, count;
    int minusDelta, plusDelta;
    int minusStep, plusStep;
    int minusShift, plusShift;
    Point2i p;
    bool ptmode;
};

static void
Line(ImGui::ImMat& img, const Point2i& pt1, const Point2i& pt2,
        const void* _color, int connectivity = 8)
{
    if( connectivity == 0 )
        connectivity = 8;
    else if( connectivity == 1 )
        connectivity = 4;

    LineIterator iterator(img, pt1, pt2, connectivity, true);
    int i, count = iterator.count;
    int pix_size = (int)img.elemsize;
    const uint8_t* color = (const uint8_t*)_color;

    if (pix_size == 3)
    {
        for (i = 0; i < count; i++, ++iterator)
        {
            uint8_t* ptr = *iterator;
            ptr[0] = color[0];
            ptr[1] = color[1];
            ptr[2] = color[2];
        }
    }
    else
    {
        for (i = 0; i < count; i++, ++iterator)
        {
            uint8_t* ptr = *iterator;
            if (pix_size == 1)
                ptr[0] = color[0];
            else
                memcpy(*iterator, color, pix_size);
        }
    }
}

#define XY_SHIFT 16
#define XY_ONE 65536

/* Correction table depent on the slope */
static const uint8_t SlopeCorrTable[] = {
    181, 181, 181, 182, 182, 183, 184, 185, 187, 188, 190, 192, 194, 196, 198, 201,
    203, 206, 209, 211, 214, 218, 221, 224, 227, 231, 235, 238, 242, 246, 250, 254
};

/* Gaussian for antialiasing filter */
static const int FilterTable[] = {
    168, 177, 185, 194, 202, 210, 218, 224, 231, 236, 241, 246, 249, 252, 254, 254,
    254, 254, 252, 249, 246, 241, 236, 231, 224, 218, 210, 202, 194, 185, 177, 168,
    158, 149, 140, 131, 122, 114, 105, 97, 89, 82, 75, 68, 62, 56, 50, 45,
    40, 36, 32, 28, 25, 22, 19, 16, 14, 12, 11, 9, 8, 7, 5, 5
};

static inline void PutPoint3C8U(uint8_t* ptr, int x, int y, size_t line_size, const int rgb[3], int alpha)
{
    uint8_t* tptr = ptr+x*3+y*line_size;
    const int &r = rgb[0], &g = rgb[1], &b = rgb[2];
    int _r, _g, _b;
    _b = tptr[0];
    _b += ((b-_b)*alpha+127)>> 8;
    _b += ((b-_b)*alpha+127)>> 8;
    _g = tptr[1];
    _g += ((g-_g)*alpha+127)>> 8;
    _g += ((g-_g)*alpha+127)>> 8;
    _r = tptr[2];
    _r += ((r-_r)*alpha+127)>> 8;
    _r += ((r-_r)*alpha+127)>> 8;
    tptr[0] = (uint8_t)_b;
    tptr[1] = (uint8_t)_g;
    tptr[2] = (uint8_t)_r;
}

static inline void PutPoint1C8U(uint8_t* ptr, int x, int y, size_t line_size, const int rgb[1], int alpha)
{
    uint8_t* tptr = ptr+x+y*line_size;
    const int &g = rgb[0];
    int _g;
    _g = tptr[0];
    _g += ((g-_g)*alpha+127)>> 8;
    _g += ((g-_g)*alpha+127)>> 8;
    tptr[0] = (uint8_t)_g;
}

static inline void PutPoint4C8U(uint8_t* ptr, int x, int y, size_t line_size, const int rgb[4], int alpha)
{
    uint8_t* tptr = ptr+x*4+y*line_size;
    const int &r = rgb[0], &g = rgb[1], &b = rgb[2], &a = rgb[3];
    int _r, _g, _b, _a;
    _b = tptr[0];
    _b += ((b-_b)*alpha+127)>> 8;
    _b += ((b-_b)*alpha+127)>> 8;
    _g = tptr[1];
    _g += ((g-_g)*alpha+127)>> 8;
    _g += ((g-_g)*alpha+127)>> 8;
    _r = tptr[2];
    _r += ((r-_r)*alpha+127)>> 8;
    _r += ((r-_r)*alpha+127)>> 8;
    _a = tptr[3];
    _a += ((a-_a)*alpha+127)>> 8;
    _a += ((a-_a)*alpha+127)>> 8;
    tptr[0] = (uint8_t)_b;
    tptr[1] = (uint8_t)_g;
    tptr[2] = (uint8_t)_r;
    tptr[3] = (uint8_t)_a;
}

static inline void PutPoint3C32F(uint8_t* ptr, int x, int y, size_t line_size, const float rgb[3], int _alpha)
{
    float* tptr = (float*)(ptr+x*12+y*line_size);
    const float &r = rgb[0], &g = rgb[1], &b = rgb[2];
    const float alpha = (float)_alpha/255.f;
    float _r, _g, _b;
    _b = tptr[0];
    _b += (b-_b)*alpha;
    _b += (b-_b)*alpha;
    _g = tptr[1];
    _g += (g-_g)*alpha;
    _g += (g-_g)*alpha;
    _r = tptr[2];
    _r += (r-_r)*alpha;
    _r += (r-_r)*alpha;
    tptr[0] = _b;
    tptr[1] = _g;
    tptr[2] = _r;
}

static inline void PutPoint1C32F(uint8_t* ptr, int x, int y, size_t line_size, const float rgb[1], int _alpha)
{
    float* tptr = (float*)(ptr+x*4+y*line_size);
    const float &g = rgb[0];
    const float alpha = (float)_alpha/255.f;
    float _g;
    _g = tptr[0];
    _g += (g-_g)*alpha;
    _g += (g-_g)*alpha;
    tptr[0] = _g;
}

static inline void PutPoint4C32F(uint8_t* ptr, int x, int y, size_t line_size, const float rgb[4], int _alpha)
{
    float* tptr = (float*)(ptr+x*16+y*line_size);
    const float &r = rgb[0], &g = rgb[1], &b = rgb[2], &a = rgb[3];
    const float alpha = (float)_alpha/255.f;
    float _r, _g, _b, _a;
    _b = tptr[0];
    _b += (b-_b)*alpha;
    _b += (b-_b)*alpha;
    _g = tptr[1];
    _g += (g-_g)*alpha;
    _g += (g-_g)*alpha;
    _r = tptr[2];
    _r += (r-_r)*alpha;
    _r += (r-_r)*alpha;
    _a = tptr[3];
    _a += (a-_a)*alpha;
    _a += (a-_a)*alpha;
    tptr[0] = _b;
    tptr[1] = _g;
    tptr[2] = _r;
    tptr[3] = _a;
}

static void
LineAA(ImGui::ImMat& img, Point2l pt1, Point2l pt2, const void* _pColor)
{
    int64_t dx, dy;
    int ecount, scount = 0;
    int slope;
    int64_t ax, ay;
    int64_t xStep, yStep;
    int64_t i, j;
    int epTable[9];
    int nch = img.c;
    uint8_t* ptr = (uint8_t*)img.data;
    size_t lineSize = img.w*img.c*img.elemsize;
    Size2l size0(img.w, img.h), size = size0;

    if (!((nch == 1 || nch == 3 || nch == 4) && (img.type == IM_DT_INT8 || img.type == IM_DT_FLOAT32)))
    {
        Line(img, Point2i((int)(pt1.x>>XY_SHIFT), (int)(pt1.y>>XY_SHIFT)), Point2i((int)(pt2.x>>XY_SHIFT), (int)(pt2.y>>XY_SHIFT)), _pColor);
        return;
    }

    size.x <<= XY_SHIFT;
    size.y <<= XY_SHIFT;
    if (!ClipLine(size, pt1, pt2))
        return;

    dx = pt2.x - pt1.x;
    dy = pt2.y - pt1.y;

    j = dx < 0 ? -1 : 0;
    ax = (dx ^ j) - j;
    i = dy < 0 ? -1 : 0;
    ay = (dy ^ i) - i;

    if (ax > ay)
    {
        dy = (dy ^ j) - j;
        pt1.x ^= pt2.x & j;
        pt2.x ^= pt1.x & j;
        pt1.x ^= pt2.x & j;
        pt1.y ^= pt2.y & j;
        pt2.y ^= pt1.y & j;
        pt1.y ^= pt2.y & j;

        xStep = XY_ONE;
        yStep = (dy << XY_SHIFT) / (ax | 1);
        pt2.x += XY_ONE;
        ecount = (int)((pt2.x >> XY_SHIFT) - (pt1.x >> XY_SHIFT));
        j = -(pt1.x & (XY_ONE - 1));
        pt1.y += ((yStep * j) >> XY_SHIFT) + (XY_ONE >> 1);
        slope = (yStep >> (XY_SHIFT - 5)) & 0x3f;
        slope ^= (yStep < 0 ? 0x3f : 0);

        /* Get 4-bit fractions for end-point adjustments */
        i = (pt1.x >> (XY_SHIFT - 7)) & 0x78;
        j = (pt2.x >> (XY_SHIFT - 7)) & 0x78;
    }
    else
    {
        dx = (dx ^ i) - i;
        pt1.x ^= pt2.x & i;
        pt2.x ^= pt1.x & i;
        pt1.x ^= pt2.x & i;
        pt1.y ^= pt2.y & i;
        pt2.y ^= pt1.y & i;
        pt1.y ^= pt2.y & i;

        xStep = (dx << XY_SHIFT) / (ay | 1);
        yStep = XY_ONE;
        pt2.y += XY_ONE;
        ecount = (int)((pt2.y >> XY_SHIFT) - (pt1.y >> XY_SHIFT));
        j = -(pt1.y & (XY_ONE - 1));
        pt1.x += ((xStep * j) >> XY_SHIFT) + (XY_ONE >> 1);
        slope = (xStep >> (XY_SHIFT - 5)) & 0x3f;
        slope ^= (xStep < 0 ? 0x3f : 0);

        /* Get 4-bit fractions for end-point adjustments */
        i = (pt1.y >> (XY_SHIFT - 7)) & 0x78;
        j = (pt2.y >> (XY_SHIFT - 7)) & 0x78;
    }

    slope = (slope & 0x20) ? 0x100 : SlopeCorrTable[slope];

    /* Calc end point correction table */
    {
        int t0 = slope << 7;
        int t1 = ((0x78 - (int)i) | 4) * slope;
        int t2 = ((int)j | 4) * slope;

        epTable[0] = 0;
        epTable[8] = slope;
        epTable[1] = epTable[3] = ((((j - i) & 0x78) | 4) * slope >> 8) & 0x1ff;
        epTable[2] = (t1 >> 8) & 0x1ff;
        epTable[4] = ((((j - i) + 0x80) | 4) * slope >> 8) & 0x1ff;
        epTable[5] = ((t1 + t0) >> 8) & 0x1ff;
        epTable[6] = (t2 >> 8) & 0x1ff;
        epTable[7] = ((t2 + t0) >> 8) & 0x1ff;
    }

    #define LINEAA_CALC_POINTS(PutPointFunc) \
        if (ax > ay) { \
            int x = (int)(pt1.x >> XY_SHIFT); \
            for (; ecount >= 0; x++, pt1.y += yStep, scount++, ecount--) { \
                if( (unsigned)x >= (unsigned)size0.x ) \
                    continue; \
                int y = (int)((pt1.y >> XY_SHIFT) - 1); \
                int ep_corr = epTable[(((scount >= 2) + 1) & (scount | 2)) * 3 + \
                                    (((ecount >= 2) + 1) & (ecount | 2))]; \
                int a, dist = (pt1.y >> (XY_SHIFT - 5)) & 31; \
                a = (ep_corr * FilterTable[dist + 32] >> 8) & 0xff; \
                if ((unsigned)y < (unsigned)size0.y) \
                    PutPointFunc(ptr, x, y, lineSize, pColor, a); \
                a = (ep_corr * FilterTable[dist] >> 8) & 0xff; \
                if ((unsigned)(y+1) < (unsigned)size0.y) \
                    PutPointFunc(ptr, x, y+1, lineSize, pColor, a); \
                a = (ep_corr * FilterTable[63 - dist] >> 8) & 0xff; \
                if ((unsigned)(y+2) < (unsigned)size0.y) \
                    PutPointFunc(ptr, x, y+2, lineSize, pColor, a); \
            } \
        } \
        else { \
            int y = (int)(pt1.y >> XY_SHIFT); \
            for (; ecount >= 0; y++, pt1.x += xStep, scount++, ecount--) { \
                if ((unsigned)y >= (unsigned)size0.y) \
                    continue; \
                int x = (int)((pt1.x >> XY_SHIFT) - 1); \
                int ep_corr = epTable[(((scount >= 2) + 1) & (scount | 2)) * 3 + \
                                    (((ecount >= 2) + 1) & (ecount | 2))]; \
                int a, dist = (pt1.x >> (XY_SHIFT - 5)) & 31; \
                a = (ep_corr * FilterTable[dist + 32] >> 8) & 0xff; \
                if ((unsigned)x < (unsigned)size0.x) \
                    PutPointFunc(ptr, x, y, lineSize, pColor, a); \
                a = (ep_corr * FilterTable[dist] >> 8) & 0xff; \
                if ((unsigned)(x+1) < (unsigned)size0.x) \
                    PutPointFunc(ptr, x+1, y, lineSize, pColor, a); \
                a = (ep_corr * FilterTable[63 - dist] >> 8) & 0xff; \
                if ((unsigned)(x+2) < (unsigned)size0.x) \
                    PutPointFunc(ptr, x+2, y, lineSize, pColor, a); \
            } \
        }

    if (img.type == IM_DT_INT8)
    {
        const int* pColor = (const int*)_pColor;
        if (nch == 3)
        {
            LINEAA_CALC_POINTS(PutPoint3C8U);
        }
        else if (nch == 1)
        {
            LINEAA_CALC_POINTS(PutPoint1C8U);
        }
        else if (nch == 4)
        {
            LINEAA_CALC_POINTS(PutPoint4C8U);
        }
        else
        {
            throw runtime_error("INVALID code branch.");
        }
    }
    else if (img.type == IM_DT_FLOAT32)
    {
        const float* pColor = (const float*)_pColor;
        if (nch == 3)
        {
            LINEAA_CALC_POINTS(PutPoint3C32F);
        }
        else if (nch == 1)
        {
            LINEAA_CALC_POINTS(PutPoint1C32F);
        }
        else if (nch == 4)
        {
            LINEAA_CALC_POINTS(PutPoint4C32F);
        }
        else
        {
            throw runtime_error("INVALID code branch.");
        }
    }
    else
    {
        throw runtime_error("INVALID code branch.");
    }
}

struct _PolyEdge
{
    _PolyEdge() : y0(0), y1(0), x(0), dx(0), next(nullptr) {}

    int y0, y1;
    int64_t x, dx;
    _PolyEdge* next;
};

static void CollectPolyEdges(ImGui::ImMat& img, const vector<Point2l>& vertices, vector<_PolyEdge>& edges, const void* pColor, int lineType, const Point2l& offset, int shift)
{
    int i, count = vertices.size();
    Point2l pt0 = vertices[count-1], pt1;
    int delta = offset.y+((1<<shift)>>1), scalar = 1<<shift;
    pt0.x = (pt0.x+offset.x) << (XY_SHIFT-shift);
    pt0.y = (pt0.y+delta) >> shift;

    ImPixel color;
    if (lineType > 2)
    {
        if (img.c == 1)
        {
            if (img.type == IM_DT_INT8)
            {
                float g = (float)(*((const uint8_t*)pColor))/(float)UINT8_MAX;
                color.r = color.g = color.b = g;
                color.a = 1.f;
            }
            else if (img.type == IM_DT_FLOAT32)
            {
                color.r = color.g = color.b = *((const float*)pColor);
                color.a = 1.f;
            }
        }
        else if (img.c == 3)
        {
            if (img.type == IM_DT_INT8)
            {
                const uint8_t* pu8Color = (const uint8_t*)pColor;
                color.r = (float)pu8Color[0]/(float)UINT8_MAX;
                color.g = (float)pu8Color[1]/(float)UINT8_MAX;
                color.b = (float)pu8Color[2]/(float)UINT8_MAX;
                color.a = 1.f;
            }
            else if (img.type == IM_DT_FLOAT32)
            {
                const float* pf32Color = (const float*)pColor;
                color.r = pf32Color[0];
                color.g = pf32Color[1];
                color.b = pf32Color[2];
                color.a = 1.f;
            }
            else
                throw runtime_error("NOT IMPLEMENTED YET!");
        }
    }

    edges.reserve(vertices.size()+1);
    for( i = 0; i < count; i++, pt0 = pt1 )
    {
        _PolyEdge edge;
        pt1 = vertices[i];
        pt1.x = (pt1.x+offset.x) << (XY_SHIFT-shift);
        pt1.y = (pt1.y+delta) >> shift;

        if (lineType < 2)
        {
            Point2i t0, t1;
            t0.y = pt0.y; t1.y = pt1.y;
            t0.x = (pt0.x+(XY_ONE>>1)) >> XY_SHIFT;
            t1.x = (pt1.x+(XY_ONE>>1)) >> XY_SHIFT;
            Line(img, t0, t1, pColor, lineType);
        }
        else if (lineType == 2)
        {
            const int j = i == 0 ? count-1 : i-1;
            const auto& fp0 = vertices[j];
            const auto& fp1 = vertices[i];
            Point2l t0, t1;
            t0.x = pt0.x; t1.x = pt1.x;
            t0.y = (fp0.y+offset.y) << (XY_SHIFT-shift);
            t1.y = (fp1.y+offset.y) << (XY_SHIFT-shift);
            LineAA(img, t0, t1, pColor);
        }
        else
        {
            const int j = i == 0 ? count-1 : i-1;
            const auto& fp0 = vertices[j];
            const auto& fp1 = vertices[i];
            const ImPoint p0((float)((double)fp0.x/scalar), (float)((double)fp0.y/scalar)), p1((float)((double)fp1.x/scalar), (float)((double)fp1.y/scalar));
            img.draw_line(p0, p1, 0.5f, color);
        }

        if( pt0.y == pt1.y )
            continue;

        if( pt0.y < pt1.y )
        {
            edge.y0 = (int)(pt0.y);
            edge.y1 = (int)(pt1.y);
            edge.x = pt0.x;
        }
        else
        {
            edge.y0 = (int)(pt1.y);
            edge.y1 = (int)(pt0.y);
            edge.x = pt1.x;
        }
        edge.dx = (pt1.x - pt0.x) / (pt1.y - pt0.y);
        edges.push_back(edge);
    }
}

struct _CmpEdges
{
    bool operator ()(const _PolyEdge& e1, const _PolyEdge& e2)
    {
        return e1.y0 - e2.y0 ? e1.y0 < e2.y0 :
            e1.x - e2.x ? e1.x < e2.x : e1.dx < e2.dx;
    }
};

static void HLine(uint8_t* pLine, int x1, int x2, const void* pColor, int pix_size)
{
    if (pix_size == 1)
    {
        memset(pLine+x1, *((uint8_t*)pColor), x2-x1);
    }
    else if (pix_size == 2)
    {
        uint16_t* pDst = ((uint16_t*)pLine)+x1;
        uint16_t* pEnd = ((uint16_t*)pLine)+x2;
        const uint16_t value = *(const uint16_t*)pColor;
        while (pDst != pEnd)
            *pDst++ = value;
    }
    else if (pix_size == 4)
    {
        uint32_t* pDst = ((uint32_t*)pLine)+x1;
        uint32_t* pEnd = ((uint32_t*)pLine)+x2;
        const uint32_t value = *(const uint32_t*)pColor;
        while (pDst != pEnd)
            *pDst++ = value;
    }
    else if (pix_size == 8)
    {
        uint64_t* pDst = ((uint64_t*)pLine)+x1;
        uint64_t* pEnd = ((uint64_t*)pLine)+x2;
        const uint64_t value = *(const uint64_t*)pColor;
        while (pDst != pEnd)
            *pDst++ = value;
    }
    else
    {
        uint8_t* pDst = pLine+x1*pix_size;
        uint8_t* pEnd = pLine+x2*pix_size;
        while (pDst != pEnd)
        {
            memcpy(pDst, pColor, pix_size);
            pDst += pix_size;
        }
    }
}

static void
FillEdgeCollection(ImGui::ImMat& img, vector<_PolyEdge>& edges, const void* pColor)
{
    _PolyEdge tmp;
    int i, y, total = (int)edges.size();
    Size2i size(img.w, img.h);
    _PolyEdge* e;
    int y_max = INT_MIN, y_min = INT_MAX;
    int64_t x_max = 0xFFFFFFFFFFFFFFFF, x_min = 0x7FFFFFFFFFFFFFFF;
    int pix_size = (int)img.elemsize;
    if( total < 2 )
        return;

    for( i = 0; i < total; i++ )
    {
        const auto& e1 = edges[i];
        assert(e1.y0 < e1.y1);
        // Determine x-coordinate of the end of the edge.
        // (This is not necessary x-coordinate of any vertex in the array.)
        int64_t x1 = e1.x + (e1.y1 - e1.y0) * e1.dx;
        y_min = std::min(y_min, e1.y0);
        y_max = std::max(y_max, e1.y1);
        x_min = std::min(x_min, e1.x);
        x_max = std::max(x_max, e1.x);
        x_min = std::min(x_min, x1);
        x_max = std::max(x_max, x1);
    }

    if( y_max < 0 || y_min >= size.y || x_max < 0 || x_min >= ((int64_t)size.x<<XY_SHIFT) )
        return;

    std::sort(edges.begin(), edges.end(), _CmpEdges());

    // start drawing
    tmp.y0 = INT_MAX;
    edges.push_back(tmp); // after this point we do not add
                          // any elements to edges, thus we can use pointers
    i = 0;
    tmp.next = 0;
    e = &edges[i];
    y_max = std::min(y_max, size.y);

    for( y = e->y0; y < y_max; y++ )
    {
        _PolyEdge *last, *prelast, *keep_prelast;
        int draw = 0;
        int clipline = y < 0;

        prelast = &tmp;
        last = tmp.next;
        while( last || e->y0 == y )
        {
            if( last && last->y1 == y )
            {
                // exclude edge if y reaches its lower point
                prelast->next = last->next;
                last = last->next;
                continue;
            }
            keep_prelast = prelast;
            if( last && (e->y0 > y || last->x < e->x) )
            {
                // go to the next edge in active list
                prelast = last;
                last = last->next;
            }
            else if( i < total )
            {
                // insert new edge into active list if y reaches its upper point
                prelast->next = e;
                e->next = last;
                prelast = e;
                e = &edges[++i];
            }
            else
                break;

            if (draw)
            {
                if (!clipline)
                {
                    // convert x's from fixed-point to image coordinates
                    int x1, x2;
                    if (keep_prelast->x > prelast->x)
                    {
                        x1 = (int)((prelast->x+XY_ONE-1) >> XY_SHIFT);
                        x2 = (int)((keep_prelast->x+(XY_ONE-1)) >> XY_SHIFT);
                    }
                    else
                    {
                        x1 = (int)((keep_prelast->x+XY_ONE-1) >> XY_SHIFT);
                        x2 = (int)((prelast->x+XY_ONE-1) >> XY_SHIFT);
                    }

                    // clip and draw the line
                    if (x1 < size.x && x2 >= 0)
                    {
                        if (x1 < 0)
                            x1 = 0;
                        if (x2 >= size.x)
                            x2 = size.x-1;
                        uint8_t* pLine = (uint8_t*)img.data+y*img.w*img.elemsize;
                        HLine(pLine, x1, x2, pColor, pix_size);
                    }
                }
                keep_prelast->x += keep_prelast->dx;
                prelast->x += prelast->dx;
            }
            draw ^= 1;
        }

        // sort edges (using bubble sort)
        keep_prelast = 0;

        do
        {
            prelast = &tmp;
            last = tmp.next;
            _PolyEdge *last_exchange = 0;

            while( last != keep_prelast && last->next != 0 )
            {
                _PolyEdge *te = last->next;

                // swap edges
                if( last->x > te->x )
                {
                    prelast->next = te;
                    last->next = te->next;
                    te->next = last;
                    prelast = te;
                    last_exchange = prelast;
                }
                else
                {
                    prelast = last;
                    last = te;
                }
            }
            if (last_exchange == NULL)
                break;
            keep_prelast = last_exchange;
        } while( keep_prelast != tmp.next && keep_prelast != &tmp );
    }
}

template<template<class T, class Alloc = std::allocator<T>> class Container>
void FeatherFillHLineU8(const Container<Point2f>& aPolygonVertices, float fFeatherSize, int y, uint8_t* pLine, int x1, int x2, const void* pColor)
{
    int iLoopCnt = x2-x1;
    uint8_t* pWrite = pLine;
    const float color = *(uint8_t*)pColor;
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = x1; i < x2; i++)
    {
        float fMinDist;
        CalcNearestPointOnPloygon(Point2f(i, y), aPolygonVertices, nullptr, nullptr, &fMinDist);
        const float factor = fMinDist < fFeatherSize ? fMinDist/fFeatherSize : 1.0f;
        pWrite[i] = (uint8_t)(color*factor);
    }
}

template<template<class T, class Alloc = std::allocator<T>> class Container>
void FeatherFillHLineU16(const Container<Point2f>& aPolygonVertices, float fFeatherSize, int y, uint8_t* pLine, int x1, int x2, const void* pColor)
{
    int iLoopCnt = x2-x1;
    uint16_t* pWrite = ((uint16_t*)pLine);
    const float color = *(uint16_t*)pColor;
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = x1; i < x2; i++)
    {
        float fMinDist;
        CalcNearestPointOnPloygon(Point2f(i, y), aPolygonVertices, nullptr, nullptr, &fMinDist);
        const float factor = fMinDist < fFeatherSize ? fMinDist/fFeatherSize : 1.0f;
        pWrite[i] = (uint16_t)(color*factor);
    }
}

template<template<class T, class Alloc = std::allocator<T>> class Container>
void FeatherFillHLineF32(const Container<Point2f>& aPolygonVertices, float fFeatherSize, int y, uint8_t* pLine, int x1, int x2, const void* pColor)
{
    int iLoopCnt = x2-x1;
    float* pWrite = ((float*)pLine);
    const float color = *(float*)pColor;
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int i = x1; i < x2; i++)
    {
        float fMinDist;
        CalcNearestPointOnPloygon(Point2f(i, y), aPolygonVertices, nullptr, nullptr, &fMinDist);
        const float factor = fMinDist < fFeatherSize ? fMinDist/fFeatherSize : 1.0f;
        pWrite[i] = color*factor;
    }
}

template<template<class T, class Alloc = std::allocator<T>> class Container>
void FillEdgeCollectionWithFeatherEffect(ImGui::ImMat& img, vector<_PolyEdge>& edges, const void* pColor, const Container<Point2f>& aPolygonVertices, float fFeatherSize)
{
    const ImDataType eDtype = img.type;
    if (eDtype != IM_DT_INT8 && eDtype != IM_DT_INT16 && eDtype != IM_DT_FLOAT32)
        throw runtime_error("INVALID data type!");
    function<void(const Container<Point2f>&,float,int,uint8_t*,int,int,const void*)> fnHLine;
    if (eDtype == IM_DT_INT8)
        fnHLine = FeatherFillHLineU8<Container>;
    else if (eDtype == IM_DT_INT16)
        fnHLine = FeatherFillHLineU16<Container>;
    else if (eDtype == IM_DT_FLOAT32)
        fnHLine = FeatherFillHLineF32<Container>;

    _PolyEdge tmp;
    int i, y, total = (int)edges.size();
    Size2i size(img.w, img.h);
    _PolyEdge* e;
    int y_max = INT_MIN, y_min = INT_MAX;
    int64_t x_max = 0xFFFFFFFFFFFFFFFF, x_min = 0x7FFFFFFFFFFFFFFF;
    if( total < 2 )
        return;

    for (i = 0; i < total; i++)
    {
        const auto& e1 = edges[i];
        assert(e1.y0 < e1.y1);
        // Determine x-coordinate of the end of the edge.
        // (This is not necessary x-coordinate of any vertex in the array.)
        int64_t x1 = e1.x + (e1.y1 - e1.y0) * e1.dx;
        y_min = std::min(y_min, e1.y0);
        y_max = std::max(y_max, e1.y1);
        x_min = std::min(x_min, e1.x);
        x_max = std::max(x_max, e1.x);
        x_min = std::min(x_min, x1);
        x_max = std::max(x_max, x1);
    }

    if (y_max < 0 || y_min >= size.y || x_max < 0 || x_min >= ((int64_t)size.x<<XY_SHIFT))
        return;

    std::sort(edges.begin(), edges.end(), _CmpEdges());

    // start drawing
    tmp.y0 = INT_MAX;
    edges.push_back(tmp); // after this point we do not add
                          // any elements to edges, thus we can use pointers
    i = 0;
    tmp.next = 0;
    e = &edges[i];
    y_max = std::min(y_max, size.y);

    for (y = e->y0; y < y_max; y++)
    {
        _PolyEdge *last, *prelast, *keep_prelast;
        int draw = 0;
        int clipline = y < 0;

        prelast = &tmp;
        last = tmp.next;
        while (last || e->y0 == y)
        {
            if (last && last->y1 == y)
            {
                // exclude edge if y reaches its lower point
                prelast->next = last->next;
                last = last->next;
                continue;
            }
            keep_prelast = prelast;
            if (last && (e->y0 > y || last->x < e->x))
            {
                // go to the next edge in active list
                prelast = last;
                last = last->next;
            }
            else if (i < total)
            {
                // insert new edge into active list if y reaches its upper point
                prelast->next = e;
                e->next = last;
                prelast = e;
                e = &edges[++i];
            }
            else
                break;

            if (draw)
            {
                if (!clipline)
                {
                    // convert x's from fixed-point to image coordinates
                    int x1, x2;
                    if (keep_prelast->x > prelast->x)
                    {
                        x1 = (int)((prelast->x+XY_ONE-1) >> XY_SHIFT);
                        x2 = (int)((keep_prelast->x+(XY_ONE-1)) >> XY_SHIFT);
                    }
                    else
                    {
                        x1 = (int)((keep_prelast->x+XY_ONE-1) >> XY_SHIFT);
                        x2 = (int)((prelast->x+XY_ONE-1) >> XY_SHIFT);
                    }

                    // clip and draw the line
                    if (x1 < size.x && x2 >= 0)
                    {
                        if (x1 < 0)
                            x1 = 0;
                        if (x2 >= size.x)
                            x2 = size.x-1;
                        uint8_t* pLine = (uint8_t*)img.data+y*img.w*img.elemsize;
                        fnHLine(aPolygonVertices, fFeatherSize, y, pLine, x1, x2, pColor);
                    }
                }
                keep_prelast->x += keep_prelast->dx;
                prelast->x += prelast->dx;
            }
            draw ^= 1;
        }

        // sort edges (using bubble sort)
        keep_prelast = 0;

        do
        {
            prelast = &tmp;
            last = tmp.next;
            _PolyEdge *last_exchange = 0;

            while (last != keep_prelast && last->next != 0)
            {
                _PolyEdge *te = last->next;

                // swap edges
                if (last->x > te->x)
                {
                    prelast->next = te;
                    last->next = te->next;
                    te->next = last;
                    prelast = te;
                    last_exchange = prelast;
                }
                else
                {
                    prelast = last;
                    last = te;
                }
            }
            if (last_exchange == NULL)
                break;
            keep_prelast = last_exchange;
        } while (keep_prelast != tmp.next && keep_prelast != &tmp);
    }
}

ImGui::ImMat MakeColor(ImDataType eDtype, double dColorVal)
{
    ImGui::ImMat color;
    color.create_type(1, eDtype);
    switch (eDtype)
    {
    case IM_DT_INT8:
        color.at<uint8_t>(0) = static_cast<uint8_t>(dColorVal);
        break;
    case IM_DT_INT16:
        color.at<uint16_t>(0) = static_cast<uint16_t>(dColorVal);
        break;
    case IM_DT_INT16_BE:
    {
        uint16_t be16value = static_cast<uint16_t>(dColorVal);
        color.at<uint16_t>(0) = ((be16value>>8)|((be16value&0xff)<<8));
        break;
    }
    case IM_DT_INT32:
        color.at<int32_t>(0) = static_cast<int32_t>(dColorVal);
        break;
    case IM_DT_INT64:
        color.at<int64_t>(0) = static_cast<int64_t>(dColorVal);
        break;
    case IM_DT_FLOAT32:
        color.at<float>(0) = static_cast<float>(dColorVal);
        break;
    case IM_DT_FLOAT64:
        color.at<double>(0) = dColorVal;
        break;
    default:
        throw runtime_error("UNSUPPORTED image data type!");
    }
    return color;
}

void DrawPolygon(ImGui::ImMat& img, const std::vector<Point2l>& aPolygonVertices, int iFixPointShift, const ImGui::ImMat& color, int iLineType)
{
    if (aPolygonVertices.empty())
        return;
    vector<_PolyEdge> edges;
    auto orgDims = img.dims;
    img.dims = 3; // wyvern: to pass the assertion in ImMat::draw_line()
    CollectPolyEdges(img, aPolygonVertices, edges, color.data, iLineType, {0, 0}, iFixPointShift);
    img.dims = orgDims;
}

template<template<class T, class Alloc = std::allocator<T>> class Container>
ImGui::ImMat _Contour2Mask(
        const Size2i& szMaskSize, const Container<Point2f>& aPolygonVertices, const Point2f& ptOffset,
        ImDataType dtMaskDataType, double dMaskValue, double dNonMaskValue, int iLineType, bool bFilled, int iFeatherKsize)
{
    ImGui::ImMat mask;
    mask.create_type((int)szMaskSize.x, (int)szMaskSize.y, dtMaskDataType);
    switch (dtMaskDataType)
    {
    case IM_DT_INT8:
    {
        uint8_t fillValue = static_cast<uint8_t>(dNonMaskValue);
        mask.fill(*((int8_t*)&fillValue));
        break;
    }
    case IM_DT_INT16:
    {
        uint16_t fillValue = static_cast<uint16_t>(dNonMaskValue);
        mask.fill(*((int16_t*)&fillValue));
        break;
    }
    case IM_DT_INT32:
    {
        int32_t fillValue = static_cast<int32_t>(dNonMaskValue);
        mask.fill(fillValue);
        break;
    }
    case IM_DT_INT64:
    {
        int64_t fillValue = static_cast<int64_t>(dNonMaskValue);
        mask.fill(fillValue);
        break;
    }
    case IM_DT_FLOAT32:
    {
        float fillValue = static_cast<float>(dNonMaskValue);
        mask.fill(fillValue);
        break;
    }
    case IM_DT_FLOAT64:
    {
        double fillValue = dNonMaskValue;
        mask.fill(fillValue);
        break;
    }
    default:
        throw runtime_error("UNSUPPORTED mask image data type!");
    }
    if (aPolygonVertices.empty())
        return mask;

    ImGui::ImMat color = MakeColor(mask.type, dMaskValue);
    const int iFixPointShift = 8;
    const auto aPolygonVertices_ = ConvertFixPointPolygonVertices(aPolygonVertices, iFixPointShift);
    const double dFixPointScale = (double)(1LL << iFixPointShift);
    const MatUtils::Point2l ptOffset_((int64_t)((double)ptOffset.x*dFixPointScale), (int64_t)((double)ptOffset.y*dFixPointScale));
    vector<_PolyEdge> edges;
    mask.dims = 3; // wyvern: to pass the assertion in ImMat::draw_line()
    CollectPolyEdges(mask, aPolygonVertices_, edges, color.data, iLineType, ptOffset_, iFixPointShift);
    mask.dims = 2;
    if (bFilled)
    {
        FillEdgeCollection(mask, edges, color.data);
        if (iFeatherKsize > 0)
            mask = MatUtils::Blur(mask, {iFeatherKsize, iFeatherKsize});
    }
    return mask;
}

ImGui::ImMat Contour2Mask(
        const Size2i& szMaskSize, const vector<Point2f>& aPolygonVertices, const Point2f& ptOffset,
        ImDataType dtMaskDataType, double dMaskValue, double dNonMaskValue, int iLineType, bool bFilled, int iFeatherKsize)
{
    return _Contour2Mask(szMaskSize, aPolygonVertices, ptOffset, dtMaskDataType, dMaskValue, dNonMaskValue, iLineType, bFilled, iFeatherKsize);
}

ImGui::ImMat Contour2Mask(
        const Size2i& szMaskSize, const list<Point2f>& aPolygonVertices, const Point2f& ptOffset,
        ImDataType dtMaskDataType, double dMaskValue, double dNonMaskValue, int iLineType, bool bFilled, int iFeatherKsize)
{
    return _Contour2Mask(szMaskSize, aPolygonVertices, ptOffset, dtMaskDataType, dMaskValue, dNonMaskValue, iLineType, bFilled, iFeatherKsize);
}

template<template<class T, class Alloc = std::allocator<T>> class Container>
void _DrawMask(
        ImGui::ImMat& mMask, const Container<Point2f>& aPolygonVertices, const Point2f& ptOffset,
        double dMaskValue, int iLineType)
{
    if (aPolygonVertices.empty())
        return;

    ImGui::ImMat color = MakeColor(mMask.type, dMaskValue);
    int iFixPointShift = 8;
    const auto aPolygonVertices_ = ConvertFixPointPolygonVertices(aPolygonVertices, iFixPointShift);
    const double dFixPointScale = (double)(1LL << iFixPointShift);
    Point2l ptOffset_((int64_t)((double)ptOffset.x*dFixPointScale), (int64_t)((double)ptOffset.y*dFixPointScale));
    vector<_PolyEdge> edges;
    mMask.dims = 3; // wyvern: to pass the assertion in ImMat::draw_line()
    CollectPolyEdges(mMask, aPolygonVertices_, edges, color.data, iLineType, ptOffset_, iFixPointShift);
    mMask.dims = 2;
    FillEdgeCollection(mMask, edges, color.data);
}

void DrawMask(
        ImGui::ImMat& mMask, const vector<Point2f>& aPolygonVertices, const Point2f& ptOffset,
        double dMaskValue, int iLineType)
{
    _DrawMask(mMask, aPolygonVertices, ptOffset, dMaskValue, iLineType);
}

void DrawMask(
        ImGui::ImMat& mMask, const list<Point2f>& aPolygonVertices, const Point2f& ptOffset,
        double dMaskValue, int iLineType)
{
    _DrawMask(mMask, aPolygonVertices, ptOffset, dMaskValue, iLineType);
}

bool CheckTwoLinesCross(const Point2f v[4], Point2f* pCross)
{
    Point2f r1lt, r1rb;
    if (v[0].x < v[1].x)
    {
        r1lt.x = v[0].x;
        r1rb.x = v[1].x;
    }
    else
    {
        r1lt.x = v[1].x;
        r1rb.x = v[0].x;
    }
    if (v[0].y < v[1].y)
    {
        r1lt.y = v[0].y;
        r1rb.y = v[1].y;
    }
    else
    {
        r1lt.y = v[1].y;
        r1rb.y = v[0].y;
    }
    Point2f r2lt, r2rb;
    if (v[2].x < v[3].x)
    {
        r2lt.x = v[2].x;
        r2rb.x = v[3].x;
    }
    else
    {
        r2lt.x = v[3].x;
        r2rb.x = v[2].x;
    }
    if (v[2].y < v[3].y)
    {
        r2lt.y = v[2].y;
        r2rb.y = v[3].y;
    }
    else
    {
        r2lt.y = v[3].y;
        r2rb.y = v[2].y;
    }
    if (r1lt.x > r2rb.x || r1rb.x < r2lt.x || r1lt.y > r2rb.y || r1rb.y < r2lt.y)
        return false;

    const double A1 = (double)v[1].y-v[0].y;
    const double B1 = (double)v[0].x-v[1].x;
    const double C1 = (double)v[1].x*v[0].y-(double)v[0].x*v[1].y;
    const double A2 = (double)v[3].y-v[2].y;
    const double B2 = (double)v[2].x-v[3].x;
    const double C2 = (double)v[3].x*v[2].y-(double)v[2].x*v[3].y;
    const double den = A1*B2-A2*B1;
    const float cpX = (B1*C2-B2*C1)/den;
    const float cpY = (A2*C1-A1*C2)/den;
    if (cpX < r1lt.x || cpX > r1rb.x)
        return false;
    if (pCross)
    {
        pCross->x = cpX; pCross->y = cpY;
    }
    return true;
}

bool CheckTwoLinesCross(const ImVec2 v[4], ImVec2* pCross)
{
    const Point2f v_[] = { FromImVec2<float>(v[0]), FromImVec2<float>(v[1]), FromImVec2<float>(v[2]), FromImVec2<float>(v[3]) };
    Point2f poCross;
    bool res = CheckTwoLinesCross(v_, &poCross);
    if (pCross)
    {
        pCross->x = poCross.x; pCross->y = poCross.y;
    }
    return res;
}

bool CheckPointOnLine(const ImVec2& po, const ImVec2 v[2])
{
    Point2f r1lt, r1rb;
    if (v[0].x < v[1].x)
    {
        r1lt.x = v[0].x;
        r1rb.x = v[1].x;
    }
    else
    {
        r1lt.x = v[1].x;
        r1rb.x = v[0].x;
    }
    if (v[0].y < v[1].y)
    {
        r1lt.y = v[0].y;
        r1rb.y = v[1].y;
    }
    else
    {
        r1lt.y = v[1].y;
        r1rb.y = v[0].y;
    }
    if (po.x < r1lt.x || po.x > r1rb.x || po.y < r1lt.y || po.y > r1rb.y)
        return false;
    return (po.x-v[0].x)*(v[1].y-v[0].y) == (po.y-v[0].y)*(v[1].x-v[0].x);
}
}