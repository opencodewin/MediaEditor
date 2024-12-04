// ImGuiTexInspect, a texture inspector widget for dear imgui

#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_tex_inspect.h"

namespace ImGuiTexInspect
{
//-------------------------------------------------------------------------
// [SECTION] UTILITIES
//-------------------------------------------------------------------------
// Returns true if a flag is set
template <typename TSet, typename TFlag>
static inline bool HasFlag(TSet set, TFlag flag)
{
    return (set & flag) == flag;
}

// Set flag or flags in set
template <typename TSet, typename TFlag>
static inline void SetFlag(TSet &set, TFlag flags)
{
    set = static_cast<TSet>(set | flags);
}

// Clear flag or flags in set
template <typename TSet, typename TFlag>
static inline void ClearFlag(TSet &set, TFlag flag)
{
    set = static_cast<TSet>(set & ~flag);
}

// Proper modulus operator, as opposed to remainder as calculated by %
template <typename T>
static inline T Modulus(T a, T b)
{
    return a - b * ImFloor(a / b);
}

// Defined in recent versions of imgui_internal.h.  Included here in case user is on older
// imgui version.
static inline float ImFloorSigned(float f)
{
    return (float)((f >= 0 || (int)f == f) ? (int)f : (int)f - 1);
}

static inline float Round(float f)
{
    return ImFloorSigned(f + 0.5f);
}

static inline ImVec2 Abs(ImVec2 v)
{
    return ImVec2(ImAbs(v.x), ImAbs(v.y));
}

//-------------------------------------------------------------------------
// [SECTION] STRUCTURES
//-------------------------------------------------------------------------
struct ShaderOptions
{
    float  ColorTransform[16] = {};                    // See CurrentInspector_SetColorMatrix for details
    float  ColorOffset[4] = {};

    ImVec4 BackgroundColor                = {0,0,0,0}; // Color used for alpha blending
    float  PremultiplyAlpha               = 0;         // If 1 then color will be multiplied by alpha in shader, before blend stage 
    float  DisableFinalAlpha              = 0;         // If 1 then fragment shader will always output alpha = 1

    bool   ForceNearestSampling           = false;     // If true fragment shader will always sample from texel centers

    ImVec2 GridWidth                      = {0,0};     // Width in UV coords of grid line
    ImVec4 GridColor                      = {0,0,0,0};

    void   ResetColorTransform();
    ShaderOptions();
};

struct Inspector
{
    ImGuiID ID;
    bool Initialized = false;

    // Texture
    ImTextureID Texture = ImTextureID{};
    ImVec2 TextureSize = {0, 0};        // Size in texels of texture
    float PixelAspectRatio = 1;         // Values other than 1 not supported yet

    // View State
    bool IsDragging = false;            // Is user currently dragging to pan view
    ImVec2 PanPos = {0.5f, 0.5f};       // The UV value at the center of the current view
    ImVec2 Scale = {1, 1};              // 1 pixel is 1 texel

    ImVec2 PanelTopLeftPixel = {0, 0};  // Top left of view in ImGui pixel coordinates
    ImVec2 PanelSize = {0, 0};          // Size of area allocated to drawing the image in pixels.

    ImVec2 ViewTopLeftPixel = {0, 0};   // Position in ImGui pixel coordinates
    ImVec2 ViewSize = {0, 0};           // Rendered size of current image. This could be smaller than panel size if user has zoomed out.
    ImVec2 ViewSizeUV = {0, 0};         // Visible region of the texture in UV coordinates

    /* Conversion transforms to go back and forth between screen pixels  (what ImGui considers screen pixels) and texels*/
    Transform2D TexelsToPixels;
    Transform2D PixelsToTexels;

    // Cached pixel data
    bool HaveCurrentTexelData = false;
    BufferDesc Buffer;

    /* We don't actually access texel data through this pointer.  We just 
     * manage its lifetime. The backend might have asked us to allocated a 
     * buffer, or it might not.  The pointer we actually use to access texel 
     * data is in the Buffer object above (which depending on what the backend 
     * did might point to the same memory as this pointer)
     */
    ImU8 *DataBuffer = nullptr;  
    size_t DataBufferSize = 0;

    // Configuration
    InspectorFlags Flags = 0;

    // Background mode
    InspectorAlphaMode AlphaMode = InspectorAlphaMode_ImGui;
    ImVec4 CustomBackgroundColor = {0, 0, 0, 1};

    // Scaling limits
    ImVec2 ScaleMin = {0.1f, 0.1f};
    ImVec2 ScaleMax = {200, 200};

    // Grid
    float MinimumGridSize = 4; // Don't draw the grid if lines would be closer than MinimumGridSize pixels

    // Annotations
    ImU32 MaxAnnotatedTexels = 0;

    // Color transformation
    ShaderOptions ActiveShaderOptions;
    ShaderOptions CachedShaderOptions;

    ~Inspector();
};

//-------------------------------------------------------------------------
// [SECTION] INTERNAL FUNCTIONS
//-------------------------------------------------------------------------

Inspector *GetByKey(const Context *ctx, ImGuiID key);
Inspector *GetOrAddByKey(Context *ctx, ImGuiID key);

void SetPanPos(Inspector *inspector, ImVec2 pos);
void SetScale(Inspector *inspector, ImVec2 scale);
void SetScale(Inspector *inspector, float scaleY);
void RoundPanPos(Inspector *inspector);

ImU8 *GetBuffer(Inspector *inspector, size_t bytes);

/* GetTexelsToPixels 
 * Calculate a transform to convert from texel coordinates to screen pixel coordinates
 * */
Transform2D GetTexelsToPixels(ImVec2 screenTopLeft, ImVec2 screenViewSize, ImVec2 uvTopLeft, ImVec2 uvViewSize, ImVec2 textureSize);

//-------------------------------------------------------------------------
// [SECTION] IMGUI UTILS
//-------------------------------------------------------------------------
/* TextVector
 * Draws a single-column ImGui table with one row for each provided string
 */
void TextVector(const char *title, const char *const *strings, int n);

/* PushDisabled & PopDisabled
 * Push and Pop and ImGui styles that disable and "grey out" ImGui elements
 * by making them non interactive and transparent*/
void PushDisabled();
void PopDisabled();

//-------------------------------------------------------------------------
// [SECTION] BACKEND FUNCTIONS
//-------------------------------------------------------------------------
void BackEnd_SetShader(const ImDrawList *drawList, const ImDrawCmd *cmd, const Inspector *inspector);
bool BackEnd_GetData(Inspector *inspector, ImTextureID texture, int x, int y, int width, int height, BufferDesc *buffer);

} // namespace ImGuiTexInspect
