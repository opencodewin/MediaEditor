// ImGuiTexInspect, a texture inspector widget for dear imgui

//-------------------------------------------------------------------------
// [SECTION] INCLUDES
//-------------------------------------------------------------------------
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_tex_inspect.h"
#include "imgui_tex_inspect_internal.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_helper.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4996) // 'sprintf' considered unsafe
#endif

namespace ImGuiTexInspect
{

//-------------------------------------------------------------------------
// [SECTION] FORWARD DECLARATIONS
//-------------------------------------------------------------------------
void UpdateShaderOptions(Inspector *inspector);
void InspectorDrawCallback(const ImDrawList *parent_list, const ImDrawCmd *cmd);
bool GetVisibleTexelRegionAndGetData(Inspector *inspector, ImVec2 &texelTL, ImVec2 &texelBR);

//-------------------------------------------------------------------------
// [SECTION] GLOBAL STATE
//-------------------------------------------------------------------------

// Input mapping structure, default values listed in the comments.
struct InputMap
{
    ImGuiMouseButton PanButton; // LMB      enables panning when held
    InputMap();
};

InputMap::InputMap()
{
    PanButton = ImGuiMouseButton_Left;
}

// Settings configured via SetNextPanelOptions etc.
struct NextPanelSettings
{
    InspectorFlags ToSet = 0;
    InspectorFlags ToClear = 0;
};

// Main context / configuration structure for imgui_tex_inspect
struct Context
{
    InputMap                                    Input;                           // Input mapping config
    ImGuiStorage                                Inspectors;                      // All the inspectors we've seen
    Inspector *                                 CurrentInspector;                // Inspector currently being processed
    NextPanelSettings                           NextPanelOptions;                // Options configured for next inspector panel
    float                                       ZoomRate                 = 1.3f; // How fast mouse wheel affects zoom
    float                                       DefaultPanelHeight       = 600;  // Height of panel in pixels
    float                                       DefaultInitialPanelWidth = 600;  // Only applies when window first appears
    int                                         MaxAnnotations           = 1000; // Limit number of texel annotations for performance
};

Context *GContext = nullptr;

//-------------------------------------------------------------------------
// [SECTION] USER FUNCTIONS
//-------------------------------------------------------------------------

void Init()
{
    // Nothing to do here.  But there might be in a later version. So client code should still call it!
}

void Shutdown()
{
    // Nothing to do here.  But there might be in a later version. So client code should still call it!
}

Context *CreateContext()
{
    GContext = IM_NEW(Context);
    SetCurrentContext(GContext);
    return GContext;
}

void DestroyContext(Context *ctx)
{
    if (ctx == NULL)
    {
        ctx = GContext;
    }

    if (ctx == GContext)
    {
        GContext = NULL;
    }

    for (ImGuiStoragePair &pair : ctx->Inspectors.Data)
    {
        Inspector *inspector = (Inspector *)pair.val_p;
        if (inspector)
        {
            IM_DELETE(inspector);
        }
    }

    IM_DELETE(ctx);
}


void SetCurrentContext(Context *context)
{
    ImGuiTexInspect::GContext = context;
}

void SetNextPanelFlags(InspectorFlags setFlags, InspectorFlags clearFlags)
{
    SetFlag(GContext->NextPanelOptions.ToSet, setFlags);
    SetFlag(GContext->NextPanelOptions.ToClear, clearFlags);
}

bool BeginInspectorPanel(const char *title, ImTextureID texture, ImVec2 textureSize, InspectorFlags flags,
                         SizeIncludingBorder sizeIncludingBorder)
{
    const int borderWidth = 1;
    // Unpack size param.  It's in the SizeIncludingBorder structure just to make sure users know what they're requesting
    ImVec2 size = sizeIncludingBorder.Size;

    ImGuiWindow *window = ImGui::GetCurrentWindow();

    Context *ctx = GContext;

    const ImGuiID ID = window->GetID(title);
    const ImGuiIO &IO = ImGui::GetIO();

    // Create or find inspector
    bool justCreated = GetByKey(ctx, ID) == NULL;
    ctx->CurrentInspector = GetOrAddByKey(ctx, ID);
    Inspector *inspector = ctx->CurrentInspector;
    justCreated |= !inspector->Initialized;

    // Cache the basics
    inspector->ID = ID;
    inspector->Texture = texture;
    inspector->TextureSize = textureSize;
    inspector->Initialized = true;

    // Handle incoming flags. We keep special track of the 
    // newly set flags because somethings only take effect
    // the first time the flag is set.
    InspectorFlags newlySetFlags = ctx->NextPanelOptions.ToSet;
    if (justCreated)
    {
        SetFlag(newlySetFlags, flags);
        inspector->MaxAnnotatedTexels = ctx->MaxAnnotations;
    }
    SetFlag(inspector->Flags, newlySetFlags);
    ClearFlag(inspector->Flags, ctx->NextPanelOptions.ToClear);
    ClearFlag(newlySetFlags, ctx->NextPanelOptions.ToClear);
    ctx->NextPanelOptions = NextPanelSettings();

    // Calculate panel size
    ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail();

    ImVec2 panelSize;
    // A size value of zero indicates we should use defaults
    if (justCreated)
    {
        panelSize = {size.x == 0 ? ImMax(ctx->DefaultInitialPanelWidth, contentRegionAvail.x) : size.x,
                     size.y == 0 ? ctx->DefaultPanelHeight : size.y};
    }
    else
    {
        panelSize = {size.x == 0 ? contentRegionAvail.x : size.x, size.y == 0 ? ctx->DefaultPanelHeight : size.y};
    }

    inspector->PanelSize = panelSize;
    ImVec2 availablePanelSize = panelSize - ImVec2(borderWidth, borderWidth) * 2;

    {
        // Possibly update scale
        float newScale = -1;

        if (HasFlag(newlySetFlags, InspectorFlags_FillVertical))
        {
            newScale = availablePanelSize.y / textureSize.y;
        }
        else if (HasFlag(newlySetFlags, InspectorFlags_FillHorizontal))
        {
            newScale = availablePanelSize.x / textureSize.x;
        }
        else if (justCreated)
        {
            newScale = 1;
        }

        if (newScale != -1)
        {
            inspector->Scale = ImVec2(newScale, newScale);
            SetPanPos(inspector, ImVec2(0.5f, 0.5f));
        }
    }

    RoundPanPos(inspector);

    ImVec2 textureSizePixels = inspector->Scale * textureSize;  // Size whole texture would appear on screen
    ImVec2 viewSizeUV = availablePanelSize / textureSizePixels; // Cropped size in terms of UV
    ImVec2 uv0 = inspector->PanPos - viewSizeUV * 0.5;
    ImVec2 uv1 = inspector->PanPos + viewSizeUV * 0.5;

    ImVec2 drawImageOffset{borderWidth, borderWidth};
    ImVec2 viewSize = availablePanelSize;

    if ((inspector->Flags & InspectorFlags_ShowWrap) == 0)
    {
        /* Don't crop the texture to UV [0,1] range.  What you see outside this 
         * range will depend on API and texture properties */
        if (textureSizePixels.x < availablePanelSize.x)
        {
            // Not big enough to horizontally fill view
            viewSize.x = ImFloor(textureSizePixels.x);
            drawImageOffset.x += ImFloor((availablePanelSize.x - textureSizePixels.x) / 2);
            uv0.x = 0;
            uv1.x = 1;
            viewSizeUV.x = 1;
            inspector->PanPos.x = 0.5f;
        }
        if (textureSizePixels.y < availablePanelSize.y)
        {
            // Not big enough to vertically fill view
            viewSize.y = ImFloor(textureSizePixels.y);
            drawImageOffset.y += ImFloor((availablePanelSize.y - textureSizePixels.y) / 2);
            uv0.y = 0;
            uv1.y = 1;
            viewSizeUV.y = 1;
            inspector->PanPos.y = 0.5;
        }
    }

    if (HasFlag(flags,InspectorFlags_FlipX))
    {
        ImSwap(uv0.x, uv1.x);
        viewSizeUV.x *= -1;
    }

    if (HasFlag(flags,InspectorFlags_FlipY))
    {
        ImSwap(uv0.y, uv1.y);
        viewSizeUV.y *= -1;
    }

    inspector->ViewSize = viewSize;
    inspector->ViewSizeUV = viewSizeUV;

    /* We use mouse scroll to zoom so we don't want scroll to propagate to 
     * parent window. For this to happen we must NOT set 
     * ImGuiWindowFlags_NoScrollWithMouse.  This seems strange but it's the way 
     * ImGui works.  Also we must ensure the ScrollMax.y is not zero for the 
     * child window. */
    if (ImGui::BeginChild(title, panelSize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove))
    {
        // See comment above
        ImGui::GetCurrentWindow()->ScrollMax.y = 1.0f;

        // Callback for using our own image shader 
        ImGui::GetWindowDrawList()->AddCallback(InspectorDrawCallback, inspector);

        // Keep track of size of area that we draw for borders later
        inspector->PanelTopLeftPixel = ImGui::GetCursorScreenPos();
        ImGui::SetCursorPos(ImGui::GetCursorPos() + drawImageOffset);
        inspector->ViewTopLeftPixel = ImGui::GetCursorScreenPos();

        UpdateShaderOptions(inspector);
        inspector->CachedShaderOptions = inspector->ActiveShaderOptions;
        ImGui::Image(texture, viewSize, uv0, uv1);
        ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

        /* Matrices for going back and forth between texel coordinates in the 
         * texture and screen coordinates based on where texture is drawn. 
         * Useful for annotations and mouse hover etc. */
        inspector->TexelsToPixels = GetTexelsToPixels(inspector->ViewTopLeftPixel, viewSize, uv0, viewSizeUV, inspector->TextureSize);
        inspector->PixelsToTexels = inspector->TexelsToPixels.Inverse();

        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 mousePosTexel = inspector->PixelsToTexels * mousePos;
        ImVec2 mouseUV = mousePosTexel / textureSize;
        mousePosTexel.x = Modulus(mousePosTexel.x, textureSize.x);
        mousePosTexel.y = Modulus(mousePosTexel.y, textureSize.y);

        if (ImGui::IsItemHovered() && (inspector->Flags & ImGuiTexInspect::InspectorFlags_NoTooltip) == 0)
        {
            // Show a tooltip for currently hovered texel
            ImVec2 texelTL;
            ImVec2 texelBR;
            if (GetVisibleTexelRegionAndGetData(inspector, texelTL, texelBR))
            {
                ImVec4 color = GetTexel(&inspector->Buffer, (int)mousePosTexel.x, (int)mousePosTexel.y);

                char buffer[128];
                snprintf(buffer, 128, "UV: (%.5f, %.5f)\nTexel: (%d, %d)", mouseUV.x, mouseUV.y, (int)mousePosTexel.x, (int)mousePosTexel.y);

                ImGui::ColorTooltip(buffer, &color.x, 0);
            }
        }

        bool hovered = ImGui::IsWindowHovered();

        {  //DRAGGING
            
            // start drag
            if (!inspector->IsDragging && hovered && IO.MouseClicked[ctx->Input.PanButton])
            {
                inspector->IsDragging = true;
            }
            // carry on dragging
            else if (inspector->IsDragging)
            {
                ImVec2 uvDelta = IO.MouseDelta * viewSizeUV / viewSize;
                inspector->PanPos -= uvDelta;
                RoundPanPos(inspector);
            }

            // end drag
            if (inspector->IsDragging && (IO.MouseReleased[ctx->Input.PanButton] || !IO.MouseDown[ctx->Input.PanButton]))
            {
                inspector->IsDragging = false;
            }
        }

        // ZOOM
        if (hovered && IO.MouseWheel != 0)
        {
            float zoomRate  = ctx->ZoomRate;
            float scale     = inspector->Scale.y;
            float prevScale = scale;

            bool keepTexelSizeRegular = scale > inspector->MinimumGridSize && !HasFlag(inspector->Flags, InspectorFlags_NoGrid);
            if (IO.MouseWheel > 0)
            {
                scale *= zoomRate;
                if (keepTexelSizeRegular)
                {
                    // It looks nicer when all the grid cells are the same size
                    // so keep scale integer when zoomed in
                    scale = ImCeil(scale);
                }
            }
            else
            {
                scale /= zoomRate;
                if (keepTexelSizeRegular)
                {
                    // See comment above. We're doing a floor this time to make
                    // sure the scale always changes when scrolling
                    scale = ImFloorSigned(scale);
                }
            }
            /* To make it easy to get back to 1:1 size we ensure that we stop 
             * here without going straight past it*/
            if ((prevScale < 1 && scale > 1) || (prevScale > 1 && scale < 1))
            {
                scale = 1;
            }
            SetScale(inspector, ImVec2(inspector->PixelAspectRatio * scale, scale));
            SetPanPos(inspector, inspector->PanPos + (mouseUV - inspector->PanPos) * (1 - prevScale / scale));
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool BeginInspectorPanel(const char *name, ImTextureID texture, ImVec2 textureSize, InspectorFlags flags)
{
    return BeginInspectorPanel(name, texture, textureSize, flags, SizeIncludingBorder{{0, 0}});
}

bool BeginInspectorPanel(const char *name, ImTextureID texture, ImVec2 textureSize, InspectorFlags flags, SizeExcludingBorder size)
{
    // Correct the size to include the border, but preserve 0 which has a special meaning
    return BeginInspectorPanel(name, texture, textureSize, flags,
                               SizeIncludingBorder{ImVec2{size.size.x == 0 ? 0 : size.size.x + 2, 
                                                          size.size.y == 0 ? 0 : size.size.y + 2}});
}

void EndInspectorPanel()
{
    const ImU32 innerBorderColour = 0xFFFFFFFF;
    const ImU32 outerBorderColour = 0xFF888888;
    Inspector *inspector = GContext->CurrentInspector;

    // Draw out border around whole inspector panel
    ImGui::GetWindowDrawList()->AddRect(inspector->PanelTopLeftPixel, inspector->PanelTopLeftPixel + inspector->PanelSize,
                                        outerBorderColour);

    // Draw innder border around texture.  If zoomed in this will completely cover the outer border
    ImGui::GetWindowDrawList()->AddRect(inspector->ViewTopLeftPixel - ImVec2(1, 1),
                                        inspector->ViewTopLeftPixel + inspector->ViewSize + ImVec2(1, 1), innerBorderColour);

    ImGui::EndChild();

    // We set this back to false every frame in case the texture is dynamic
    if (!HasFlag(inspector->Flags, InspectorFlags_NoAutoReadTexture))
    {
        inspector->HaveCurrentTexelData = false;
    }
}

void ReleaseInspectorData(ImGuiID ID)
{
    Inspector *inspector = GetByKey(GContext, ID);

    if (inspector == NULL)
        return;

    if (inspector->DataBuffer)
    {
        IM_FREE(inspector->DataBuffer);
        inspector->DataBuffer = NULL;
        inspector->DataBufferSize = 0;
    }

    /* In a later version we will remove inspector from the inspector table 
     * altogether. For now we reset the whole inspector structure to prevent 
     * clients relying on persisted data. 
     */
    *inspector = Inspector();
}


ImGuiID CurrentInspector_GetID()
{
    return GContext->CurrentInspector->ID;
}

void CurrentInspector_SetColorMatrix(const float (&matrix)[16], const float (&colorOffset)[4])
{
    Inspector *inspector = GContext->CurrentInspector;
    ShaderOptions *shaderOptions = &inspector->ActiveShaderOptions;
    memcpy(shaderOptions->ColorTransform, matrix, sizeof(matrix));
    memcpy(shaderOptions->ColorOffset, colorOffset, sizeof(colorOffset));
}

void CurrentInspector_ResetColorMatrix()
{
    Inspector *inspector = GContext->CurrentInspector;
    ShaderOptions *shaderOptions = &inspector->ActiveShaderOptions;
    shaderOptions->ResetColorTransform();
}

void CurrentInspector_SetAlphaMode(InspectorAlphaMode mode)
{
    Inspector *inspector = GContext->CurrentInspector;
    ShaderOptions *shaderOptions = &inspector->ActiveShaderOptions;

    inspector->AlphaMode = mode;

    switch (mode)
    {
    case InspectorAlphaMode_Black:
        shaderOptions->BackgroundColor = ImVec4(0, 0, 0, 1);
        shaderOptions->DisableFinalAlpha = 1;
        shaderOptions->PremultiplyAlpha = 1;
        break;
    case InspectorAlphaMode_White:
        shaderOptions->BackgroundColor = ImVec4(1, 1, 1, 1);
        shaderOptions->DisableFinalAlpha = 1;
        shaderOptions->PremultiplyAlpha = 1;
        break;
    case InspectorAlphaMode_ImGui:
        shaderOptions->BackgroundColor = ImVec4(0, 0, 0, 0);
        shaderOptions->DisableFinalAlpha = 0;
        shaderOptions->PremultiplyAlpha = 0;
        break;
    case InspectorAlphaMode_CustomColor:
        shaderOptions->BackgroundColor = inspector->CustomBackgroundColor;
        shaderOptions->DisableFinalAlpha = 1;
        shaderOptions->PremultiplyAlpha = 1;
        break;
    }
}

void CurrentInspector_SetFlags(InspectorFlags toSet, InspectorFlags toClear)
{
    Inspector *inspector = GContext->CurrentInspector;
    SetFlag(inspector->Flags, toSet);
    ClearFlag(inspector->Flags, toClear);
}

void CurrentInspector_SetGridColor(ImU32 color)
{
    Inspector *inspector = GContext->CurrentInspector;
    float alpha = inspector->ActiveShaderOptions.GridColor.w;
    inspector->ActiveShaderOptions.GridColor = ImColor(color);
    inspector->ActiveShaderOptions.GridColor.w = alpha;
}

void CurrentInspector_SetMaxAnnotations(int maxAnnotations)
{
    Inspector *inspector = GContext->CurrentInspector;
    inspector->MaxAnnotatedTexels = maxAnnotations;
}

void CurrentInspector_InvalidateTextureCache()
{
    Inspector *inspector = GContext->CurrentInspector;
    inspector->HaveCurrentTexelData = false;
}

void CurrentInspector_SetCustomBackgroundColor(ImVec4 color)
{
    Inspector *inspector = GContext->CurrentInspector;
    inspector->CustomBackgroundColor = color;
    if (inspector->AlphaMode == InspectorAlphaMode_CustomColor)
    {
        inspector->ActiveShaderOptions.BackgroundColor = color;
    }
}

void CurrentInspector_SetCustomBackgroundColor(ImU32 color)
{
    CurrentInspector_SetCustomBackgroundColor(ImGui::ColorConvertU32ToFloat4(color));
}

void DrawColorMatrixEditor()
{
    const char *colorVectorNames[] = {"R", "G", "B", "A", "1"};
    const char *finalColorVectorNames[] = {"R'", "G'", "B'", "A'"};
    const float dragSpeed = 0.02f;
    Inspector *inspector = GContext->CurrentInspector;
    ShaderOptions *shaderOptions = &inspector->ActiveShaderOptions;
    
    // Left hand side of equation. The final color vector which is the actual drawn color
    TextVector("FinalColorVector", finalColorVectorNames, IM_ARRAYSIZE(finalColorVectorNames));

    ImGui::SameLine();
    ImGui::TextUnformatted("=");
    ImGui::SameLine();

    // Right hand side of the equation: the Matrix. This is the editable part
    ImGui::BeginGroup();
    for (int i = 0; i < 4; ++i)
    {
        ImGui::PushID(i);
        for (int j = 0; j < 4; ++j)
        {
            ImGui::PushID(j);
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("##f", &shaderOptions->ColorTransform[j * 4 + i], dragSpeed);
            ImGui::PopID();
            ImGui::SameLine();
        }
        ImGui::SetNextItemWidth(50);
        ImGui::DragFloat("##offset", &shaderOptions->ColorOffset[i], dragSpeed);
        ImGui::PopID();
    }
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::TextUnformatted("*");
    ImGui::SameLine();

    // Right hand side of equation.  The input vector, the source color of the texel.
    TextVector("ColorVector", colorVectorNames, IM_ARRAYSIZE(colorVectorNames));
}

void DrawGridEditor()
{
    Inspector *inspector = GContext->CurrentInspector;

    ImGui::BeginGroup();
    bool gridEnabled = !HasFlag(inspector->Flags, InspectorFlags_NoGrid);
    if (ImGui::Checkbox("Grid", &gridEnabled))
    {
        if (gridEnabled)
        {
            CurrentInspector_ClearFlags(InspectorFlags_NoGrid);
        }
        else
        {
            CurrentInspector_SetFlags(InspectorFlags_NoGrid);
        }
    }
    if (gridEnabled)
    {
        ImGui::SameLine();
        ImGui::ColorEdit3("Grid Color", (float *)&inspector->ActiveShaderOptions.GridColor, ImGuiColorEditFlags_NoInputs);
    }

    ImGui::EndGroup();
}

void DrawColorChannelSelector()
{
    Inspector *inspector = GContext->CurrentInspector;
    ShaderOptions *shaderOptions = &inspector->ActiveShaderOptions;

    ImGuiStorage *storage = ImGui::GetStateStorage();
    const ImGuiID greyScaleID = ImGui::GetID("greyScale");

    bool greyScale = storage->GetBool(greyScaleID, false);

    bool red = shaderOptions->ColorTransform[0] > 0;
    bool green = shaderOptions->ColorTransform[5] > 0;
    bool blue = shaderOptions->ColorTransform[10] > 0;

    bool changed = false;

    // In greyScale made we draw the red, green, blue checkboxes as disabled
    if (greyScale)
    {
        PushDisabled();
    }
    ImGui::BeginGroup();
    changed |= ImGui::Checkbox("Red", &red);
    changed |= ImGui::Checkbox("Green", &green);
    changed |= ImGui::Checkbox("Blue", &blue);
    ImGui::EndGroup();

    ImGui::SameLine();

    if (greyScale)
    {
        PopDisabled();
    }

    if (changed)
    {
        // Overwrite the color transform matrix with one based on the settings
        shaderOptions->ResetColorTransform();
        shaderOptions->ColorTransform[0] = red ? 1.0f : 0.0f;
        shaderOptions->ColorTransform[5] = green ? 1.0f : 0.0f;
        shaderOptions->ColorTransform[10] = blue ? 1.0f : 0.0f;
    }

    ImGui::BeginGroup();
    if (ImGui::Checkbox("Grey", &greyScale))
    {
        shaderOptions->ResetColorTransform();
        storage->SetBool(greyScaleID, greyScale);
        if (greyScale)
        {
            for (int i = 0; i < 3; i++)
            {
                for (int j = 0; j < 3; j++)
                {
                    shaderOptions->ColorTransform[i * 4 + j] = 0.333f;
                }
            }
        }
    }

    ImGui::EndGroup();
}

void DrawAlphaModeSelector()
{
    Inspector *inspector = GContext->CurrentInspector;

    const char *alphaModes[] = {"ImGui Background", "Black", "White", "Custom Color"};

    ImGui::SetNextItemWidth(200);

    InspectorAlphaMode currentAlphaMode = inspector->AlphaMode;

    ImGui::Combo("Alpha Modes", (int *)&currentAlphaMode, alphaModes, IM_ARRAYSIZE(alphaModes));

    CurrentInspector_SetAlphaMode(currentAlphaMode);

    if (inspector->AlphaMode == InspectorAlphaMode_CustomColor)
    {
        ImVec4 backgroundColor = inspector->CustomBackgroundColor;
        if (ImGui::ColorEdit3("Background Color", (float *)&backgroundColor, 0))
        {
            CurrentInspector_SetCustomBackgroundColor(backgroundColor);
        }
    }
}

void SetZoomRate(float rate)
{
    GContext->ZoomRate = rate;
}

//-------------------------------------------------------------------------
// [SECTION] Life Cycle
//-------------------------------------------------------------------------

Inspector::~Inspector()
{
    if (DataBuffer)
    {
        IM_FREE(DataBuffer);
    }
}
//-------------------------------------------------------------------------
// [SECTION] Scaling and Panning
//-------------------------------------------------------------------------
void RoundPanPos(Inspector *inspector)
{
    if ((inspector->Flags & InspectorFlags_ShowWrap) > 0)
    {
        /* PanPos is the point in the center of the current view. Allow the 
         * user to pan anywhere as long as the view center is inside the 
         * texture.*/
        inspector->PanPos = ImClamp(inspector->PanPos, ImVec2(0, 0), ImVec2(1, 1));
    }
    else
    {
        /* When ShowWrap mode is disabled the limits are a bit more strict. We 
         * try to keep it so that the user cannot pan past the edge of the 
         * texture at all.*/
        ImVec2 absViewSizeUV = Abs(inspector->ViewSizeUV);
        inspector->PanPos = ImMax(inspector->PanPos - absViewSizeUV / 2, ImVec2(0, 0)) + absViewSizeUV / 2;
        inspector->PanPos = ImMin(inspector->PanPos + absViewSizeUV / 2, ImVec2(1, 1)) - absViewSizeUV / 2;
    }

    /* If inspector->scale is 1 then we should ensure that pixels are aligned 
     * with texel centers to get pixel-perfect texture rendering*/
    ImVec2 topLeftSubTexel = inspector->PanPos * inspector->Scale * inspector->TextureSize - inspector->ViewSize * 0.5f;

    if (inspector->Scale.x >= 1)
    {
        topLeftSubTexel.x = Round(topLeftSubTexel.x);
    }
    if (inspector->Scale.y >= 1)
    {
        topLeftSubTexel.y = Round(topLeftSubTexel.y);
    }
    inspector->PanPos = (topLeftSubTexel + inspector->ViewSize * 0.5f) / (inspector->Scale * inspector->TextureSize);
}

void SetPanPos(Inspector *inspector, ImVec2 pos)
{
    inspector->PanPos = pos;
    RoundPanPos(inspector);
}

void SetScale(Inspector *inspector, ImVec2 scale)
{
    scale = ImClamp(scale, inspector->ScaleMin, inspector->ScaleMax);

    inspector->ViewSizeUV *= inspector->Scale / scale;

    inspector->Scale = scale;
    
    // Only force nearest sampling if zoomed in
    inspector->ActiveShaderOptions.ForceNearestSampling =
        (inspector->Scale.x > 1.0f || inspector->Scale.y > 1.0f) && !HasFlag(inspector->Flags, InspectorFlags_NoForceFilterNearest);
    inspector->ActiveShaderOptions.GridWidth = ImVec2(1.0f / inspector->Scale.x, 1.0f / inspector->Scale.y);
}

void SetScale(Inspector *inspector, float scaleY)
{
    SetScale(inspector, ImVec2(scaleY * inspector->PixelAspectRatio, scaleY));
}
//-------------------------------------------------------------------------
// [SECTION] INSPECTOR MAP
//-------------------------------------------------------------------------

Inspector *GetByKey(const Context *ctx, ImGuiID key)
{
    return (Inspector *)ctx->Inspectors.GetVoidPtr(key);
}

Inspector *GetOrAddByKey(Context *ctx, ImGuiID key)
{
    Inspector *inspector = GetByKey(ctx, key);
    if (inspector)
    {
        return inspector;
    }
    else
    {
        inspector = IM_NEW(Inspector);
        ctx->Inspectors.SetVoidPtr(key, inspector);
        return inspector;
    }
}

//-------------------------------------------------------------------------
// [SECTION] TextureConversion class
//-------------------------------------------------------------------------

void ShaderOptions::ResetColorTransform()
{
    memset(ColorTransform, 0, sizeof(ColorTransform));
    for (int i = 0; i < 4; ++i)
    {
        ColorTransform[i * 4 + i] = 1;
    }
}

ShaderOptions::ShaderOptions()
{
    ResetColorTransform();
    memset(ColorOffset, 0, sizeof(ColorOffset));
}

//-------------------------------------------------------------------------
// [SECTION] UI and CONFIG
//-------------------------------------------------------------------------

void UpdateShaderOptions(Inspector *inspector)
{
    if (HasFlag(inspector->Flags, InspectorFlags_NoGrid) == false && inspector->Scale.y > inspector->MinimumGridSize)
    {
        // Enable grid in shader
        inspector->ActiveShaderOptions.GridColor.w = 1;
        SetScale(inspector, Round(inspector->Scale.y));
    }
    else
    {
        // Disable grid in shader
        inspector->ActiveShaderOptions.GridColor.w = 0;
    }

    inspector->ActiveShaderOptions.ForceNearestSampling =
        (inspector->Scale.x > 1.0f || inspector->Scale.y > 1.0f) && !HasFlag(inspector->Flags, InspectorFlags_NoForceFilterNearest);
}

// Draws a single column ImGui table with one row for each provided string
void TextVector(const char *title, const char *const *strings, int stringCount)
{
    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(50);
    if (ImGui::BeginTable(title, 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
    {
        for (int i = 0; i < stringCount; ++i)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(50);
            ImGui::Text("%s", strings[i]);
        }
        ImGui::EndTable();
    }
    ImGui::EndGroup();
}

const ImGuiCol disabledUIColorIds[] = {ImGuiCol_FrameBg, 
                                       ImGuiCol_FrameBgActive, 
                                       ImGuiCol_FrameBgHovered, 
                                       ImGuiCol_Text,
                                       ImGuiCol_CheckMark};

// Push disabled style for ImGui elements
void PushDisabled()
{
    for (ImGuiCol colorId : disabledUIColorIds)
    {
        ImVec4 color = ImGui::GetStyleColorVec4(colorId);
        color = color * ImVec4(0.5f, 0.5f, 0.5f, 0.5f);
        ImGui::PushStyleColor(colorId, color);
    }
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
}

// Pop disabled style for ImGui elements
void PopDisabled()
{
    for (ImGuiCol colorId : disabledUIColorIds)
    {
        (void)colorId;
        ImGui::PopStyleColor();
    }
    ImGui::PopItemFlag();
}

//-------------------------------------------------------------------------
// [SECTION] Rendering & Buffer Management
//-------------------------------------------------------------------------

void InspectorDrawCallback(const ImDrawList *parent_list, const ImDrawCmd *cmd)
{
    // Forward call to API-specific backend
    Inspector *inspector = (Inspector *)cmd->UserCallbackData;
    BackEnd_SetShader(parent_list, cmd, inspector);
}

// Calculate a transform to convert from texel coordinates to screen pixel coordinates
Transform2D GetTexelsToPixels(ImVec2 screenTopLeft, ImVec2 screenViewSize, ImVec2 uvTopLeft, ImVec2 uvViewSize, ImVec2 textureSize)
{
    ImVec2 uvToPixel = screenViewSize / uvViewSize;

    Transform2D transform;
    transform.Scale = uvToPixel / textureSize;
    transform.Translate.x = screenTopLeft.x - uvTopLeft.x * uvToPixel.x;
    transform.Translate.y = screenTopLeft.y - uvTopLeft.y * uvToPixel.y;
    return transform;
}

/* Fills in the AnnotationsDesc structure which provides all necessary 
 * information for code which draw annoations.  Returns false if no annoations 
 * should be drawn.  The maxAnnotatedTexels argument provides a way to override
 * the default maxAnnotatedTexels.
 */
bool GetAnnotationDesc(AnnotationsDesc *ad, ImU64 maxAnnotatedTexels)
{
    Inspector *inspector = GContext->CurrentInspector;

    if (maxAnnotatedTexels == 0)
    {
        maxAnnotatedTexels = inspector->MaxAnnotatedTexels;
    }
    if (maxAnnotatedTexels != 0)
    {
        /* Check if we would draw too many annotations.  This is to avoid poor 
         * frame rate when too zoomed out.  Increase MaxAnnotatedTexels if you 
         * want to draw more annotations.  Note that we don't use texelTL & 
         * texelBR to get total visible texels as this would cause flickering 
         * while panning as the exact number of visible texels changes.
        */

        ImVec2 screenViewSizeTexels = Abs(inspector->PixelsToTexels.Scale) * inspector->ViewSize;
        ImU64 approxVisibleTexelCount = (ImU64)screenViewSizeTexels.x * (ImU64)screenViewSizeTexels.y;
        if (approxVisibleTexelCount > maxAnnotatedTexels)
        {
            return false;
        }
    }

    // texelTL & texelBL will describe the currently visible texel region
    ImVec2 texelTL;
    ImVec2 texelBR;

    if (GetVisibleTexelRegionAndGetData(inspector, texelTL, texelBR))
    {
        ad->Buffer= inspector->Buffer;
        ad->DrawList = ImGui::GetWindowDrawList();
        ad->TexelsToPixels = inspector->TexelsToPixels;
        ad->TexelTopLeft = texelTL;
        ad->TexelViewSize = texelBR - texelTL;
        return true;
    }

    return false;
}

/* Calculates currently visible region of texture (which is returned in texelTL 
 * and texelBR) then also actually ensure that that data is in memory. Returns 
 * false if fetching data failed.
 */
bool GetVisibleTexelRegionAndGetData(Inspector *inspector, ImVec2 &texelTL, ImVec2 &texelBR)
{
    /* Figure out which texels correspond to the top left and bottom right
     * corners of the texture view.  The plus + ImVec2(1,1) is because we
     * want to draw partially visible texels on the bottom and right edges.
     */
    texelTL = ImFloor(inspector->PixelsToTexels * inspector->ViewTopLeftPixel);
    texelBR = ImFloor(inspector->PixelsToTexels * (inspector->ViewTopLeftPixel + inspector->ViewSize));

    if (texelTL.x > texelBR.x)
    {
        ImSwap(texelTL.x, texelBR.x);
    }
    if (texelTL.y > texelBR.y)
    {
        ImSwap(texelTL.y, texelBR.y);
    }

    /* Add ImVec2(1,1) because we want to draw partially visible texels on the 
     * bottom and right edges.*/
    texelBR += ImVec2(1,1);

    texelTL = ImClamp(texelTL, ImVec2(0, 0), inspector->TextureSize);
    texelBR = ImClamp(texelBR, ImVec2(0, 0), inspector->TextureSize);

    if (inspector->HaveCurrentTexelData)
    {
        return true;
    }

    // Now request pixel data for this region from backend

    ImVec2 texelViewSize = texelBR - texelTL;

    if (ImMin(texelViewSize.x, texelViewSize.y) > 0)
    {
        if (BackEnd_GetData(inspector, inspector->Texture, (int)texelTL.x, (int)texelTL.y, (int)texelViewSize.x, (int)texelViewSize.y,
                    &inspector->Buffer))
        {
            inspector->HaveCurrentTexelData = true;
            return true;
        }
    }
    return false;
}

/* This is a function the backends can use to allocate a buffer for storing 
 * texture texel data.  The buffer is owned by the inpsector so the backend 
 * code doesn't need to worry about freeing it.
 */
ImU8 *GetBuffer(Inspector *inspector, size_t bytes)
{
    if (inspector->DataBufferSize < bytes || inspector->DataBuffer == nullptr)
    {
        // We need to allocate a buffer

        if (inspector->DataBuffer)
        {
            IM_FREE(inspector->DataBuffer);
        }

        // Allocate slightly more than we need to avoid reallocating
        // very frequently in the case that size is increasing.
        size_t size = bytes * 5 / 4;
        inspector->DataBuffer = (ImU8 *)IM_ALLOC(size);
        inspector->DataBufferSize = size;
    }

    return inspector->DataBuffer;
}

ImVec4 GetTexel(const BufferDesc *bd, int x, int y)
{
    if (x < bd->StartX || x >= bd->StartX + bd->Width || y < bd->StartY || y >= bd->StartY + bd->Height)
    {
        // Outside the range of data in the buffer.
        return ImVec4();
    }
    // Calculate position in array
    size_t offset = ((size_t)bd->LineStride * (y - bd->StartY) + bd->Stride * (x - bd->StartX));
    if (bd->Data_float)
    {
        const float *texel = bd->Data_float + offset;
        // It's possible our buffer doesn't have all 4 channels so fill gaps in with zeros
        return ImVec4(                   texel[bd->Red], 
                bd->ChannelCount >= 2 ?  texel[bd->Green]  : 0, 
                bd->ChannelCount >= 3 ?  texel[bd->Blue]   : 0,
                bd->ChannelCount >= 4 ?  texel[bd->Alpha]  : 0);
    }
    else if (bd->Data_uint8_t)
    {
        const ImU8 *texel = bd->Data_uint8_t + offset;
        // It's possible our buffer doesn't have all 4 channels so fill gaps in with zeros.
        // Also map from [0,255] to [0,1]
        return ImVec4(                  (float)texel[bd->Red]   / 255.0f, 
                bd->ChannelCount >= 2 ? (float)texel[bd->Green] / 255.0f : 0, 
                bd->ChannelCount >= 3 ? (float)texel[bd->Blue]  / 255.0f : 0,
                bd->ChannelCount >= 4 ? (float)texel[bd->Alpha] / 255.0f  : 0);
    }
    else
    {
        return ImVec4();
    }
}
//-------------------------------------------------------------------------
// [SECTION] Annotations
//-------------------------------------------------------------------------

ValueText::ValueText(Format format)
{
    /* The ValueText annotation draws a string inside each texel displaying the 
     * values of each channel. We now select a format string based on the enum 
     * parameter*/
    switch (format)
    {
    case Format::HexString:
        TextFormatString = "#%02X%02X%02X%02X";
        TextColumnCount = 9;
        TextRowCount = 1;
        FormatAsFloats = false;
        break;
    case Format::BytesHex:
        TextFormatString = "R:#%02X\nG:#%02X\nB:#%02X\nA:#%02X";
        TextColumnCount = 5;
        TextRowCount = 4;
        FormatAsFloats = false;
        break;
    case Format::BytesDec:
        TextFormatString = "R:%3d\nG:%3d\nB:%3d\nA:%3d";
        TextColumnCount = 5;
        TextRowCount = 4;
        FormatAsFloats = false;
        break;
    case Format::Floats:
        TextFormatString = "%5.3f\n%5.3f\n%5.3f\n%5.3f";
        TextColumnCount = 5;
        TextRowCount = 4;
        FormatAsFloats = true;
        break;
    }
}

void ValueText::DrawAnnotation(ImDrawList *drawList, ImVec2 texel, Transform2D texelsToPixels, ImVec4 value)
{
    char buffer[64];
    ImGui::SetWindowFontScale(0.75);
    float fontHeight = ImGui::GetFontSize();
    float fontWidth = fontHeight / 2; /* WARNING this is a hack that gets a constant
    * character width from half the height.  This work for the default font but
    * won't work on other fonts which may even not be monospace.*/

    // Calculate size of text and check if it fits
    ImVec2 textSize = ImVec2((float)TextColumnCount * fontWidth, (float)TextRowCount * fontHeight);
    ImVec2 pixelCenter = texelsToPixels * texel;
    /* Choose black or white text based on how bright the texel.  I.e. don't 
     * draw black text on a dark background or vice versa. */
    float brightness = (value.x + value.y + value.z) * value.w / 3;
    ImU32 lineColor = brightness > 0.5 ? 0xFF000000 : 0xFFFFFFFF;
    drawList->AddRect(pixelCenter - texelsToPixels.Scale * 0.5f, pixelCenter + texelsToPixels.Scale * 0.5f, lineColor, 4, ImDrawFlags_RoundCornersAll);

    if (textSize.x > ImAbs(texelsToPixels.Scale.x) || textSize.y > ImAbs(texelsToPixels.Scale.y))
    {
        // Not enough room in texel to fit the text.  Don't draw it.
        ImGui::SetWindowFontScale(1.0);
        return;
    }

    if (FormatAsFloats)
    {
        snprintf(buffer, 64, TextFormatString, value.x, value.y, value.z, value.w);
    }
    else
    {
        /* Map [0,1] to [0,255]. Also clamp it since input data wasn't 
         * necessarily in [0,1] range. */
        ImU8 r = (ImU8)Round((ImClamp(value.x, 0.0f, 1.0f)) * 255);
        ImU8 g = (ImU8)Round((ImClamp(value.y, 0.0f, 1.0f)) * 255);
        ImU8 b = (ImU8)Round((ImClamp(value.z, 0.0f, 1.0f)) * 255);
        ImU8 a = (ImU8)Round((ImClamp(value.w, 0.0f, 1.0f)) * 255);
        snprintf(buffer, 64, TextFormatString, r, g, b, a);
    }

    // Add text to drawlist!
    drawList->AddText(pixelCenter - textSize * 0.5f, lineColor, buffer);
    ImGui::SetWindowFontScale(1.0);
}

Arrow::Arrow(int xVectorIndex, int yVectorIndex, ImVec2 lineScale)
    : VectorIndex_x(xVectorIndex), VectorIndex_y(yVectorIndex), LineScale(lineScale)
{
}

Arrow &Arrow::UsePreset(Preset preset)
{
    switch (preset)
    {
        default:
        case Preset::NormalMap:
            VectorIndex_x = 0;
            VectorIndex_y = 1;
            LineScale = ImVec2(1, -1);
            ZeroPoint = ImVec2(128.0f / 255, 128.0f / 255);
            break;
        case Preset::NormalizedFloat:
            VectorIndex_x = 0;
            VectorIndex_y = 1;
            LineScale = ImVec2(0.5f, -0.5f);
            ZeroPoint = ImVec2(0, 0);
            break;
    }
    return *this;
}

void Arrow::DrawAnnotation(ImDrawList *drawList, ImVec2 texel, Transform2D texelsToPixels, ImVec4 value)
{
    const float arrowHeadScale = 0.35f;
    const ImU32 lineColor = 0xFFFFFFFF;
    float *vecPtr = &value.x;

    // Draw an arrow!
    ImVec2 lineDir = (ImVec2(vecPtr[VectorIndex_x], vecPtr[VectorIndex_y]) - ZeroPoint) * LineScale;
    ImVec2 lineStart = texel;
    ImVec2 lineEnd = lineStart + lineDir;

    ImVec2 arrowHead1 = ImVec2(lineDir.x - lineDir.y,  lineDir.x + lineDir.y) * -arrowHeadScale;
    ImVec2 arrowHead2 = ImVec2(lineDir.x + lineDir.y, -lineDir.x + lineDir.y) * -arrowHeadScale;

    DrawAnnotationLine(drawList, lineStart, lineEnd, texelsToPixels, lineColor);
    DrawAnnotationLine(drawList, lineEnd, lineEnd + arrowHead1, texelsToPixels, lineColor);
    DrawAnnotationLine(drawList, lineEnd, lineEnd + arrowHead2, texelsToPixels, lineColor);
}

void DrawAnnotationLine(ImDrawList *drawList, ImVec2 fromTexel, ImVec2 toTexel, Transform2D texelsToPixels, ImU32 color)
{
    ImVec2 lineFrom = texelsToPixels * fromTexel;
    ImVec2 lineTo = texelsToPixels * toTexel;
    drawList->AddLine(lineFrom, lineTo, color, 1.0f);
}

void BackEnd_SetShader(const ImDrawList *, const ImDrawCmd *, const Inspector *inspector)
{
    // TODO::Dicky BackEnd_SetShader
}

bool BackEnd_GetData(Inspector *inspector, ImTextureID texture, int /*x*/, int /*y*/, int /*width*/, int /*height*/, BufferDesc *bufferDesc)
{
    int ret = -1;
    if (!inspector || !texture)
        return false;
    int width = ImGui::ImGetTextureWidth(texture);
    int height = ImGui::ImGetTextureHeight(texture);
    int channels = 4; // TODO::Dicky need check
    size_t bufferSize = sizeof(uint8_t) * width * height * channels;
    bufferDesc->Data_uint8_t = (uint8_t *)GetBuffer(inspector, bufferSize);
    void *data               = (void *)bufferDesc->Data_uint8_t;

    if (data == NULL)
        return false;

    bufferDesc->BufferByteSize = bufferSize;
    bufferDesc->Red            = 0; // Data is packed such that red channel is first
    bufferDesc->Green          = 1; // then green, then blue, the alpha. Hence, 0,1,2,3
    bufferDesc->Blue           = 2; // for these channel order values.
    bufferDesc->Alpha          = 3;
    bufferDesc->ChannelCount   = 4; // RGBA
    bufferDesc->LineStride     = (int)inspector->TextureSize.x * channels;
    bufferDesc->Stride         = 4; // No padding between each RGBA texel
    bufferDesc->StartX         = 0; // We queried the whole texture
    bufferDesc->StartY         = 0;
    bufferDesc->Width          = width;
    bufferDesc->Height         = height;

    ret = ImGui::ImGetTextureData(texture, data);
    if (ret != 0)
        return false;
    return true;
}
} // namespace ImGuiTexInspect
