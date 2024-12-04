#include "imgui_curve.h"
#include "imgui_spline.h"
void ImGui::ShowCurveDemo()
{
    ImGuiIO& io = ImGui::GetIO();
    bool reset = false;
    static bool value_limited = true;
    static bool scroll_v = true;
    static bool move_curve = true;
    static bool keep_begin_end = false;
    static bool dock_begin_end = false;
    static ImGui::KeyPointEditor rampEdit(IM_COL32(0, 0, 0, 255), IM_COL32(32, 32, 32, 128));
    char ** curve_type_list = nullptr;
    auto curve_type_count = ImGui::ImCurveEdit::GetCurveTypeName(curve_type_list);
    float table_width = 300;
    auto size_x = ImGui::GetWindowSize().x - table_width - 60;
    if (rampEdit.GetCurveCount() <= 0)
    {
        auto index_1 = rampEdit.AddCurveByDim("key1", ImGui::ImCurveEdit::Smooth, IM_COL32(255, 0, 0, 255), true, ImGui::ImCurveEdit::DIM_X, -1, 1, 0);
        rampEdit.AddPointByDim(index_1, ImVec2(size_x * 0.f, 0), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        rampEdit.AddPointByDim(index_1, ImVec2(size_x * 0.25f, 0.610f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        rampEdit.AddPointByDim(index_1, ImVec2(size_x * 0.5f, 1.0f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        rampEdit.AddPointByDim(index_1, ImVec2(size_x * 0.75f, 0.610f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        rampEdit.AddPointByDim(index_1, ImVec2(size_x * 1.f, 0.f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        auto index_2 = rampEdit.AddCurveByDim("key2", ImGui::ImCurveEdit::Smooth, IM_COL32(0, 255, 0, 255), true, ImGui::ImCurveEdit::DIM_X, 0, 1, 0);
        rampEdit.AddPointByDim(index_2, ImVec2(size_x * 0.f, 1.f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        rampEdit.AddPointByDim(index_2, ImVec2(size_x * 0.25f, 0.75f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        rampEdit.AddPointByDim(index_2, ImVec2(size_x * 0.5f, 0.5f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        rampEdit.AddPointByDim(index_2, ImVec2(size_x * 0.75f, 0.75f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        rampEdit.AddPointByDim(index_2, ImVec2(size_x * 1.f, 1.f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        auto index_3 = rampEdit.AddCurveByDim("key3", ImGui::ImCurveEdit::Smooth, IM_COL32(0, 0, 255, 255), true, ImGui::ImCurveEdit::DIM_X, 0, 100, 50);
        rampEdit.AddPointByDim(index_3, ImVec2(size_x * 0.f, 0.f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        rampEdit.AddPointByDim(index_3, ImVec2(size_x * 0.25f, 0.05f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        rampEdit.AddPointByDim(index_3, ImVec2(size_x * 0.5f, 0.25f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        rampEdit.AddPointByDim(index_3, ImVec2(size_x * 0.75f, 0.75f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
        rampEdit.AddPointByDim(index_3, ImVec2(size_x * 1.f, 1.f), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, false);
    }
    if (ImGui::Button("Reset##curve_reset"))
        reset = true;
    if (rampEdit.GetMax().x <= 0 || reset)
    {
        rampEdit.SetMax(ImVec4(1.f, 1.f, 1.f, size_x));
        rampEdit.SetMin(ImVec4(0.f, 0.f, 0.f, 0.f));
    }
    ImGui::Checkbox("Value limited", &value_limited); ImGui::SameLine();
    ImGui::Checkbox("Scroll V", &scroll_v); ImGui::SameLine();
    ImGui::Checkbox("Move Curve", &move_curve); ImGui::SameLine();
    ImGui::Checkbox("Keep Begin End", &keep_begin_end); ImGui::SameLine();
    ImGui::Checkbox("Dock Begin End", &dock_begin_end);
    uint32_t curvs_flags = CURVE_EDIT_FLAG_NONE;
    if (value_limited) curvs_flags |= CURVE_EDIT_FLAG_VALUE_LIMITED;
    if (scroll_v) curvs_flags |= CURVE_EDIT_FLAG_SCROLL_V;
    if (move_curve) curvs_flags |= CURVE_EDIT_FLAG_MOVE_CURVE;
    if (keep_begin_end) curvs_flags |= CURVE_EDIT_FLAG_KEEP_BEGIN_END;
    if (dock_begin_end) curvs_flags |= CURVE_EDIT_FLAG_DOCK_BEGIN_END;
    ImVec2 item_pos = ImGui::GetCursorScreenPos();
    float current_pos = -1.f;
    ImGui::ImCurveEdit::Edit(nullptr, &rampEdit, ImVec2(size_x, 300), ImGui::GetID("##bezier_view"), true, current_pos, curvs_flags, nullptr, nullptr);
    if (ImGui::IsItemHovered() && ImGui::BeginTooltip())
    {
        float pos = io.MousePos.x - item_pos.x;
        for (int i = 0; i < rampEdit.GetCurveCount(); i++)
        {
            auto value = rampEdit.GetValueByDim(i, pos, ImGui::ImCurveEdit::DIM_X);
            ImGui::Text("pos=%.0f val=%f", pos, value);
        }
        ImGui::EndTooltip();
    }
    ImGui::SameLine();
    static ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;
    if (ImGui::BeginTable("table_selected", 5, flags, ImVec2(table_width, 300.f)))
    {
        ImGui::TableSetupScrollFreeze(2, 1);
        ImGui::TableSetupColumn("C", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed, 20); // Make the first column not hideable to match our use of TableSetupScrollFreeze()
        ImGui::TableSetupColumn("P", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed, 20);
        ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("T", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableHeadersRow();
        //for (int row = 0; row < selected.size(); row++)
        for (auto ep : rampEdit.selectedPoints)
        {
            ImGui::TableNextRow(ImGuiTableRowFlags_None);
            for (int column = 0; column < 5; column++)
            {
                if (!ImGui::TableSetColumnIndex(column) && column > 0)
                    continue;
                auto point = rampEdit.GetPoint(ep.curveIndex, ep.pointIndex);
                std::string column_id = std::to_string(ep.curveIndex) + "@" + std::to_string(ep.pointIndex);
                switch (column)
                {
                    case 0 : ImGui::Text("%u", ep.curveIndex); break;
                    case 1 : ImGui::Text("%u", ep.pointIndex); break;
                    case 2 :
                        ImGui::PushItemWidth(80);
                        if (ImGui::SliderFloat(("##time@" + column_id).c_str(), &point.t, 0.f, size_x, "%.0f"))
                        {
                            rampEdit.EditPoint(ep.curveIndex, ep.pointIndex, point.val, point.type);
                        }
                        ImGui::PopItemWidth();
                    break;
                    case 3 :
                        ImGui::PushItemWidth(80);
                        if (ImGui::SliderFloat(("##x_pos" + column_id).c_str(), &point.x, 0.f, 1.f, "%.1f"))
                        {
                            rampEdit.EditPoint(ep.curveIndex, ep.pointIndex, point.val, point.type);
                        }
                        ImGui::PopItemWidth();
                    break;
                    case 4 :
                        ImGui::PushItemWidth(100);
                        if (ImGui::Combo(("##type" + column_id).c_str(), (int*)&point.type, curve_type_list, curve_type_count))
                        {
                            rampEdit.EditPoint(ep.curveIndex, ep.pointIndex, point.val, point.type);
                        }
                        ImGui::PopItemWidth();
                    break;
                    default : break;
                }
                
            }
        }
        ImGui::EndTable();
    }
}

void ImGui::ShowSplineDemo()
{
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    static float tension = 0.f;
    ImGui::SliderFloat("Spline tension", &tension, -1.f, 1.f, "%.2f");
    ImGui::Spacing();
    auto pos = ImGui::GetCursorScreenPos() + ImVec2(64, 64);
    auto view_size = ImVec2(512, 512);
    drawList->AddRect(pos, pos + view_size, IM_COL32_WHITE);
    static int dragging_dot = -1;
    static std::vector<ImVec2> points = {
        { view_size.x * 0.f,  view_size.y * 1.f },
        { view_size.x * 0.5f, view_size.y * 0.5f },
        { view_size.x * 1.f,  view_size.y * 0.f }
    };
    int numPoints = points.size();
    //const int numPoints = sizeof(points) / sizeof(points[0]);
    ImGui::ImSpline::cSpline2 splines[numPoints + 1];
    int numSplines = ImGui::ImSpline::SplinesFromPoints(numPoints, points.data(), numPoints + 1, splines, tension);
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton("spline curve view", view_size);
    if (ImGui::IsItemHovered())
    {
        int index;
        float t = FindClosestPoint(io.MousePos - pos, numSplines, splines, &index);
        auto cp = ImGui::ImSpline::Position(splines[index], t);
        drawList->AddCircle(pos + cp, 5, IM_COL32(255, 255, 0, 128), 0, 3);
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            points.insert(points.begin() + index + 1, cp);
        }
    }

    for (int p = 0; p < numSplines; p++)
    {
        ImRect bb = ImGui::ImSpline::ExactBounds(splines[p]);
        for (float t = 0.0; t < 1.0; t+= 0.01)
        {
            ImVec2 ps = ImGui::ImSpline::Position(splines[p], t);
            drawList->AddCircle(pos + ps, 1, IM_COL32(255,0,0,255));
        }
        drawList->AddRect(pos + bb.Min, pos + bb.Max, IM_COL32(0, 255, 0, 128));
    }
    for (auto p = points.begin(); p != points.end();)
    {
        ImRect DotRect = ImRect(pos + *p - ImVec2(4, 4), pos + *p + ImVec2(4, 4));
        bool overDot = DotRect.Contains(io.MousePos);
        if (overDot)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                ImGui::SetNextFrameWantCaptureMouse(true);
                dragging_dot = p - points.begin();
            }
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                p = points.erase(p);
                continue;
            }
            drawList->AddCircle(pos + *p, 5, IM_COL32(255,255,255,255), 0, 3);
        }
        else
        {
            drawList->AddCircle(pos + *p, 5, IM_COL32(255,255,255,128));
        }
        p++;
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && dragging_dot != -1)
    {
        points[dragging_dot].x += io.MouseDelta.x;
        if (points[dragging_dot].x < 0) points[dragging_dot].x = 0;
        if (points[dragging_dot].x > view_size.x) points[dragging_dot].x = view_size.x;
        points[dragging_dot].y += io.MouseDelta.y;
        if (points[dragging_dot].y < 0) points[dragging_dot].y = 0;
        if (points[dragging_dot].y > view_size.y) points[dragging_dot].y = view_size.y;
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        ImGui::SetNextFrameWantCaptureMouse(false);
        dragging_dot = -1;
    }
}
