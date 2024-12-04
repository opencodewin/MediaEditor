#include <Icon.h>
#include <imgui_internal.h>

static void bezier_arc(ImVec2 center, ImVec2 start, ImVec2 end, ImVec2& c1, ImVec2 & c2)
{
    float ax = start[0] - center[0];
    float ay = start[1] - center[1];
    float bx = end[0] - center[0];
    float by = end[1] - center[1];
    float q1 = ax * ax + ay * ay;
    float q2 = q1 + ax * bx + ay * by;
    float k2 = (4.0 / 3.0) * (sqrt(2.0 * q1 * q2) - q2) / (ax * by - ay * bx);
    c1 = ImVec2(center[0] + ax - k2 * ay, center[1] + ay + k2 * ax);
    c2 = ImVec2(center[0] + bx + k2 * by, center[1] + by - k2 * bx);
}

static void draw_arc1(ImDrawList* draw_list, ImVec2 center, float radius, float start_angle, float end_angle, float thickness, ImU32 color, int num_segments)
{
    ImVec2 start = {center[0] + cos(start_angle) * radius, center[1] + sin(start_angle) * radius};
    ImVec2 end = {center[0] + cos(end_angle) * radius, center[1] + sin(end_angle) * radius};
    ImVec2 c1, c2;
    bezier_arc(center, start, end, c1, c2);
    draw_list->AddBezierCubic(start, c1, c2, end, color, thickness, num_segments);
}

static void draw_arc(ImDrawList* draw_list, ImVec2 center, float radius, float start_angle, float end_angle, float thickness, ImU32 color, int num_segments, int8_t bezier_count)
{
    float overlap = thickness * radius * 0.00001 * IM_PI;
    float delta = end_angle - start_angle;
    float bez_step = 1.0 / (float)bezier_count;
    float mid_angle = start_angle + overlap;
    for (int i = 0; i < bezier_count - 1; i++)
    {
        float mid_angle2 = delta * bez_step + mid_angle;
        draw_arc1(draw_list, center, radius, mid_angle - overlap, mid_angle2 + overlap, thickness, color, num_segments);
        mid_angle = mid_angle2;
    }
    draw_arc1(draw_list, center, radius, mid_angle - overlap, end_angle, thickness, color, num_segments);
}

namespace BluePrint
{
void DrawIcon(ImDrawList* drawList, const ImVec2& a, const ImVec2& b, IconType type, bool filled, ImU32 color, ImU32 innerColor)
{
            auto rect           = ImRect(a, b);
            auto rect_x         = rect.Min.x;
            auto rect_y         = rect.Min.y;
            auto rect_w         = rect.Max.x - rect.Min.x;
            auto rect_h         = rect.Max.y - rect.Min.y;
            auto rect_center_x  = (rect.Min.x + rect.Max.x) * 0.5f;
            auto rect_center_y  = (rect.Min.y + rect.Max.y) * 0.5f;
            auto rect_center    = ImVec2(rect_center_x, rect_center_y);
            auto rect_center_l  = ImVec2(rect_center_x - 2.0f, rect_center_y);
    const   auto outline_scale  = ImMin(rect_w, rect_h) / 24.0f;
    const   auto extra_segments = static_cast<int>(2 * outline_scale); // for full circle

    if (type == IconType::Flow || type == IconType::FlowDown)
    {
        auto tr = [type, rect_center_x, rect_center_y](const ImVec2& point) -> ImVec2
        {
            ImVec2 result = point;
            if (type == IconType::FlowDown)
            {
                result = point - ImVec2(rect_center_x, rect_center_y);
                result = ImVec2(result.y, result.x);
                result = result + ImVec2(rect_center_x, rect_center_y);
            }
            return result;
        };

        const auto origin_scale = rect_w / 24.0f;

        const auto offset_x  = 1.0f * origin_scale;
        const auto offset_y  = 0.0f * origin_scale;
        const auto margin     = (filled ? 2.0f : 2.0f) * origin_scale;
        const auto rounding   = 0.1f * origin_scale;
        const auto tip_round  = 0.7f; // percentage of triangle edge (for tip)
        //const auto edge_round = 0.7f; // percentage of triangle edge (for corner)
        const auto canvas = ImRect(
            tr(ImVec2(rect.Min.x + margin + offset_x, rect.Min.y + margin + offset_y)),
            tr(ImVec2(rect.Max.x - margin + offset_x, rect.Max.y - margin + offset_y)));
        const auto canvas_x = canvas.Min.x;
        const auto canvas_y = canvas.Min.y;
        const auto canvas_w = canvas.Max.x - canvas.Min.x;
        const auto canvas_h = canvas.Max.y - canvas.Min.y;

        const auto left   = canvas_x + canvas_w            * 0.5f * 0.3f;
        const auto right  = canvas_x + canvas_w - canvas_w * 0.5f * 0.3f;
        const auto top    = canvas_y + canvas_h            * 0.5f * 0.2f;
        const auto bottom = canvas_y + canvas_h - canvas_h * 0.5f * 0.2f;
        const auto center_x = (left + right) * 0.5f;
        const auto center_y = (top + bottom) * 0.5f;
        //const auto angle = AX_PI * 0.5f * 0.5f * 0.5f;

        const auto tip_top    = ImVec2(canvas_x + canvas_w * 0.5f, top);
        const auto tip_right  = ImVec2(right, center_y);
        const auto tip_bottom = ImVec2(canvas_x + canvas_w * 0.5f, bottom);

        drawList->PathLineTo(tr(ImVec2(left, top) + ImVec2(0, rounding)));
        drawList->PathBezierCubicCurveTo(
            tr(ImVec2(left, top)),
            tr(ImVec2(left, top)),
            tr(ImVec2(left, top) + ImVec2(rounding, 0)));
        drawList->PathLineTo(tr(tip_top));
        drawList->PathLineTo(tr(tip_top + (tip_right - tip_top) * tip_round));
        drawList->PathBezierCubicCurveTo(
            tr(tip_right),
            tr(tip_right),
            tr(tip_bottom + (tip_right - tip_bottom) * tip_round));
        drawList->PathLineTo(tr(tip_bottom));
        drawList->PathLineTo(tr(ImVec2(left, bottom) + ImVec2(rounding, 0)));
        drawList->PathBezierCubicCurveTo(
            tr(ImVec2(left, bottom)),
            tr(ImVec2(left, bottom)),
            tr(ImVec2(left, bottom) - ImVec2(0, rounding)));

        if (type == IconType::FlowDown)
        {
            // reverse order of vertices in path, so PathFillConvex will emit
            // proper AA fringe
            auto first = drawList->_Path.Data;
            auto last = drawList->_Path.Data + drawList->_Path.Size;

            while ((first != last) && (first != --last))
            {
                ImSwap(*first, *last);
                ++first;
            }
        }

        if (!filled)
        {
            if (innerColor & 0xFF000000)
                drawList->AddConvexPolyFilled(drawList->_Path.Data, drawList->_Path.Size, innerColor);

            drawList->PathStroke(color, true, 2.0f * outline_scale);
        }
        else
            drawList->PathFillConvex(color);
    }
    else
    {
        auto triangleStart = rect_center_x + 0.32f * rect_w;

        auto rect_offset = -static_cast<int>(rect_w * 0.25f * 0.25f);

        rect.Min.x    += rect_offset;
        rect.Max.x    += rect_offset;
        rect_x        += rect_offset;
        rect_center_x += rect_offset * 0.5f;
        rect_center.x += rect_offset * 0.5f;

        if (type == IconType::Circle)
        {
            const auto c = rect_center;

            if (!filled)
            {
                const auto r = 0.5f * rect_w / 2.0f - 0.5f;

                if (innerColor & 0xFF000000)
                    drawList->AddCircleFilled(c, r, innerColor, 12 + extra_segments);
                drawList->AddCircle(c, r, color, 12 + extra_segments, 2.0f * outline_scale);
            }
            else
            {
                drawList->AddCircleFilled(c, 0.5f * rect_w / 2.0f, color, 12 + extra_segments);
            }
        }

        if (type == IconType::Square)
        {
            if (filled)
            {
                const auto r  = 0.5f * rect_w / 2.0f;
                const auto p0 = rect_center - ImVec2(r, r);
                const auto p1 = rect_center + ImVec2(r, r);

                drawList->AddRectFilled(p0, p1, color, 0, ImDrawFlags_RoundCornersAll);
            }
            else
            {
                const auto r = 0.5f * rect_w / 2.0f - 0.5f;
                const auto p0 = rect_center - ImVec2(r, r);
                const auto p1 = rect_center + ImVec2(r, r);

                if (innerColor & 0xFF000000)
                    drawList->AddRectFilled(p0, p1, innerColor, 0, ImDrawFlags_RoundCornersAll);

                drawList->AddRect(p0, p1, color, 0, extra_segments, 2.0f * outline_scale);
            }
        }

        if (type == IconType::BracketSquare)
        {
            const auto r  = 0.5f * rect_w / 2.0f;
            const auto w = ceilf(r / 3.0f);
            const auto s = r / 1.5f;
            const auto p00 = rect_center - ImVec2(r, r);
            const auto p00w = p00 + ImVec2(s, 0);
            const auto p01 = p00 + ImVec2(0, 2 * r);
            const auto p01w = p01 + ImVec2(s, 0);
            const auto p10 = p00 + ImVec2(2 * r, 0);
            const auto p10w = p10 - ImVec2(s, 0);
            const auto p11 = rect_center + ImVec2(r, r);
            const auto p11w = p11 - ImVec2(s, 0);
            drawList->AddLine(p00, p01, color, 2.0f * outline_scale);
            drawList->AddLine(p10, p11, color, 2.0f * outline_scale);
            drawList->AddLine(p00, p00w, color, 2.0f * outline_scale);
            drawList->AddLine(p01, p01w, color, 2.0f * outline_scale);
            drawList->AddLine(p10, p10w, color, 2.0f * outline_scale);
            drawList->AddLine(p11, p11w, color, 2.0f * outline_scale);
            if (filled)
            {
                const auto pc0 = rect_center - ImVec2(1, 1);
                const auto pc1 = rect_center + ImVec2(2, 2);
                drawList->AddRectFilled(pc0, pc1, color, 0, ImDrawFlags_RoundCornersAll);
            }

            triangleStart = p11.x + w + 1.0f / 24.0f * rect_w;
        }

        if (type == IconType::Grid)
        {
            const auto r = 0.5f * rect_w / 2.0f;
            const auto w = ceilf(r / 3.0f);

            const auto baseTl = ImVec2(floorf(rect_center_x - w * 2.5f), floorf(rect_center_y - w * 2.5f));
            const auto baseBr = ImVec2(floorf(baseTl.x + w), floorf(baseTl.y + w));

            auto tl = baseTl;
            auto br = baseBr;
            for (int i = 0; i < 3; ++i)
            {
                tl.x = baseTl.x;
                br.x = baseBr.x;
                drawList->AddRectFilled(tl, br, color);
                tl.x += w * 2;
                br.x += w * 2;
                if (i != 1 || filled)
                    drawList->AddRectFilled(tl, br, color);
                tl.x += w * 2;
                br.x += w * 2;
                drawList->AddRectFilled(tl, br, color);

                tl.y += w * 2;
                br.y += w * 2;
            }

            triangleStart = br.x + w + 1.0f / 24.0f * rect_w;
        }

        if (type == IconType::Bracket)
        {
            ImVec2 l_center = ImVec2(rect_center.x + 1, rect_center.y);
            ImVec2 r_center = ImVec2(rect_center.x - 1, rect_center.y);
            draw_arc(drawList, l_center, rect_w / 3, -4.14, -2.14, 1.5, color, 16, 2);
            draw_arc(drawList, r_center, rect_w / 3, -1.0, 1.0, 1.5, color, 16, 2);
            if (filled)
            {
                drawList->AddCircleFilled(rect_center, rect_w / 8.0f, color, 12 + extra_segments);
            }
            triangleStart += 1.0;
        }

        if (type == IconType::RoundSquare)
        {
            if (filled)
            {
                const auto r  = 0.5f * rect_w / 2.0f;
                const auto cr = r * 0.5f;
                const auto p0 = rect_center - ImVec2(r, r);
                const auto p1 = rect_center + ImVec2(r, r);

                drawList->AddRectFilled(p0, p1, color, cr, ImDrawFlags_RoundCornersAll);
            }
            else
            {
                const auto r = 0.5f * rect_w / 2.0f - 0.5f;
                const auto cr = r * 0.5f;
                const auto p0 = rect_center - ImVec2(r, r);
                const auto p1 = rect_center + ImVec2(r, r);

                if (innerColor & 0xFF000000)
                    drawList->AddRectFilled(p0, p1, innerColor, cr, ImDrawFlags_RoundCornersAll);

                drawList->AddRect(p0, p1, color, cr, ImDrawFlags_RoundCornersAll, 2.0f * outline_scale);
            }
        }
        else if (type == IconType::Diamond)
        {
            if (filled)
            {
                const auto r = 0.607f * rect_w / 2.0f;
                const auto c = rect_center;

                drawList->PathLineTo(c + ImVec2( 0, -r));
                drawList->PathLineTo(c + ImVec2( r,  0));
                drawList->PathLineTo(c + ImVec2( 0,  r));
                drawList->PathLineTo(c + ImVec2(-r,  0));
                drawList->PathFillConvex(color);
            }
            else
            {
                const auto r = 0.607f * rect_w / 2.0f - 0.5f;
                const auto c = rect_center;

                drawList->PathLineTo(c + ImVec2( 0, -r));
                drawList->PathLineTo(c + ImVec2( r,  0));
                drawList->PathLineTo(c + ImVec2( 0,  r));
                drawList->PathLineTo(c + ImVec2(-r,  0));

                if (innerColor & 0xFF000000)
                    drawList->AddConvexPolyFilled(drawList->_Path.Data, drawList->_Path.Size, innerColor);

                drawList->PathStroke(color, true, 2.0f * outline_scale);
            }
        }
        else
        {
            const auto triangleTip = triangleStart + rect_w * (0.45f - 0.32f);
            drawList->AddTriangleFilled(
                ImVec2(ceilf(triangleTip), rect_y + rect_h * 0.5f),
                ImVec2(triangleStart, rect_center_y + 0.15f * rect_h),
                ImVec2(triangleStart, rect_center_y - 0.15f * rect_h),
                color);
        }
    }
}

void Icon(const ImVec2& size, IconType type, bool filled, bool exported, bool publicized, const ImVec4& color/* = ImVec4(1, 1, 1, 1)*/, const ImVec4& innerColor/* = ImVec4(0, 0, 0, 0)*/)
{
    if (ImGui::IsRectVisible(size))
    {
        auto cursorPos = ImGui::GetCursorScreenPos();
        auto drawList  = ImGui::GetWindowDrawList();
        DrawIcon(drawList, cursorPos, cursorPos + size, type, filled, ImColor(color), ImColor(innerColor));
    }

    ImGui::Dummy(size);

    if (exported || publicized)
    {
        auto drawList  = ImGui::GetWindowDrawList();
        // Put cursor on the left side of the Pin
        auto itemRectMin = ImGui::GetItemRectMin();
        auto itemRectMax = ImGui::GetItemRectMax();
        auto itemRectSize = ImGui::GetItemRectSize();
        auto markerMin = itemRectMin + ImVec2(0.0f, itemRectSize.y / 8);
        auto markerMax = itemRectMax - ImVec2(itemRectSize.x * 3 / 4, itemRectSize.y / 8);
        if (exported && !publicized)
            drawList->AddRect(markerMin, markerMax, IM_COL32(255, 0, 128, 255));
        else if (!exported && publicized)
            drawList->AddRect(markerMin, markerMax, IM_COL32(0, 255, 128, 255));
        else
            drawList->AddRectFilled(markerMin, markerMax, IM_COL32(255, 0, 128, 255), 0, 0);
    }
}

} // namespace BluePrint