/*
MIT License

Copyright (c) 2023 Stephane Cuillerdier (aka Aiekick)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "ImCoolbar.h" // modify by Dicky
#include "imgui_internal.h"
#include <cmath>
#include <vector>
#include <array>

#define ICB_PREFIX "ICB"
//#define ENABLE_IMCOOLBAR_DEBUG

#ifdef _MSC_VER
#include <Windows.h>
#define ICB_DEBUG_BREAK       \
    if (IsDebuggerPresent()) \
    __debugbreak()
#else
#define ICB_DEBUG_BREAK IM_ASSERT(0) // modify by Dicky
#endif

#define BREAK_ON_KEY(KEY)         \
    if (ImGui::IsKeyPressed(KEY)) \
    ICB_DEBUG_BREAK

static float bubbleEffect(const float vValue, const float vStength) {
    return pow(cos(vValue * IM_PI * vStength), 12.0f);
}

// https://codesandbox.io/s/motion-dock-forked-hs4p8d?file=/src/Dock.tsx
static float getHoverSize(const float vValue, const float vNormalSize, const float vHoveredSize, const float vStength, const float vScale) {
    return ImClamp(vNormalSize + (vHoveredSize - vNormalSize) * bubbleEffect(vValue, vStength) * vScale, vNormalSize, vHoveredSize);
}

static bool isWindowHovered(ImGuiWindow* vWindow) {
    return ImGui::IsMouseHoveringRect(vWindow->Rect().Min, vWindow->Rect().Max);
}

static float getBarSize(ImGuiWindow* vWindow, const float vNormalSize, const float vHoveredSize, const float vScale) {
    ImGuiContext& g = *GImGui;
    return vNormalSize + vHoveredSize * vScale;
}

static float getChannel(const ImVec2& vVec, const ImCoolBarFlags vCBFlags) {
    if (vCBFlags & ImCoolBarFlags_Horizontal) {
        return vVec.x;
    }
    return vVec.y;
}

static float getChannelInv(const ImVec2& vVec, const ImCoolBarFlags vCBFlags) {
    if (vCBFlags & ImCoolBarFlags_Horizontal) {
        return vVec.y;
    }
    return vVec.x;
}

IMGUI_API bool ImGui::BeginCoolBar(const char* vLabel, ImCoolBarFlags vCBFlags, const ImCoolBarConfig& vConfig, ImGuiWindowFlags vFlags) {
    ImGuiWindowFlags flags =                   //
        vFlags |                               //
        ImGuiWindowFlags_NoTitleBar |          //
        ImGuiWindowFlags_NoScrollbar |         //
        ImGuiWindowFlags_AlwaysAutoResize |    //
        ImGuiWindowFlags_NoCollapse |          //
        ImGuiWindowFlags_NoMove |              //
        ImGuiWindowFlags_NoSavedSettings |     //
#ifndef ENABLE_IMCOOLBAR_DEBUG                 //
        ImGuiWindowFlags_NoBackground |        //
#endif                                         //
        ImGuiWindowFlags_NoFocusOnAppearing |  //
        ImGuiWindowFlags_DockNodeHost |        //
        ImGuiWindowFlags_NoDocking;            //
    bool res = ImGui::Begin(vLabel, nullptr, flags);
    if (!res) {
        ImGui::End();
    } else {
        // Can be Horizontal or Vertical, not both
        // this working atm, just because we have only H or V flags
        IM_ASSERT(                                                                    //
            ((vCBFlags & ImCoolBarFlags_Horizontal) == ImCoolBarFlags_Horizontal) ||  //
            ((vCBFlags & ImCoolBarFlags_Vertical) == ImCoolBarFlags_Vertical)         //
        );

        ImGuiContext& g = *GImGui;
        ImGuiWindow* window_ptr = GetCurrentWindow();
        window_ptr->StateStorage.SetVoidPtr(window_ptr->GetID(ICB_PREFIX "Type"), (void*)"ImCoolBar");
        window_ptr->StateStorage.SetInt(window_ptr->GetID(ICB_PREFIX "ItemIdx"), 0);
        window_ptr->StateStorage.SetInt(window_ptr->GetID(ICB_PREFIX "Flags"), vCBFlags);
        window_ptr->StateStorage.SetFloat(window_ptr->GetID(ICB_PREFIX "Anchor"), getChannelInv(vConfig.anchor, vCBFlags));
        window_ptr->StateStorage.SetFloat(window_ptr->GetID(ICB_PREFIX "NormalSize"), vConfig.normal_size);
        window_ptr->StateStorage.SetFloat(window_ptr->GetID(ICB_PREFIX "HoveredSize"), vConfig.hovered_size);
        window_ptr->StateStorage.SetFloat(window_ptr->GetID(ICB_PREFIX "EffectStrength"), vConfig.effect_strength);

        const auto bar_size = window_ptr->ContentSize + ImGui::GetStyle().WindowPadding * 2.0f;
        const auto viewport_ptr = window_ptr->Viewport;
        const auto new_pos = viewport_ptr->Pos + (viewport_ptr->Size - bar_size) * vConfig.anchor;
        ImGui::SetWindowPos(new_pos);

        const auto anim_scale_id = window_ptr->GetID(ICB_PREFIX "AnimScale");
        float anim_scale = window_ptr->StateStorage.GetFloat(anim_scale_id);
        if (isWindowHovered(window_ptr)) {
            if (anim_scale < 1.0f) {
                anim_scale += vConfig.anim_step;
            }
        } else {
            if (anim_scale > 0.0f) {
                anim_scale -= vConfig.anim_step;
            }
        }
        window_ptr->StateStorage.SetFloat(anim_scale_id, ImClamp(anim_scale, 0.0f, 1.0f));
    }

    return res;
}

IMGUI_API void ImGui::EndCoolBar() {
    ImGui::End();
}

IMGUI_API bool ImGui::CoolBarItem() {
    ImGuiWindow* window_ptr = GetCurrentWindow();
    if (window_ptr->SkipItems)
        return false;

    const auto item_index_id = window_ptr->GetID(ICB_PREFIX "ItemIdx");
    const auto idx = window_ptr->StateStorage.GetInt(item_index_id);
    const auto coolbar_item_id = window_ptr->GetID(window_ptr->ID + idx + 1);
    const auto current_item_size = window_ptr->StateStorage.GetFloat(coolbar_item_id);
    const auto flags             = window_ptr->StateStorage.GetInt(window_ptr->GetID(ICB_PREFIX "Flags"));
    const auto anim_scale        = window_ptr->StateStorage.GetFloat(window_ptr->GetID(ICB_PREFIX "AnimScale"));
    const auto normal_size       = window_ptr->StateStorage.GetFloat(window_ptr->GetID(ICB_PREFIX "NormalSize"));
    const auto hovered_size      = window_ptr->StateStorage.GetFloat(window_ptr->GetID(ICB_PREFIX "HoveredSize"));
    const auto effect_strength = window_ptr->StateStorage.GetFloat(window_ptr->GetID(ICB_PREFIX "EffectStrength"));
    const auto last_mouse_pos_id = window_ptr->GetID(ICB_PREFIX "LastMousePos");
    auto last_mouse_pos = window_ptr->StateStorage.GetFloat(last_mouse_pos_id);

    assert(normal_size > 0.0f);

    if (flags & ImCoolBarFlags_Horizontal) {
        if (idx) {
            ImGui::SameLine();
        }
    }

    float current_size = normal_size;
    ImGuiContext& g = *GImGui;

    if (isWindowHovered(window_ptr)) {
        last_mouse_pos = getChannel(ImGui::GetMousePos(), flags);
    }

    if (anim_scale > 0.0f) {
        const auto csp = getChannel(ImGui::GetCursorScreenPos(), flags);
        const auto ws = getChannel(window_ptr->Size, flags);
        const auto wp = getChannel(g.Style.WindowPadding, flags);
        const float btn_center = csp + current_item_size * 0.5f;
        const float diff_pos = (last_mouse_pos - btn_center) / ws;
        current_size = getHoverSize(diff_pos, normal_size, hovered_size, effect_strength, anim_scale);
        const float anchor = window_ptr->StateStorage.GetFloat(window_ptr->GetID(ICB_PREFIX "Anchor"));
        const float bar_height = getBarSize(window_ptr, normal_size, hovered_size, anim_scale);
        float btn_offset = (bar_height - current_size) * anchor + wp;
        if (flags & ImCoolBarFlags_Horizontal) {
            ImGui::SetCursorPosY(btn_offset);
        } else if (flags & ImCoolBarFlags_Vertical) {
            ImGui::SetCursorPosX(btn_offset);
        }
    }

    BREAK_ON_KEY(ImGuiKey_D);
    window_ptr->StateStorage.SetInt(item_index_id, idx + 1);
    window_ptr->StateStorage.SetFloat(coolbar_item_id, current_size);
    window_ptr->StateStorage.SetFloat(last_mouse_pos_id, last_mouse_pos);
    window_ptr->StateStorage.SetFloat(window_ptr->GetID(ICB_PREFIX "ItemCurrentSize"), current_size);
    window_ptr->StateStorage.SetFloat(window_ptr->GetID(ICB_PREFIX "ItemCurrentScale"), current_size / normal_size);

    return true;
}

IMGUI_API float ImGui::GetCoolBarItemWidth() {
    ImGuiWindow* window_ptr = GetCurrentWindow();
    if (window_ptr->SkipItems) {
        return 0.0f;
    }
    return window_ptr->StateStorage.GetFloat(  //
        window_ptr->GetID(ICB_PREFIX "ItemCurrentSize"));
}

IMGUI_API float ImGui::GetCoolBarItemScale() {
    ImGuiWindow* window_ptr = GetCurrentWindow();
    if (window_ptr->SkipItems) {
        return 0.0f;
    }

    return window_ptr->StateStorage.GetFloat(  //
        window_ptr->GetID(ICB_PREFIX "ItemCurrentScale"));
}

IMGUI_API void ImGui::ShowCoolBarMetrics(bool* vOpened) {
    if (ImGui::Begin("ImCoolBar Metrics", vOpened)) {
        ImGuiContext& g = *GImGui;
        for (auto* window_ptr : g.Windows) {
            const char* type = (const char*)window_ptr->StateStorage.GetVoidPtr(window_ptr->GetID(ICB_PREFIX "Type"));
            if (type != nullptr && strcmp(type, "ImCoolBar") == 0) {
                if (!TreeNode(window_ptr, "ImCoolBar %s", window_ptr->Name)) {
                    continue;
                }

                const auto flags_id = window_ptr->GetID(ICB_PREFIX "Flags");
                const auto anchor_id = window_ptr->GetID(ICB_PREFIX "Anchor");
                const auto anim_scale_id = window_ptr->GetID(ICB_PREFIX "AnimScale");
                const auto item_index_id = window_ptr->GetID(ICB_PREFIX "ItemIdx");
                const auto normal_size_id = window_ptr->GetID(ICB_PREFIX "NormalSize");
                const auto hovered_size_id = window_ptr->GetID(ICB_PREFIX "HoveredSize");
                const auto effect_strength_id = window_ptr->GetID(ICB_PREFIX "EffectStrength");
                const auto item_current_size_id = window_ptr->GetID(ICB_PREFIX "ItemCurrentSize");
                const auto item_current_scale_id = window_ptr->GetID(ICB_PREFIX "ItemCurrentScale");

                const auto flags = window_ptr->StateStorage.GetInt(flags_id);
                const auto anchor = window_ptr->StateStorage.GetFloat(anchor_id);
                const auto max_idx = window_ptr->StateStorage.GetInt(item_index_id);
                const auto anim_scale = window_ptr->StateStorage.GetFloat(anim_scale_id);
                const auto normal_size = window_ptr->StateStorage.GetFloat(normal_size_id);
                const auto hovered_size = window_ptr->StateStorage.GetFloat(hovered_size_id);
                const auto effect_strength = window_ptr->StateStorage.GetFloat(effect_strength_id);
                const auto item_current_size = window_ptr->StateStorage.GetFloat(item_current_size_id);
                const auto item_current_scale = window_ptr->StateStorage.GetFloat(item_current_scale_id);

#define SetColumnLabel(a, fmt, v) \
    ImGui::TableNextColumn();     \
    ImGui::Text("%s", a);         \
    ImGui::TableNextColumn();     \
    ImGui::Text(fmt, v);          \
    ImGui::TableNextRow()

                if (ImGui::BeginTable("CoolbarDebugDatas", 2)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    SetColumnLabel("MaxIdx ", "%i", max_idx);
                    SetColumnLabel("Anchor ", "%f", anchor);
                    SetColumnLabel("AnimScale ", "%f", anim_scale);
                    SetColumnLabel("NormalSize ", "%f", normal_size);
                    SetColumnLabel("HoveredSize ", "%f", hovered_size);
                    SetColumnLabel("EffectStrength ", "%f", effect_strength);
                    SetColumnLabel("ItemCurrentSize ", "%f", item_current_size);
                    SetColumnLabel("ItemCurrentScale ", "%f", item_current_scale);

                    ImGui::TableNextColumn();
                    ImGui::Text("%s", "Flags ");
                    ImGui::TableNextColumn();
                    if (flags & ImCoolBarFlags_None) {
                        ImGui::Text("None");
                    }
                    if (flags & ImCoolBarFlags_Vertical) {
                        ImGui::Text("Vertical");
                    }
                    if (flags & ImCoolBarFlags_Horizontal) {
                        ImGui::Text("Horizontal");
                    }
                    ImGui::TableNextRow();

                    for (int idx = 0; idx < max_idx; ++idx) {
                        const auto coolbar_item_id = window_ptr->GetID(window_ptr->ID + idx + 1);
                        const auto current_item_size = window_ptr->StateStorage.GetFloat(coolbar_item_id);
                        ImGui::TableNextColumn();
                        ImGui::Text("Item %i Size ", idx);
                        ImGui::TableNextColumn();
                        ImGui::Text("%f", current_item_size);
                        ImGui::TableNextRow();
                    }

                    ImGui::EndTable();
                }

#undef SetColumnLabel
                TreePop();
            } 
        }
    }
    ImGui::End();
}
