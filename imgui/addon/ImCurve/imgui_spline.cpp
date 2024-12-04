#include "imgui_spline.h"
template<class T> inline int size_i(const T& container) { return int(container.size()); }
static inline float     sqr(float x) { return x * x; }
static inline float     sqrlen(ImVec2 v) { return v.x * v.x + v.y * v.y; }
static inline float     lerp(float a, float b, float t) { return (1.0f - t) * a + t * b; }
static inline float     len(ImVec2 v)           { return sqrtf(v.x * v.x + v.y * v.y); }
static inline float     dot(ImVec2 a, ImVec2 b) { return a.x * b.x + a.y * b.y; }
static inline float     dot(ImVec4 a, ImVec4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
static inline ImVec2    abs(ImVec2 v)           { return { fabsf(v.x), fabsf(v.y) }; }
static inline bool      Larger(const ImRect& bb, float t) { ImVec2 d = bb.Max - bb.Min; return d.x > t || d.y > t; }
static inline bool      Intersects(const ImRect& a, const ImRect& b) { return a.Max.x >= b.Min.x && a.Min.x <= b.Max.x && a.Max.y >= b.Min.y && a.Min.y <= b.Max.y; }
static inline void      Add(ImRect& a, const ImRect& b) { if (a.Min.x > b.Min.x) a.Min.x = b.Min.x; else if (a.Max.x < b.Max.x) a.Max.x = b.Max.x; if (a.Min.y > b.Min.y) a.Min.y = b.Min.y; else if (a.Max.y < b.Max.y) a.Max.y = b.Max.y; }

static inline float InvSqrtFast(float x)
{
    float xhalf = 0.5f * x;
    int32_t i = (int32_t&) x;
    i = 0x5f375a86 - (i >> 1);
    x = (float&) i;
    x = x * (1.5f - xhalf * x * x);
    return x;
}

namespace ImGui
{
namespace
{
    // Utilities
    inline ImVec4 BezierWeights(float t)
    /// Returns Bezier basis weights for 't'
    {
        float s  = 1.0f - t;
        
        float t2 = t * t;
        float t3 = t2 * t;
        
        float s2 = s * s;
        float s3 = s2 * s;
        
        return ImVec4(s3, 3.0f * s2 * t, 3.0f * s * t2, t3);
    }
    
    inline ImVec4 BezierWeights(const ImVec4& t)
    /// Vector version useful for derivatives
    {
        return ImVec4
        (
            t.x - 3.0f * t.y + 3.0f * t.z -         t.w,
                  3.0f * t.y - 6.0f * t.z + 3.0f *  t.w,
                               3.0f * t.z - 3.0f *  t.w,
                                                    t.w
        );
    }

    inline ImVec4 CubicCoeffs(const ImVec4& b)
    /// Returns cubic coefficients for the given Bezier weights
    {
        return ImVec4
        (
                    b.x                                ,
            -3.0f * b.x + 3.0f * b.y                   ,
             3.0f * b.x - 6.0f * b.y + 3.0f * b.z      ,
                   -b.x + 3.0f * b.y - 3.0f * b.z + b.w
        );
    }

    inline ImVec2 HullBounds(const ImVec4& s)
    /// Returns bounds of the convex hull
    {
        ImVec2 b01;

        if (s.x <= s.y)
            b01 = ImVec2(s.x, s.y);
        else
            b01 = ImVec2(s.y, s.x);

        ImVec2 b23;

        if (s.z <= s.w)
            b23 = ImVec2(s.z, s.w);
        else
            b23 = ImVec2(s.w, s.z);

        return ImVec2
        (
            ImMin(b01.x, b23.x),
            ImMax(b01.y, b23.y)
        );
    }

    ImVec2 ExactBounds(const ImVec4& spline)
    /// Returns accurate bounds taking extrema into account.
    {
        ImVec2 bounds;

        // First take endpoints into account
        if (spline.x <= spline.w)
        {
            bounds.x = spline.x;
            bounds.y = spline.w;
        }
        else
        {
            bounds.x = spline.w;
            bounds.y = spline.x;
        }

        // Now find extrema via standard quadratic equation: c.t' = 0
        ImVec4 c = CubicCoeffs(spline);

        float c33 = 3.0f * c.w;
        float cx2 = c.z * c.z - c33 * c.y;

        if (cx2 < 0.0f)
            return bounds;  // no roots!

        float invC33 = 1.0f / c33;
        float ct = -c.z * invC33;
        float cx = sqrtf(cx2) * invC33;

        float t0 = ct + cx;
        float t1 = ct - cx;

        // Must make sure the roots are within the spline interval
        if (t0 > 0.0f && t0 < 1.0f)
        {
            float x = c.x + (c.y + (c.z + c.w * t0) * t0) * t0;

            if      (bounds.x > x)
                bounds.x = x;
            else if (bounds.y < x)
                bounds.y = x;
        }


        if (t1 > 0.0f && t1 < 1.0f)
        {
            float x = c.x + (c.y + (c.z + c.w * t1) * t1) * t1;

            if      (bounds.x > x)
                bounds.x = x;
            else if (bounds.y < x)
                bounds.y = x;
        }

        return bounds;
    }

    // This is based on one step of De Casteljau's algorithm
    inline void Split(float t, const ImVec4& spline, ImVec4* spline0, ImVec4* spline1)
    {
        // assumption: seg = (P0, P1, P2, P3)
        float q0 = lerp(spline.x, spline.y, t);
        float q1 = lerp(spline.y, spline.z, t);
        float q2 = lerp(spline.z, spline.w, t);

        float r0 = lerp(q0, q1, t);
        float r1 = lerp(q1, q2, t);

        float s0 = lerp(r0, r1, t);

        float sx = spline.x;    // support aliasing
        float sw = spline.w;

        *spline0 = ImVec4(sx, q0, r0, s0);
        *spline1 = ImVec4(s0, r1, q2, sw);
    }

    // Optimised for t=0.5
    inline void Split(const ImVec4& spline, ImVec4* spline0, ImVec4* spline1)
    {
        float q0 = (spline.x + spline.y) * 0.5f;    // x + y / 2
        float q1 = (spline.y + spline.z) * 0.5f;    // y + z / 2
        float q2 = (spline.z + spline.w) * 0.5f;    // z + w / 2

        float r0 = (q0 + q1) * 0.5f;    // x + 2y + z / 4
        float r1 = (q1 + q2) * 0.5f;    // y + 2z + w / 4

        float s0 = (r0 + r1) * 0.5f;    // q0 + 2q1 + q2 / 4 = x+y + 2(y+z) + z+w / 8 = x + 3y + 3z + w

        float sx = spline.x;    // support aliasing
        float sw = spline.w;

        *spline0 = ImVec4(sx, q0, r0, s0);
        *spline1 = ImVec4(s0, r1, q2, sw);
    }

    bool Join(const ImVec4& s0, const ImVec4& s1, ImVec4* sOut)
    {    
        if (s0.w != s1.x) // early out
            return false;

        // assumes t = 0.5

        // backwards solve from left
        float x0 =     s0.x;
        float y0 = 2 * s0.y - x0;
        float z0 = 4 * s0.z - x0 - 2 * y0;
        float w0 = 8 * s0.w - x0 - 3 * (y0 + z0);

        // backwards solve from right
        float w1 =     s1.w;
        float z1 = 2 * s1.z - w1;
        float y1 = 4 * s1.y - w1 - 2 * z1;
        float x1 = 8 * s1.x - w1 - 3 * (y1 + z1);

        float dp = sqr(x0 - x1) + sqr(y0 - y1) + sqr(z0 - z1) + sqr(w0 - w1);

        if (dp < 1e-4f) // do left and right reconstructions agree?
        {
            *sOut = ImVec4(x0, y0, z1, w1);   // use most stable terms
            return true;
        }

        return false;
    }

    inline ImVec2 ArcError2(ImVec4 s)
    /// Returns squared displacement from linear (b0_b3) for hull points b1/b2
    {
        float w = s.w - s.x;

        float ty = s.x + w * 1.0f / 3.0f - s.y;
        float tz = s.x + w * 2.0f / 3.0f - s.z;
        float d2 = 1.0f / (sqr(w) + 1.0f);

        return ImVec2(sqr(ty) * d2, sqr(tz) * d2);
    }

    bool AdvanceAgent(int* indexInOut, float* tInOut, int numSplines)
    /// Update index for t if necessary, but don't run off array
    {
        int& index = *indexInOut;
        float& t = *tInOut;

        while (t < 0.0f)
        {
            if (index <= 0)
                return false;

            t += 1.0f;
            index--;
        }

        while (t > 1.0f)
        {
            if (index >= numSplines - 1)
                return false;

            t -= 1.0f;
            index++;
        }

        return true;
    }
} // namespace

namespace
{
    const float kCircleOffset = 4.0f / 3.0f * (sqrtf(2.0f) - 1.0f);
    const ImVec4 kQuarterB0(1.0f, 1.0f, kCircleOffset, 0.0f);
    const ImVec4 kQuarterB1(0.0f, kCircleOffset, 1.0f, 1.0f);
} // namespace

////////////////////////////////////////////////////////////////////////////////
// 2D
////////////////////////////////////////////////////////////////////////////////
ImSpline::cSpline2 ImSpline::BezierSpline(const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3)
{
    return cSpline2
    {
        ImVec4(p0.x, p1.x, p2.x, p3.x),
        ImVec4(p0.y, p1.y, p2.y, p3.y),
    };
}

ImSpline::cSpline2 ImSpline::HermiteSpline(const ImVec2& p0, const ImVec2& p1, const ImVec2& v0, const ImVec2& v1)
{
    ImVec2 pb1 = p0 + v0 * (1.0f / 3.0f);
    ImVec2 pb2 = p1 - v1 * (1.0f / 3.0f);

    return BezierSpline(p0, pb1, pb2, p1);
}

ImSpline::cSpline2 ImSpline::CatmullRomSpline(const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3)
{
    ImVec2 pb1 = p1 + (p2 - p0) * (1.0f / 6.0f);
    ImVec2 pb2 = p2 - (p3 - p1) * (1.0f / 6.0f);

    return BezierSpline(p1, pb1, pb2, p2);
}

ImSpline::cSpline2 ImSpline::QuadrantSpline(const ImVec2& p, float r, int quadrant)
{
    IM_ASSERT(quadrant >= 0 && quadrant < 4);
    cSpline2 s;

    switch (quadrant)
    {
    case 0:
        s.xb =  kQuarterB0;
        s.yb =  kQuarterB1;
        break;
    case 1:
        s.xb = -kQuarterB1;
        s.yb =  kQuarterB0;
        break;
    case 2:
        s.xb = -kQuarterB0;
        s.yb = -kQuarterB1;
        break;
    case 3:
        s.xb =  kQuarterB1;
        s.yb = -kQuarterB0;
        break;
    }

    s.xb = s.xb * r + ImVec4(p.x, p.x, p.x, p.x);
    s.yb = s.yb * r + ImVec4(p.y, p.y, p.y, p.y);

    return s;
}

void ImSpline::CircleSplines(const ImVec2& p, float r, cSpline2 splines[4])
{
    for (int i = 0; i < 4; i++)
        splines[i] = QuadrantSpline(p, r, i);
}

namespace
{
    inline ImSpline::cSpline2 SplineFromPoints2(const char* p8, size_t stride, int i0, int i1, int i2, int i3, float tension)
    {
        ImVec2 p0 = *(ImVec2*) (p8 + i0 * stride);
        ImVec2 p1 = *(ImVec2*) (p8 + i1 * stride);
        ImVec2 p2 = *(ImVec2*) (p8 + i2 * stride);
        ImVec2 p3 = *(ImVec2*) (p8 + i3 * stride);

        float s = (1.0f - tension) * (1.0f / 6.0f);

        ImVec2 pb1 = p1 + (p2 - p0) * s;
        ImVec2 pb2 = p2 - (p3 - p1) * s;

        return ImSpline::BezierSpline(p1, pb1, pb2, p2);
    }
    inline ImVec2 Evaluate(const ImSpline::cSpline2& spline, const ImVec4& w)
    /// Evaluate spline with given weights
    {
        return ImVec2
        (
            dot(spline.xb, w),
            dot(spline.yb, w)
        );
    }
} // namespace

int ImSpline::SplinesFromPoints(int numPoints, const ImVec2 pi[], int maxSplines, cSpline2 splines[], float tension, size_t stride)
{
    IM_ASSERT(numPoints >= 0);
    IM_ASSERT(maxSplines >= 0 && maxSplines >= NumSplinesForPoints(numPoints));

    const char* p8 = (const char*) pi;

    switch (numPoints)
    {
    case 0:
        return 0;
    case 1:
        *splines = SplineFromPoints2(p8, stride, 0, 0, 0, 0, tension);
        return 1;
    case 2:
        *splines = SplineFromPoints2(p8, stride, 0, 0, 1, 1, tension);
        return 1;
    }

    *splines++ = SplineFromPoints2(p8, stride, 0, 0, 1, 2, tension);

    for (int i = 0; i < numPoints - 3; i++)
    {
        *splines++ = SplineFromPoints2(p8, stride, 0, 1, 2, 3, tension);
        p8 += stride;
    }

    *splines++ = SplineFromPoints2(p8, stride, 0, 1, 2, 2, tension);

    return numPoints - 1;
}

int ImSpline::SplinesFromBezier(int numPoints, const ImVec2 points[], const ImVec2 hullPoints[], cSpline2 splines[], bool split)
{
    int numSplines = split ? numPoints / 2 : numPoints - 1;
    int advance    = split ? 2 : 1;

    for (int i = 0; i < numSplines; i++)
    {
        splines[i] = BezierSpline(points[0], hullPoints[0], hullPoints[1], points[1]);
        points     += advance;
        hullPoints += advance;
    }

    return numSplines;
}

int ImSpline::SplinesFromHermite(int numPoints, const ImVec2 points[], const ImVec2 tangents  [], cSpline2 splines[], bool split)
{
    int numSplines = split ? numPoints / 2 : numPoints - 1;
    int advance    = split ? 2 : 1;

    for (int i = 0; i < numSplines; i++)
    {
        splines[i] = HermiteSpline(points[0], points[1], tangents[0], tangents[1]);
        points   += advance;
        tangents += advance;
    }

    return numSplines;
}

ImVec2 ImSpline::Position(const cSpline2& spline, float t)
{
    return Evaluate(spline, BezierWeights(t));
}

ImVec2 ImSpline::Velocity(const cSpline2& spline, float t)
{
    ImVec4 dt4(0, 1, 2 * t, 3 * t * t);
    return Evaluate(spline, BezierWeights(dt4));
}

ImVec2 ImSpline::Acceleration(const cSpline2& spline, float t)
{
    ImVec4 ddt4(0, 0, 2, 6 * t);
    return Evaluate(spline, BezierWeights(ddt4));
}

float ImSpline::Curvature(const cSpline2& spline, float t)
{
    ImVec2 v = Velocity    (spline, t);
    ImVec2 a = Acceleration(spline, t);

    float avCrossLen = fabsf(v.x * a.y - v.y * a.x);
    float vLen = len(v);

    if (vLen == 0.0f)
        return 1e10f;

    return avCrossLen / (vLen * vLen * vLen);
}

float ImSpline::LengthEstimate(const cSpline2& s, float* error)
{
    // Our convex hull is p0, p1, p2, p3, so p0_p3 is our minimum possible length, and p0_p1 + p1_p2 + p2_p3 our maximum.
    float d03 = sqr(s.xb.x - s.xb.w) + sqr(s.yb.x - s.yb.w);

    float d01 = sqr(s.xb.x - s.xb.y) + sqr(s.yb.x - s.yb.y);
    float d12 = sqr(s.xb.y - s.xb.z) + sqr(s.yb.y - s.yb.z);
    float d23 = sqr(s.xb.z - s.xb.w) + sqr(s.yb.z - s.yb.w);

    float minLength = sqrtf(d03);
    float maxLength = sqrtf(d01) + sqrtf(d12) + sqrtf(d23);

    minLength *= 0.5f;
    maxLength *= 0.5f;

    *error = maxLength - minLength;
    return minLength + maxLength;
}

float ImSpline::Length(const cSpline2& s, float maxError)
{
    float error;
    float length = LengthEstimate(s, &error);

    if (error > maxError)
    {
        cSpline2 s0;
        cSpline2 s1;

        Split(s, &s0, &s1);

        return Length(s0, maxError) + Length(s1, maxError);
    }

    return length;
}

float ImSpline::Length(const cSpline2& s, float t0, float t1, float maxError)
{
    IM_ASSERT(t0 >= 0.0f && t0 <  1.0f);
    IM_ASSERT(t1 >= 0.0f && t1 <= 1.0f);
    IM_ASSERT(t0 <= t1);

    cSpline2 s0, s1;

    if (t0 == 0.0f)
    {
        if (t1 == 1.0f)
            return Length(s, maxError);

        Split(s, t1, &s0, &s1);
        return Length(s0, maxError);
    }
    else
    {
        Split(s, t0, &s0, &s1);

        if (t1 == 1.0f)
            return Length(s1, maxError);

        Split(s1, (t1 - t0) / (1.0f - t0), &s0, &s1);
        return Length(s0, maxError);
    }
}

ImRect ImSpline::FastBounds(const cSpline2& spline)
{
    ImVec2 bx = HullBounds(spline.xb);
    ImVec2 by = HullBounds(spline.yb);

    ImRect result = { { bx.x, by.x }, { bx.y, by.y } };
    return result;
}

ImRect ImSpline::ExactBounds(const cSpline2& spline)
{
    ImVec2 bx = ImGui::ExactBounds(spline.xb);
    ImVec2 by = ImGui::ExactBounds(spline.yb);

    ImRect result = { { bx.x, by.x }, { bx.y, by.y } };
    return result;
}

void ImSpline::Split(const cSpline2& spline, cSpline2* spline0, cSpline2* spline1)
{
    ImGui::Split(spline.xb, &spline0->xb, &spline1->xb);
    ImGui::Split(spline.yb, &spline0->yb, &spline1->yb);
}

void ImSpline::Split(const cSpline2& spline, float t, cSpline2* spline0, cSpline2* spline1)
{
    ImGui::Split(t, spline.xb, &spline0->xb, &spline1->xb);
    ImGui::Split(t, spline.yb, &spline0->yb, &spline1->yb);
}

bool ImSpline::Join(const cSpline2& s0, const cSpline2& s1, cSpline2* splineOut)
{
    return
        ImGui::Join(s0.xb, s1.xb, &splineOut->xb)
    &&  ImGui::Join(s0.yb, s1.yb, &splineOut->yb);
}

void ImSpline::Split(std::vector<cSpline2>* splinesIn)
{
    std::vector<cSpline2> splines;

    for (const cSpline2& s : *splinesIn)
    {
        cSpline2 s0, s1;

        Split(s, &s0, &s1);
        splines.push_back(s0);
        splines.push_back(s1);
    }

    splinesIn->swap(splines);
}

void ImSpline::Split(std::vector<cSpline2>* splinesIn, int n)
{
    std::vector<cSpline2> splines;

    for (const cSpline2& s : *splinesIn)
    {
        cSpline2 ss(s);
        cSpline2 s0, s1;

        for (int i = n; i > 1; i--)
        {
            Split(ss, 1.0f / i, &s0, &ss);
            splines.push_back(s0);
        }
        splines.push_back(ss);
    }

    splinesIn->swap(splines);
}

void ImSpline::Join(std::vector<cSpline2>* splinesIn)
{
    std::vector<cSpline2> splines;
    const cSpline2* prevS = 0;

    for (const cSpline2& s : *splinesIn)
    {
        if (!prevS)
        {
            prevS = &s;
            continue;
        }

        cSpline2 sj;
        if (Join(*prevS, s, &sj))
            splines.push_back(sj);
        else
        {
            splines.push_back(*prevS);
            splines.push_back(s);
        }

        prevS = 0;
    }

    if (prevS)
        splines.push_back(*prevS);

    splinesIn->swap(splines);
}

namespace
{
    void SubdivideForLength(const ImSpline::cSpline2& s, std::vector<ImSpline::cSpline2>* splines, float tolerance)
    {
        float error;
        float length = LengthEstimate(s, &error);

        if (error <= tolerance * length)
            splines->push_back(s);
        else
        {
            ImSpline::cSpline2 s1, s2;
            Split(s, &s1, &s2);

            SubdivideForLength(s1, splines, tolerance);
            SubdivideForLength(s2, splines, tolerance);
        }
    }
} // namespace

void ImSpline::SubdivideForLength(std::vector<cSpline2>* splinesIn, float tolerance)
{
    std::vector<cSpline2> splines;

    for (const cSpline2& s : *splinesIn)
        ImGui::SubdivideForLength(s, &splines, tolerance);

    splinesIn->swap(splines);
}

namespace
{
    inline float ArcError(const ImSpline::cSpline2& s, float* tSplit)
    {
        ImVec2 ex = ArcError2(s.xb);
        ImVec2 ey = ArcError2(s.yb);

        float e0 = ex.x + ey.x;
        float e1 = ex.y + ey.y;
        float es2 = e0 + e1;

        float f = (es2 < 1e-6f) ? 0.5f : sqrtf(e0 / es2);

        *tSplit = (1.0f / 3.0f) * (1.0f + f);

        return sqrtf(es2);
    }

    void SubdivideForT(const ImSpline::cSpline2& s, std::vector<ImSpline::cSpline2>* splines, float tolerance)
    {
        float splitT;
        float err = ArcError(s, &splitT);

        if (err <= tolerance)
            splines->push_back(s);
        else
        {
            ImSpline::cSpline2 s1, s2;
            Split(s, splitT, &s1, &s2);

            SubdivideForT(s1, splines, tolerance);
            SubdivideForT(s2, splines, tolerance);
        }
    }
} // namespace

void ImSpline::SubdivideForT(std::vector<cSpline2>* splinesIn, float tolerance)
{
    std::vector<cSpline2> splines;

    for (const cSpline2& s : *splinesIn)
        ImGui::SubdivideForT(s, &splines, tolerance);

    splinesIn->swap(splines);
}

namespace
{
    inline float ClosestPoint(const ImVec2& p, const ImVec2& p0, const ImVec2& p1)
    {
        ImVec2 w = p1 - p0;
        ImVec2 v =  p - p0;

        float dvw = dot(v, w);

        if (dvw <= 0.0f)
            return 0.0f;

        float dww = dot(w, w);

        if (dvw >= dww)
            return 1.0f;

        return dvw / dww;
    }

    void FindClosestPointNewtonRaphson(const ImSpline::cSpline2& spline, ImVec2 p, float sIn, int maxIterations, float* tOut, float* dOut)
    {
        IM_ASSERT(sIn >= 0.0f && sIn <= 1.0f);
        const float maxS = 1.0f - 1e-6f;

        float skLast = sIn;
        float sk = sIn;

        float dk = len(Position(spline, sk) - p);

        constexpr float width = 1e-3f;

        float maxJump  = 0.5f;   // avoid jumping too far, leads to oscillation

        for (int i = 0; i < maxIterations; i++)
        {
            float ss = ImClamp(sk, width, 1.0f - width); // so can interpolate points for Newtons method

            float d1 = len(Position(spline, ss - width) - p);
            float d2 = len(Position(spline, ss        ) - p);
            float d3 = len(Position(spline, ss + width) - p);

            float g1 = (d2 - d1) / width;
            float g2 = (d3 - d2) / width;

            float grad = (d3 - d1) / (2.0f * width);
            float curv = (g2 - g1) / width;

            float sn;   // next candidate

            if (curv > 0.0f)    // if d' is heading towards a minima, apply NR for d'
                sn = ss - grad / curv;
            else if (grad != 0.0f)
                sn = ss - d2 / grad; // otherwise, apply for D.
            else
                sn = sk;

            sn = ImClamp(sn, sk - maxJump, sk + maxJump);   // avoid large steps, often unstable.

            // only update our estimate if the new value is in range and closer.
            if (sn >= 0.0f && sn < maxS)
            {
                float dn = len(Position(spline, sn) - p);

                if (dn < dk)    // only update sk if d actually gets smaller
                {
                    sk = sn;
                    dk = dn;
                }
            }

            maxJump *= 0.5f;    // reduce on a schedule -- helps binary search back towards a jump that is valid.

            skLast = sk;
        }

        (*tOut) = sk;
        (*dOut) = dk;
    }
} // namespace

float ImSpline::FindClosestPoint(const ImVec2& p, const cSpline2& spline)
{
    // Approximate s from straight line between the start and end.
    float s = ClosestPoint(p, Position0(spline), Position1(spline));

    // Use this as starting point for Newton-Raphson solve.
    float d;
    FindClosestPointNewtonRaphson(spline, p, s, 8, &s, &d);

    return s;
}

float ImSpline::FindClosestPoint(const ImVec2& p, int numSplines, const cSpline2 splines[], int* index)
{
    std::vector<cSubSpline2> nearbyInfo;

    FindNearbySplines(p, numSplines, splines, &nearbyInfo);
    return FindClosestPoint(p, numSplines, splines, nearbyInfo, index);
}

namespace
{
    void FindMinMaxDistance2s(const ImVec2& p, const ImRect& bbox, float* minD2, float* maxD2)
    {
        const ImVec2& p0 = bbox.Min;
        const ImVec2& p1 = bbox.Max;

        // Find the nearest point to p inside the bbox
        // This can be a bbox vertex, a point on an edge or face, or p itself if it's inside the box
        float minX = ImClamp(p.x, p0.x, p1.x);
        float minY = ImClamp(p.y, p0.y, p1.y);

        // Find the farthest point from p inside the bbox
        // This is always a bbox vertex.
        ImVec2 d0(abs(p - p0));
        ImVec2 d1(abs(p - p1));

        float maxX = d0.x > d1.x ? p0.x : p1.x; // select the coordinate we're farthest from
        float maxY = d0.y > d1.y ? p0.y : p1.y;

        // return len2
        *minD2 = sqr(p.x - minX) + sqr(p.y - minY);
        *maxD2 = sqr(p.x - maxX) + sqr(p.y - maxY);
    }

    void FindMinMaxDistance2s(const ImVec2& p, const ImSpline::cSpline2& spline, float* minD2, float* maxD2)
    {
        ImRect bbox = FastBounds(spline);

        FindMinMaxDistance2s(p, bbox, minD2, maxD2);
    }

    void Split(const ImSpline::cSubSpline2& s, ImSpline::cSubSpline2* s0, ImSpline::cSubSpline2* s1)
    {
        ImGui::Split(s.mSpline.xb, &s0->mSpline.xb, &s1->mSpline.xb);
        ImGui::Split(s.mSpline.yb, &s0->mSpline.yb, &s1->mSpline.yb);

        s0->mParent = s.mParent;
        s1->mParent = s.mParent;
    }
} // namespace

int ImSpline::FindNearbySplines(const ImVec2& p, int numSplines, const cSpline2 splines[], std::vector<cSubSpline2>* results, float* smallestFarOut, int numIter)
{
    std::vector<cSubSpline2>& nearSplines = *results;

    nearSplines.clear();

    float smallestFar  = FLT_MAX;
    float smallestNear = FLT_MAX;

    // Find initial list

    int maxSize = 0;

    for (int i = 0; i < numSplines; i++)
    {
        float near;
        float far;
        FindMinMaxDistance2s(p, splines[i], &near, &far);

        if (near < smallestFar)
        {
            // we at least overlap the current set.
            if (near < smallestNear)
                smallestNear = near;

            if (far < smallestFar)
            {
                // we bring in the 'best' far distance
                smallestFar = far;

                // compact list to reject any segments that now cannot be closest.
                int dj = 0;

                for (int j = 0, nj = size_i(nearSplines); j < nj; j++)
                {
                    if (nearSplines[j].mD2 < smallestFar)
                    {
                        if (dj < j)
                            nearSplines[dj] = nearSplines[j];

                        dj++;
                    }
                }

                nearSplines.resize(dj);
            }

            cSubSpline2 ss = { splines[i], i, near };
            nearSplines.push_back(ss);

            if (maxSize < size_i(nearSplines))
                maxSize = size_i(nearSplines);
        }
    }

    // Subdivide + refine

    int numNearSplines = size_i(nearSplines);

    for (int i = 0; i < numIter; i++)
    {
        int numNearSplines2 = numNearSplines * 2;

        nearSplines.resize(numNearSplines2);

        for (int i = numNearSplines - 1; i >= 0; i--)
            ImGui::Split(nearSplines[i], &nearSplines[2 * i], &nearSplines[2 * i + 1]);

        smallestNear = FLT_MAX; // this may actually increase on subdivision.

        for (int i = 0; i < numNearSplines2; i++)
        {
            float near;
            float far;
            FindMinMaxDistance2s(p, nearSplines[i].mSpline, &near, &far);

            if (far < smallestFar)
                smallestFar = far;
            if (near < smallestNear)
                smallestNear = near;

            nearSplines[i].mD2 = near;
        }

        int di = 0;
        for (int i = 0; i < numNearSplines2; i++)
        {
            if (nearSplines[i].mD2 < smallestFar)
            {
                if (di < i)
                    nearSplines  [di] = nearSplines  [i];

                di++;
            }
        }

        nearSplines.resize(di);
        numNearSplines = di;
    }

    if (smallestFarOut)
        *smallestFarOut = smallestFar;

    return numNearSplines;
}

float ImSpline::FindClosestPoint(const ImVec2& p, int numSplines, const cSpline2 splines[], const std::vector<cSubSpline2>& nearbySplines, int* index)
{
    int prevParent = -1;
    float minD = FLT_MAX;
    float minT = 0.0f;

    *index = -1;

    for (const cSubSpline2& subSpline : nearbySplines)
    {
        if (subSpline.mParent != prevParent)
        {
            IM_ASSERT(subSpline.mParent >= 0 && subSpline.mParent < numSplines);

            const cSpline2& spline = splines[subSpline.mParent];

            float t = ClosestPoint(p, Position0(spline), Position1(spline));
            float d;

            FindClosestPointNewtonRaphson(spline, p, t, 8, &t, &d);

            if (minD > d)
            {
                minD = d;
                *index = subSpline.mParent;
                minT = t;
            }
        }
    }

    return minT;
}

namespace
{
    inline void Split(const ImSpline::cSubSplineT2& s, ImSpline::cSubSplineT2* s0, ImSpline::cSubSplineT2* s1)
    {
        ImGui::Split(s.mSpline.xb, &s0->mSpline.xb, &s1->mSpline.xb);
        ImGui::Split(s.mSpline.yb, &s0->mSpline.yb, &s1->mSpline.yb);

        float midT = 0.5f * (s.mT0 + s.mT1);

        s0->mT0     = s.mT0;
        s0->mT1     = midT;

        s1->mT0     = midT;
        s1->mT1     = s.mT1;
    }
    int FindSubSplineIntersections
    (
        const ImSpline::cSubSplineT2& spline0,
        const ImSpline::cSubSplineT2& spline1,
        int   dest,
        int   maxDest,
        float results[][2],
        float tolerance
    )
    {
        IM_ASSERT(dest < maxDest);

        ImRect bbox0 = ExactBounds(spline0.mSpline);
        ImRect bbox1 = ExactBounds(spline1.mSpline);

        if (!Intersects(bbox0, bbox1))
            return dest;

        if (Larger(bbox0, tolerance))
        {
            ImSpline::cSubSplineT2 spline00, spline01;
            Split(spline0, &spline00, &spline01);

            if (Larger(bbox1, tolerance))
            {
                ImSpline::cSubSplineT2 spline10, spline11;
                Split(spline1, &spline10, &spline11);

                dest     = FindSubSplineIntersections(spline00, spline10, dest, maxDest, results, tolerance);
                if (dest < maxDest)
                    dest = FindSubSplineIntersections(spline01, spline10, dest, maxDest, results, tolerance);
                if (dest < maxDest)
                    dest = FindSubSplineIntersections(spline00, spline11, dest, maxDest, results, tolerance);
                if (dest < maxDest)
                    dest = FindSubSplineIntersections(spline01, spline11, dest, maxDest, results, tolerance);
            }
            else
            {
                dest     = FindSubSplineIntersections(spline00, spline1, dest, maxDest, results, tolerance);
                if (dest < maxDest)
                    dest = FindSubSplineIntersections(spline01, spline1, dest, maxDest, results, tolerance);
            }

            return dest;
        }

        if (Larger(bbox1, tolerance))
        {
            ImSpline::cSubSplineT2 spline10, spline11;
            Split(spline1, &spline10, &spline11);

            dest     = FindSubSplineIntersections(spline0, spline10, dest, maxDest, results, tolerance);
            if (dest < maxDest)
                dest = FindSubSplineIntersections(spline0, spline11, dest, maxDest, results, tolerance);

            return dest;
        }

        float t0 = 0.5f * (spline0.mT0 + spline0.mT1);
        float t1 = 0.5f * (spline1.mT0 + spline1.mT1);

        // debounce
        for (int i = 0; i < dest; i++)
            if (fabsf(results[i][0] - t0) < 1e-2f
                && fabsf(results[i][1] - t1) < 1e-2f)
            {
                return dest;
            }

        results[dest][0] = t0;
        results[dest][1] = t1;
        return dest + 1;
    }
} // namespace

int ImSpline::FindSplineIntersections(const cSpline2& spline0, const cSpline2& spline1, int maxResults, float results[][2], float tolerance)
{
    if (maxResults <= 0)
        return 0;

    cSubSplineT2 subSpline0 = { spline0, 0.0f, 1.0f };
    cSubSplineT2 subSpline1 = { spline1, 0.0f, 1.0f };

    return FindSubSplineIntersections(subSpline0, subSpline1, 0, maxResults, results, tolerance);
}

namespace
{
    int FindSplineIntersections
    (
        int   is0, int numSplines0, const ImSpline::cSpline2 splines0[],
        int   is1, int numSplines1, const ImSpline::cSpline2 splines1[],
        int   maxResults,
        int   resultsI[][2],
        float resultsT[][2],
        float tolerance
    )
    {
        if (maxResults <= 0 || numSplines0 == 0 || numSplines1 == 0)
            return 0;

        int count = 0;

        // once the lists are small enough, brute-force the remainder, as recalculating the bounds is not free
        if (numSplines0 >= 4 || numSplines1 >= 4)
        {
            // Terminate if the lists don't overlap
            ImRect b0 = FastBounds(splines0[0]);
            ImRect b1 = FastBounds(splines1[0]);

            for (int i = 1; i < numSplines0; i++)
                Add(b0, FastBounds(splines0[i]));
            for (int i = 1; i < numSplines1; i++)
                Add(b1, FastBounds(splines1[i]));

            if (!Intersects(b0, b1))
                return 0;

            // Divide each spline list into two, and recurse
            int n00 = numSplines0 / 2;
            int n01 = numSplines0 - n00;
            int n10 = numSplines1 / 2;
            int n11 = numSplines1 - n10;

            count += FindSplineIntersections(is0 +   0, n00, splines0 +   0, is1 +   0, n10, splines1 +   0, maxResults - count, resultsI + count, resultsT + count, tolerance);
            count += FindSplineIntersections(is0 +   0, n00, splines0 +   0, is1 + n10, n11, splines1 + n10, maxResults - count, resultsI + count, resultsT + count, tolerance);
            count += FindSplineIntersections(is0 + n00, n01, splines0 + n00, is1 +   0, n10, splines1 +   0, maxResults - count, resultsI + count, resultsT + count, tolerance);
            count += FindSplineIntersections(is0 + n00, n01, splines0 + n00, is1 + n10, n11, splines1 + n10, maxResults - count, resultsI + count, resultsT + count, tolerance);

            return count;
        }

        ImSpline::cSubSplineT2 st0, st1;

        st0.mT0 = 0.0f;
        st0.mT1 = 1.0f;
        st1.mT0 = 0.0f;
        st1.mT1 = 1.0f;

        for (int i0 = 0; i0 < numSplines0; i0++)
        {
            for (int i1 = 0; i1 < numSplines1; i1++)
            {
                st0.mSpline = splines0[i0];
                st1.mSpline = splines1[i1];

                int numIntersections = FindSubSplineIntersections(st0, st1, 0, maxResults - count, resultsT + count, tolerance);

                for (int k = 0; k < numIntersections; k++)
                {
                    resultsI[k + count][0] = is0 + i0;
                    resultsI[k + count][1] = is1 + i1;
                }

                count += numIntersections;
                IM_ASSERT(count <= maxResults);

                if (count == maxResults)
                    return count;
            }
        }
        return count;
    }
} // namespace

int ImSpline::FindSplineIntersections
(
    int   numSplines0, const cSpline2 splines0[],
    int   numSplines1, const cSpline2 splines1[],
    int   maxResults,
    int   resultsI[][2],
    float resultsT[][2],
    float tolerance
)
{
    return ImGui::FindSplineIntersections(0, numSplines0, splines0, 0, numSplines1, splines1, maxResults, resultsI, resultsT, tolerance);
}

int ImSpline::FindSplineIntersections
(
    int   numSplines, const cSpline2 splines[],
    int   maxResults,
    int   resultsI[][2],
    float resultsT[][2],
    float tolerance
)
// Find self-intersections
{
    if (maxResults <= 0 || numSplines == 0)
        return 0;

    int count = 0;

    if (numSplines >= 8)
    {
        const int n0 = numSplines / 2;
        const int n1 = numSplines - n0;

        // Find intersections between the two halves
        count += ImGui::FindSplineIntersections(0, n0, splines, 0, n1, splines + n0, maxResults - count, resultsI + count, resultsT + count, tolerance);

        for (int i = 0; i < count; i++)
        {
            // ignore spurious intersections between endpoint of first and start of second
            if (resultsI[i][1] == 0 && resultsI[i][0] == n0 - 1)
                if (resultsT[i][0] > 0.95f && resultsT[i][1] < 0.05f)
                {
                    resultsT[i][0] = resultsT[count - 1][0];
                    resultsT[i][1] = resultsT[count - 1][1];
                    resultsI[i][0] = resultsI[count - 1][0];
                    resultsI[i][1] = resultsI[count - 1][1];

                    i--;
                    count--;
                    continue;
                }

            resultsI[i][1] += n0;           // adjust second half indices
        }

        // Find self-intersections in the first half
        count += FindSplineIntersections(n0, splines +  0, maxResults - count, resultsI + count, resultsT + count, tolerance);

        // Find self-intersections in the second half
        int prevCount = count;
        count += FindSplineIntersections(n1, splines + n0, maxResults - count, resultsI + count, resultsT + count, tolerance);

        for (int i = prevCount; i < count; i++)
        {
            resultsI[i][0] += n0;   // adjust second half indices
            resultsI[i][1] += n0;
        }

        return count;
    }

    cSubSplineT2 st0, st1;

    st0.mT0 = 0.0f;
    st0.mT1 = 1.0f;
    st1.mT0 = 0.0f;
    st1.mT1 = 1.0f;

    for (int i0 = 0; i0 < numSplines; i0++)
    {
        for (int i1 = i0 + 1; i1 < numSplines; i1++)
        {
            st0.mSpline = splines[i0];
            st1.mSpline = splines[i1];

            int numIntersections = FindSubSplineIntersections(st0, st1, 0, maxResults, resultsT, tolerance);

            if (i1 == i0 + 1)   // ignore spurious intersections between endpoint of i0 and start of i1
            {
                for (int k = 0; k < numIntersections; k++)
                {
                    if (resultsT[k][0] > 0.95f && resultsT[k][1] < 0.05f)
                    {
                        resultsT[k][0] = resultsT[numIntersections - 1][0];
                        resultsT[k][1] = resultsT[numIntersections - 1][1];

                        k--;
                        numIntersections--;
                    }
                }
            }

            for (int k = 0; k < numIntersections; k++)
            {
                resultsI[k][0] = i0;
                resultsI[k][1] = i1;
            }

            count += numIntersections;
            maxResults -= numIntersections;

            if (maxResults <= 0)
                return count;

            resultsT += numIntersections;
            resultsI += numIntersections;
        }
    }
    return count;
}

float ImSpline::AdvanceAgent(const cSpline2& spline, float t, float ds)
{
    ImVec2 v  = Velocity(spline, t);
    float v2 = sqrlen(v);
    float dt = ds;

    if (v2 > 0.01f)
        dt *= InvSqrtFast(v2);
    else
        dt *= 10.0f;

    return t + dt;
}

bool ImSpline::AdvanceAgent(int* index, float* t, int numSplines, const cSpline2 splines[], float ds)
{
    *t = AdvanceAgent(splines[*index], *t, ds);

    return ImGui::AdvanceAgent(index, t, numSplines);
}

ImSpline::cSpline2 ImSpline::Reverse(const cSpline2& spline)
{
    return
    {
        ImVec4(spline.xb.w, spline.xb.z, spline.xb.y, spline.xb.x),
        ImVec4(spline.yb.w, spline.yb.z, spline.yb.y, spline.yb.x)
    };
}

void ImSpline::Reverse(std::vector<cSpline2>* splines)
{
    int n = int(splines->size());
    int h = n / 2;

    for (int i = 0; i < h; i++)
    {
        cSpline2& s0 = (*splines)[i];
        cSpline2& s1 = (*splines)[n - i - 1];

        cSpline2 sr0 = Reverse(s1);
        cSpline2 sr1 = Reverse(s0);

        s0 = sr0;
        s1 = sr1;
    }

    if (2 * h < n)
        (*splines)[h] = Reverse((*splines)[h]);
}

ImSpline::cSpline2 ImSpline::Offset(const cSpline2& spline, float offset)
{
    float sx0 = spline.xb.y - spline.xb.x;
    float sy0 = spline.yb.y - spline.yb.x;
    float sd0 = InvSqrtFast(sx0 * sx0 + sy0 * sy0) * offset;

    float sx1 = spline.xb.z - spline.xb.x;
    float sy1 = spline.yb.z - spline.yb.x;
    float sd1 = InvSqrtFast(sx1 * sx1 + sy1 * sy1) * offset;

    float sx2 = spline.xb.w - spline.xb.y;
    float sy2 = spline.yb.w - spline.yb.y;
    float sd2 = InvSqrtFast(sx2 * sx2 + sy2 * sy2) * offset;

    float sx3 = spline.xb.w - spline.xb.z;
    float sy3 = spline.yb.w - spline.yb.z;
    float sd3 = InvSqrtFast(sx3 * sx3 + sy3 * sy3) * offset;

    return
    {
        spline.xb + ImVec4(sy0 * sd0, sy1 * sd1, sy2 * sd2, sy3 * sd3),
        spline.yb - ImVec4(sx0 * sd0, sx1 * sd1, sx2 * sd2, sx3 * sd3)
    };
}

void ImSpline::Offset(std::vector<cSpline2>* splines, float offset)
{
    for (cSpline2& s : *splines)
        s = Offset(s, offset);
}

} // namespace ImGui
