#pragma once

#include <float.h>
#include <math.h>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159862f
#endif

// ----------------------------
// Notes from: www.github.com/cmaughan
// Ported from AntTweakBar
// This is a really nice implementation of an orientation widget; all due respect to the original author ;) 
// Dependencies kept to a minimum.  I basically vectorized the original code, added a few math types, cleaned things up and 
// made it clearer what the maths was doing.
// I tried to make it more imgui-like, and removed all the excess stuff not needed here.  This still needs work.
// I also added triangle culling because ImGui doesn't support winding clip 
// The widget works by transforming the 3D object to screen space and clipping the triangles.  This makes it work with any 
// imgui back end, without modifications to the renderers.
// 
// TODO:
// More cleanup.
// Figure out what ShowDir is for.
// Test direction vectors more
// The data structure that holds the orientation among other things
struct ImOrient
{
    ImVec4 Qt;            // Quaternion value

    ImVec3 Axis;          // Axis and Angle
    float Angle;

    ImVec3 Dir;           // Dir value set when used as a direction
    bool m_AAMode;        // Axis & angle mode
    bool m_IsDir;         // Mapped to a dir vector instead of a quat 
    ImVec3 m_ShowDir;     // CM: Not sure what this is all about? 
    ImU32 m_DirColor;        // Direction vector color

    ImMat3x3 AxisTransform;  // Transform to required axis frame

    // For the geometry
    enum EArrowParts { ARROW_CONE, ARROW_CONE_CAP, ARROW_CYL, ARROW_CYL_CAP };
    static ImVector<ImVec3> s_SphTri;
    static ImVector<ImU32> s_SphCol;
    static ImVector<ImVec2> s_SphTriProj;
    static ImVector<ImU32> s_SphColLight;
    static ImVector<ImVec3> s_ArrowTri[4];
    static ImVector<ImVec2> s_ArrowTriProj[4];
    static ImVector<ImVec3> s_ArrowNorm[4];
    static ImVector<ImU32> s_ArrowColLight[4];
    static void CreateSphere();
    static void CreateArrow();

    IMGUI_API bool Draw(const char* label);
    IMGUI_API void DrawTriangles(ImDrawList* draw_list, const ImVec2& offset, const ImVector<ImVec2>& triProj, const ImVector<ImU32>& colLight, int numVertices, float cullDir);
    IMGUI_API void ConvertToAxisAngle();
    IMGUI_API void ConvertFromAxisAngle();

    // Quaternions
    inline float QuatD(float w, float h) { return (float)std::min(std::abs(w), std::abs(h)) - 4.0f; }
    inline float QuatPX(float x, float w, float h) { return (x*0.5f*QuatD(w, h) + w*0.5f + 0.5f); }
    inline float QuatPY(float y, float w, float h) { return (-y*0.5f*QuatD(w, h) + h*0.5f - 0.5f); }
    inline float QuatIX(int x, float w, float h) { return (2.0f*x - w - 1.0f) / QuatD(w, h); }
    inline float QuatIY(int y, float w, float h) { return (-2.0f*y + h - 1.0f) / QuatD(w, h); }
    IMGUI_API void QuatFromDir(ImVec4& quat, const ImVec3& dir);
    IMGUI_API static void QuatFromAxisAngle(ImVec4& qt, const ImVec3& axis, float angle);

    // Useful colors
    IMGUI_API static ImU32 ColorBlend(ImU32 _Color1, ImU32 _Color2, float _S);

    typedef unsigned int color32;
    const ImU32 COLOR32_BLACK = 0xff000000;   // Black 
    const ImU32 COLOR32_WHITE = 0xffffffff;   // White 
    const ImU32 COLOR32_ZERO = 0x00000000;    // Zero 
    const ImU32 COLOR32_RED = 0xffff0000;     // Red 
    const ImU32 COLOR32_GREEN = 0xff00ff00;   // Green 
    const ImU32 COLOR32_BLUE = 0xff0000ff;    // Blue 

    const int GIZMO_SIZE = 200;
};

// The API
namespace ImGui
{

IMGUI_API bool QuaternionGizmo(const char* label, ImVec4& quat);
IMGUI_API bool AxisAngleGizmo(const char* label, ImVec3& axis, float& angle);
IMGUI_API bool DirectionGizmo(const char* label, ImVec3& dir);
};

