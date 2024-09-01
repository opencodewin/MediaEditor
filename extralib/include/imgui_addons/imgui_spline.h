#ifndef IMGUI_SPLINE_H
#define IMGUI_SPLINE_H
#include <vector>
#include <imgui.h>
#include <imgui_internal.h>

namespace ImGui
{
namespace ImSpline
{
    struct cSpline2
    {
        ImVec4 xb;   // x cubic bezier coefficients
        ImVec4 yb;   // y cubic bezier coefficients
    };
    struct cSubSplineT2
    {
        cSpline2 mSpline;
        float    mT0;
        float    mT1;
    };
    struct cSubSpline2
    {
        cSpline2 mSpline;
        int      mParent;
        float    mD2;
    };

    // Spline creation
    IMGUI_API cSpline2 BezierSpline    (const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3);  ///< Return Bezier spline from p0 to p3 with guide points p1, p2
    IMGUI_API cSpline2 HermiteSpline   (const ImVec2& p0, const ImVec2& p1, const ImVec2& v0, const ImVec2& v1);  ///< Return Hermite spline from p0 to p1 with corresponding tangents v0, v1.
    IMGUI_API cSpline2 CatmullRomSpline(const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3);  ///< Return Catmull-Rom spline passing through p1 and p2, with tangents affected by p0 and p3.
    IMGUI_API cSpline2 QuadrantSpline  (const ImVec2& p, float r, int quadrant);        ///< Returns a spline representing the given quadrant (quarter circle) of radius 'r' at 'p'
    IMGUI_API void     CircleSplines   (const ImVec2& p, float r, cSpline2 splines[4]); ///< Fills 'splines' with four splines representing a circle of radius 'r' at 'p'

    inline int NumSplinesForPoints(int numPoints) { if (numPoints < 2) return numPoints; return numPoints - 1; }     ///< Returns number of splines needed to represent the given number of points; generally n-1 except for n < 2.
    IMGUI_API int   SplinesFromPoints  (int numPoints, const ImVec2 p[], int maxSplines, cSpline2 splines[], float tension = 0.0f, size_t stride = sizeof(ImVec2));
    ///< Fills 'splines' with splines that interpolate the points in 'p', and returns the number of these splines.
    ///< 'tension' controls the interpolation -- the default value of 0 specifies Catmull-Rom splines that
    ///< guarantee tangent continuity. With +1 you get straight lines, and -1 gives more of a circular appearance.

    IMGUI_API int   SplinesFromBezier (int numPoints, const ImVec2 points[], const ImVec2 hullPoints[], cSpline2 splines[], bool split = false); ///< Creates splines from the given points and Bezier hull points. If 'split' is false the splines are assumed to be continous and numPoints - 1 splinea are output. Otherwise the points are assumed to come in pairs, and numPoints / 2 splines output.
    IMGUI_API int   SplinesFromHermite(int numPoints, const ImVec2 points[], const ImVec2 tangents  [], cSpline2 splines[], bool split = false); ///< Creates splines from the given points and tangents. If 'split' is false the splines are assumed to be continous and numPoints - 1 splinea are output. Otherwise the points are assumed to come in pairs, and numPoints / 2 splines output.

    // Queries
    inline ImVec2 Position0(const cSpline2& spline) { return ImVec2(spline.xb.x, spline.yb.x); }    ///< Starting position of spline
    inline ImVec2 Position1(const cSpline2& spline) { return ImVec2(spline.xb.w, spline.yb.w); }    ///< End position of spline

    inline ImVec2 Velocity0(const cSpline2& spline) { return ImVec2(3.0f * (spline.xb.y - spline.xb.x), 3.0f * (spline.yb.y - spline.yb.x)); }    ///< Starting (tangential) velocity
    inline ImVec2 Velocity1(const cSpline2& spline) { return ImVec2(3.0f * (spline.xb.w - spline.xb.z), 3.0f * (spline.yb.w - spline.yb.z)); }    ///< End velocity

    IMGUI_API ImVec2 Position    (const cSpline2& spline, float t); ///< Returns interpolated position
    IMGUI_API ImVec2 Velocity    (const cSpline2& spline, float t); ///< Returns interpolated velocity
    IMGUI_API ImVec2 Acceleration(const cSpline2& spline, float t); ///< Returns interpolated acceleration
    IMGUI_API float  Curvature    (const cSpline2& spline, float t); ///< Returns interpolated curvature. Curvature = 1 / r where r is the radius of the local turning circle, so 0 for flat.

    IMGUI_API float LengthEstimate(const cSpline2& s, float* error);              ///< Returns estimate of length of s and optionally in 'error' the maximum error of that length.
    IMGUI_API float Length        (const cSpline2& s, float maxError = 0.01f);    ///< Returns length of spline accurate to the given tolerance, using multiple LengthEstimate() calls.
    IMGUI_API float Length        (const cSpline2& s, float t0, float t1, float maxError = 0.01f);    ///< Returns length of spline segment over [t0, t1].

    IMGUI_API ImRect FastBounds (const cSpline2& spline);               ///< Returns fast, convervative bounds based off the convex hull of the spline.
    IMGUI_API ImRect ExactBounds(const cSpline2& spline);               ///< Returns exact bounds, taking into account extrema, requires solving a quadratic

    // Subdivision
    IMGUI_API void Split(const cSpline2& spline,          cSpline2* spline0, cSpline2* spline1);  ///< Splits 'spline' into two halves (at t = 0.5) and stores the results in 'subSplines'
    IMGUI_API void Split(const cSpline2& spline, float t, cSpline2* spline0, cSpline2* spline1);  ///< Fills 'subSplines' with the splines corresponding to [0, t] and [t, 1]
    IMGUI_API bool Join (const cSpline2& spline0, const cSpline2& spline1, cSpline2* spline);     ///< Joins two splines that were formerly Split(). Assumes t=0.5, returns false if the source splines don't match up.

    IMGUI_API void Split(std::vector<cSpline2>* splines);          ///< Subdivide each spline into two pieces
    IMGUI_API void Split(std::vector<cSpline2>* splines, int n);   ///< Subdivide each spline into 'n' pieces
    IMGUI_API void Join (std::vector<cSpline2>* splines);          ///< Join adjacent splines where possible

    IMGUI_API void SubdivideForLength(std::vector<cSpline2>* splines, float relativeError = 0.01f);    ///< Subdivide splines to be close to linear, according to relativeError.
    IMGUI_API void SubdivideForT     (std::vector<cSpline2>* splines, float error = 0.01f);            ///< Subdivide splines to be close to linear in t, i.e., arcLength

    // Nearest point
    IMGUI_API float FindClosestPoint(const ImVec2& p, const cSpline2& spline); ///< Returns t value of the closest point on s to 'p'
    IMGUI_API float FindClosestPoint(const ImVec2& p, int numSplines, const cSpline2 splines[], int* index); ///< Returns index of nearest spline, and 't' value of nearest point on that spline.

    IMGUI_API int   FindNearbySplines(const ImVec2& p, int numSplines, const cSpline2 splines[],       std::vector<cSubSpline2>* nearbySplines, float* smallestFarOut = 0, int maxIter = 2);
    IMGUI_API float FindClosestPoint (const ImVec2& p, int numSplines, const cSpline2 splines[], const std::vector<cSubSpline2>& nearbySplines, int* index);

    IMGUI_API int   FindSplineIntersections(const cSpline2& spline0, const cSpline2& spline1, int maxResults, float results[][2], float tolerance = 0.1f);
    ///< Returns up to 'maxResults' intersections between the two splines, accurate to the given tolerance.
    IMGUI_API int   FindSplineIntersections(int numSplines0, const cSpline2 splines0[], int numSplines1, const cSpline2 splines1[], int maxResults, int resultsI[][2], float resultsT[][2], float tolerance = 0.1f);
    ///< Returns up to 'maxResults' intersections between the two spline lists, accurate to the given tolerance.
    IMGUI_API int   FindSplineIntersections(int numSplines, const cSpline2 splines[], int maxResults, int resultsI[][2], float resultsT[][2], float tolerance = 0.1f);
    ///< Returns up to 'maxResults' self-intersections in the given spline list, accurate to the given tolerance.

    // Linear point movement along spline set
    IMGUI_API float AdvanceAgent(const cSpline2& spline, float t, float dl); ///< Advances 'agent' at 't' on the given spline, by dl (delta length), returning t' of the new location.
    IMGUI_API bool  AdvanceAgent(int* index, float* t, int numSplines, const cSpline2 splines[], float dl); ///< Version of AdvanceAgent for a set of splines, assumed to be continuous. Returns false if the agent has gone off the end, in which case call ClampAgent/WrapAgent/ReverseAgent.
    
    inline void  ClampAgent  (int* index, float* t, int numSplines)    ///< Clamps agent position back to the nearest endpoint.
    {
        if (*index < 0)
        {
            *index = 0;
            *t = 0.0f;
        }
        else if (*index >= numSplines)
        {
            *index = numSplines - 1;
            *t = 1.0f;
        } 
        else if (*t < 0.0f)
            *t = 0.0f;
        else if (*t > 1.0f)
            *t = 1.0f;
    }
    inline void  WrapAgent   (int* indexInOut, float* tInOut, int numSplines)    ///< Wraps the agent from the end back to the start, or vice versa.
    {
        int& index = *indexInOut;
        float& t = *tInOut;

        IM_ASSERT(!isnan(t));
        IM_ASSERT(index == 0 || index == numSplines - 1);

        t -= floorf(t);
        index ^= numSplines - 1;
    }
    inline void  ReverseAgent(int* index, float* t) { *t = ceilf(*t) - *t; }    ///< Reverses the agent. (You must also negate the sign of dl yourself.)

    // Misc operations
    IMGUI_API cSpline2 Reverse(const cSpline2& spline);                    ///< Reverses spline endpoints and tangents so that g(t) = f(1 - t).
    IMGUI_API cSpline2 Offset (const cSpline2& spline, float offset);      ///< Offset spline, e.g., for stroking, +ve = to the right.

    IMGUI_API void     Reverse(std::vector<cSpline2>* splines);                 ///< Reverses entire spline list
    IMGUI_API void     Offset (std::vector<cSpline2>* splines, float offset);   ///< Offset splines, e.g., for stroking, +ve = to the right.

} // namespace ImSpline
} // namespace ImGui

#if IMGUI_BUILD_EXAMPLE
namespace ImGui
{
    IMGUI_API void ShowSplineDemo();
} // namespace ImGui
#endif
#endif /* IMGUI_SPLINE_H */