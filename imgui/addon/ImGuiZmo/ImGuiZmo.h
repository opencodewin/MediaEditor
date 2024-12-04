#pragma once
#ifndef IMGUIZMO_NAMESPACE
#define IMGUIZMO_NAMESPACE ImGuizmo
#endif
#include <imgui.h>
#include "model.h"

struct Model_Triangle;
struct Model;
namespace IMGUIZMO_NAMESPACE
{
    // call inside your own window and before Manipulate() in order to draw gizmo to that window.
    // Or pass a specific ImDrawList to draw to (e.g. ImGui::GetForegroundDrawList()).
    IMGUI_API void SetDrawlist(ImDrawList *drawlist = nullptr);

    // this is necessary because when imguizmo is compiled into a dll, and imgui into another
    // globals are not shared between them.
    // More details at https://stackoverflow.com/questions/19373061/what-happens-to-global-and-static-variables-in-a-shared-library-when-it-is-dynam
    // expose method to set imgui context
    IMGUI_API void SetImGuiContext(ImGuiContext *ctx);

    // return true if mouse cursor is over any gizmo control (axis, plan or screen component)
    IMGUI_API bool IsOver();

    // return true if mouse IsOver or if the gizmo is in moving state
    IMGUI_API bool IsUsing();

    // return true if any gizmo is in moving state
    IMGUI_API bool IsUsingAny();

    // enable/disable the gizmo. Stay in the state until next call to Enable.
    // gizmo is rendered with gray half transparent color when disabled
    IMGUI_API void Enable(bool enable);

    // helper functions for manualy editing translation/rotation/scale with an input float
    // translation, rotation and scale float points to 3 floats each
    // Angles are in degrees (more suitable for human editing)
    // example:
    // ImVec3 matrixTranslation, matrixRotation, matrixScale;
    // ImGuizmo::DecomposeMatrixToComponents(gizmoMatrix.m16, matrixTranslation, matrixRotation, matrixScale);
    // ImGui::InputFloat3("Tr", (float *)&matrixTranslation, 3);
    // ImGui::InputFloat3("Rt", (float *)&matrixRotation, 3);
    // ImGui::InputFloat3("Sc", (float *)&matrixScale, 3);
    // ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, gizmoMatrix.m16);
    //
    // These functions have some numerical stability issues for now. Use with caution.
    IMGUI_API void DecomposeMatrixToComponents(const float *matrix, ImVec3& translation, ImVec3& rotation, ImVec3& scale);
    IMGUI_API void RecomposeMatrixFromComponents(const ImVec3& translation, const ImVec3& rotation, const ImVec3& scale, float *matrix);

    IMGUI_API void SetRect(float x, float y, float width, float height);
    // default is false
    IMGUI_API void SetOrthographic(bool isOrthographic);

    IMGUI_API void DrawLine(const float *view, const float *projection, const float *matrix, ImVec3 p1, ImVec3 p2, ImU32 col = IM_COL32_WHITE, float thickness = 1.f);
    IMGUI_API void DrawPoint(const float *view, const float *projection, const float *matrix, ImVec3 p, ImU32 col = IM_COL32_WHITE, float size = 1.f);
    IMGUI_API void DrawGrid(const float *view, const float *projection, const float *matrix, const float gridSize);

    IMGUI_API void DrawTriangles(ImDrawList* draw_list, const std::vector<Model_Triangle>& triangles);
    IMGUI_API void DrawQuats(ImDrawList* draw_list, const std::vector<ImVec2>& triProj, const std::vector<ImU32>& colLight);
    
    IMGUI_API void UpdateModel(const float *view, const float *projection, Model* model);
    IMGUI_API void DrawModel(const float *view, const float *projection, Model* model, bool bFace = true,  bool bMesh = false, bool draw_normal = false, ImU32 col = 0xFFFFFFFF, float thickness = 1.f);
    
    // Render a cube with face color corresponding to face normal. Usefull for debug/tests
    IMGUI_API void DrawCubes(const float *view, const float *projection, const float *matrices, int matrixCount);
    // call it when you want a gizmo
    // Needs view and projection matrices.
    // matrix parameter is the source matrix (where will be gizmo be drawn) and might be transformed by the function. Return deltaMatrix is optional
    // translation is applied in world space
    enum OPERATION
    {
        TRANSLATE_X = (1u << 0),
        TRANSLATE_Y = (1u << 1),
        TRANSLATE_Z = (1u << 2),
        ROTATE_X = (1u << 3),
        ROTATE_Y = (1u << 4),
        ROTATE_Z = (1u << 5),
        ROTATE_SCREEN = (1u << 6),
        SCALE_X = (1u << 7),
        SCALE_Y = (1u << 8),
        SCALE_Z = (1u << 9),
        BOUNDS = (1u << 10),
        SCALE_XU = (1u << 11),
        SCALE_YU = (1u << 12),
        SCALE_ZU = (1u << 13),

        TRANSLATE = TRANSLATE_X | TRANSLATE_Y | TRANSLATE_Z,
        ROTATE = ROTATE_X | ROTATE_Y | ROTATE_Z | ROTATE_SCREEN,
        SCALE = SCALE_X | SCALE_Y | SCALE_Z,
        SCALEU = SCALE_XU | SCALE_YU | SCALE_ZU, // universal
        UNIVERSAL = TRANSLATE | ROTATE | SCALEU
    };

    inline OPERATION operator|(OPERATION lhs, OPERATION rhs)
    {
        return static_cast<OPERATION>(static_cast<int>(lhs) | static_cast<int>(rhs));
    }

    enum MODE
    {
        LOCAL,
        WORLD
    };

    // Utils
    IMGUI_API void Frustum(float left, float right, float bottom, float top, float znear, float zfar, float* m16);
    IMGUI_API void Perspective(float fovyInDegrees, float aspectRatio, float znear, float zfar, float* m16, float x_shift = 0.f, float y_shift = 0.f);
    IMGUI_API void Cross(const float* a, const float* b, float* r);
    IMGUI_API float Dot(const float* a, const float* b);
    IMGUI_API void Normalize(const float* a, float* r);
    IMGUI_API void LookAt(const float* eye, const float* at, const float* up, float* m16);

    IMGUI_API bool Manipulate(const float *view, const float *projection, OPERATION operation, MODE mode, float *matrix, float *deltaMatrix = NULL, const float *snap = NULL, const float *localBounds = NULL, const float *boundsSnap = NULL);
    //
    // Please note that this cubeview is patented by Autodesk : https://patents.google.com/patent/US7782319B2/en
    // It seems to be a defensive patent in the US. I don't think it will bring troubles using it as
    // other software are using the same mechanics. But just in case, you are now warned!
    //
    IMGUI_API bool ViewManipulate(float *view, float length, ImVec2 position, ImVec2 size, ImU32 backgroundColor, float* xAngle = nullptr, float* yAngle = nullptr);

    // use this version if you did not call Manipulate before and you are just using ViewManipulate
    IMGUI_API bool ViewManipulate(float *view, const float *projection, OPERATION operation, MODE mode, float *matrix, float length, ImVec2 position, ImVec2 size, ImU32 backgroundColor);

    IMGUI_API void SetID(int id);

    // return true if the cursor is over the operation's gizmo
    IMGUI_API bool IsOver(OPERATION op);
    IMGUI_API void SetGizmoSizeClipSpace(float value);

    // Allow axis to flip
    // When true (default), the guizmo axis flip for better visibility
    // When false, they always stay along the positive world/local axis
    IMGUI_API void AllowAxisFlip(bool value);

   	// Configure the limit where axis are hidden
    IMGUI_API void SetAxisLimit(float value);
   	// Configure the limit where planes are hiden
    IMGUI_API void SetPlaneLimit(float value);

    IMGUI_API void DrawBoundingBox(
        const float* _View,
        const float* _Projection,
        const float* _Matrix,
        const float* _Min,
        const float* _Max);

    enum COLOR
    {
        DIRECTION_X,      // directionColor[0]
        DIRECTION_Y,      // directionColor[1]
        DIRECTION_Z,      // directionColor[2]
        PLANE_X,          // planeColor[0]
        PLANE_Y,          // planeColor[1]
        PLANE_Z,          // planeColor[2]
        SELECTION,        // selectionColor
        INACTIVE,         // inactiveColor
        TRANSLATION_LINE, // translationLineColor
        SCALE_LINE,
        ROTATION_USING_BORDER,
        ROTATION_USING_FILL,
        HATCHED_AXIS_LINES,
        TEXT,
        TEXT_SHADOW,
        COUNT
    };

    struct Style
    {
        IMGUI_API Style();

        float TranslationLineThickness;   // Thickness of lines for translation gizmo
        float TranslationLineArrowSize;   // Size of arrow at the end of lines for translation gizmo
        float RotationLineThickness;      // Thickness of lines for rotation gizmo
        float RotationOuterLineThickness; // Thickness of line surrounding the rotation gizmo
        float ScaleLineThickness;         // Thickness of lines for scale gizmo
        float ScaleLineCircleSize;        // Size of circle at the end of lines for scale gizmo
        float HatchedAxisLineThickness;   // Thickness of hatched axis lines
        float CenterCircleSize;           // Size of circle at the center of the translate/scale gizmo

        ImVec4 Colors[COLOR::COUNT];
    };

    IMGUI_API Style& GetStyle();

#if IMGUI_BUILD_EXAMPLE
    // For Demo
    IMGUI_API void ShowImGuiZmoDemo();
#endif
}

struct IMGUI_API Model_Triangle
{
    bool skipped {false};
    ImVec2 s_TriProj[3];
    ImU32  s_ColLight[3];
    ImVec2 s_NormProj;
    ImVec3 s_BarProj;
};

struct IMGUI_API Model
{
    Model() {}
    ~Model() { if (model_data) delete model_data; }
    int64_t model_id {0};
    ModelData * model_data {nullptr};
    bool useSnap {false};
    bool boundSizing {false};
    bool boundSizingSnap {false};
    bool showManipulate {false};
    ImVec3 snap {1.f, 1.f, 1.f};
    ImVec3 boundsSnap {0.1f, 0.1f, 0.1f};
    ImVec3 bounds[2] {{-0.5f, -0.5f, -0.5f},{0.5f, 0.5f, 0.5f}};
    ImGuizmo::MODE currentGizmoMode {ImGuizmo::LOCAL};
    ImGuizmo::OPERATION currentGizmoOperation {ImGuizmo::UNIVERSAL};
    ImVec3 matrixTranslation;
    ImVec3 matrixRotation;
    ImVec3 matrixScale;
    ImMat4x4 identity_matrix;
    // proj stocks
    std::mutex m_updating;
    std::vector<Model_Triangle> triangles;
};
