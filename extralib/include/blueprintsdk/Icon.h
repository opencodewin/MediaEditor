#pragma once
#include <imgui.h>

namespace BluePrint
{

enum class IconType: ImU32 { Flow, FlowDown, Circle, Square, Grid, RoundSquare, BracketSquare, Diamond, Bracket };

// Draws an icon into specified draw list.
void DrawIcon(ImDrawList* drawList, const ImVec2& a, const ImVec2& b, IconType type, bool filled, ImU32 color, ImU32 innerColor);

// Icon widget
void Icon(const ImVec2& size, IconType type, bool filled, bool exported, bool publicized, const ImVec4& color = ImVec4(1, 1, 1, 1), const ImVec4& innerColor = ImVec4(0, 0, 0, 0));

} // namespace BluePrint