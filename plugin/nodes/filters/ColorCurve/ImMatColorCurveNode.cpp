#include <UI.h>
#include <imgui_json.h>
#include <imgui_spline.h>
#include <ImVulkanShader.h>
#include <Histogram_vulkan.h>
#include "ColorCurve_vulkan.h"

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct ColorCurveNode final : Node
{
    BP_NODE_WITH_NAME(ColorCurveNode, "Color Curve", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Color")
    ColorCurveNode(BP* blueprint): Node(blueprint)
    {
        m_Name = "Color Curve";
        m_HasCustomLayout = true;
        m_Skippable = true; 
        mMat_curve.create_type(1024, 1, 4, IM_DT_FLOAT32);
        ResetCurve();
    }

    ~ColorCurveNode()
    {
        if (m_histogram) { delete m_histogram; m_histogram = nullptr; }
        if (m_filter) { delete m_filter; m_filter = nullptr; }
    }

    void ResetCurve()
    {
        // clean curve
        mCurve.clear();
        std::vector<ImVec2> y_curve = {{0.f, 1.f}, {1.f, 0.f}};
        std::vector<ImVec2> r_curve = {{0.f, 1.f}, {1.f, 0.f}};
        std::vector<ImVec2> g_curve = {{0.f, 1.f}, {1.f, 0.f}};
        std::vector<ImVec2> b_curve = {{0.f, 1.f}, {1.f, 0.f}};
        mCurve.push_back(y_curve);
        mCurve.push_back(r_curve);
        mCurve.push_back(g_curve);
        mCurve.push_back(b_curve);
        SetCurveMat();
    }

    void ResetCurve(int index)
    {
        if (index < mCurve.size())
        {
            mCurve[index].clear();
            mCurve[index].push_back({0.f, 1.f});
            mCurve[index].push_back({1.f, 0.f});
        }
        SetCurveMat();
    }

    void SetCurveMat()
    {
        if (mCurve.size() == 4 && !mMat_curve.empty())
        {
            auto SetCurve = [](ImGui::ImMat &mat, std::vector<ImVec2> CurvePoints, float tension) {
                int numPoints = CurvePoints.size();
                ImGui::ImSpline::cSpline2 splines[numPoints + 1];
                int numSplines = ImGui::ImSpline::SplinesFromPoints(numPoints, CurvePoints.data(), numPoints + 1, splines, tension);
                int point_index = -1;
                for (int i = 0; i < numSplines; i++)
                {
                    for (float t = 0.0; t <= 1.0; t += 0.01)
                    {
                        ImVec2 ps = ImGui::ImSpline::Position(splines[i], t);
                        ps.x = ImClamp(ps.x, 0.f, 1.f);
                        ps.y = 1.0 - ImClamp(ps.y, 0.f, 1.f);
                        int current_x = ImClamp((int)(ps.x * 1024), 0, 1023);
                        if (point_index == -1)
                        {
                            for (int i = 0; i <= current_x; i++)
                                mat.at<float>(i, 0) = ps.y;
                        }
                        else if (current_x > point_index)
                        {
                            float last_value = mat.at<float>(point_index, 0);
                            float value_step = (ps.y - last_value) / (current_x - point_index);
                            for (int i = point_index + 1; i <= current_x; i++)
                            {
                                float value = last_value + value_step;
                                mat.at<float>(i, 0) = value;
                                last_value = value;
                            }
                        }
                        point_index = current_x;
                    }
                }
                // add missing end point
                if (point_index < 1023)
                {
                    for (int i = point_index + 1; i < 1024; i++)
                    {
                        mat.at<float>(i, 0) = 0;
                    }
                }
            };
            auto y_mat = mMat_curve.channel(0);
            auto r_mat = mMat_curve.channel(1);
            auto g_mat = mMat_curve.channel(2);
            auto b_mat = mMat_curve.channel(3);
            SetCurve(y_mat, mCurve[0], mTension);
            SetCurve(r_mat, mCurve[1], mTension);
            SetCurve(g_mat, mCurve[2], mTension);
            SetCurve(b_mat, mCurve[3], mTension);
        }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_in = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (!mat_in.empty())
        {
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_histogram || !m_filter || gpu != m_device)
            {
                if (m_histogram) { delete m_histogram; m_histogram = nullptr; }
                if (m_filter) { delete m_filter; m_filter = nullptr; }
                m_histogram = new ImGui::Histogram_vulkan(gpu);
                m_filter = new ImGui::ColorCurve_vulkan(gpu);
            }
            if (!m_histogram || !m_filter)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, mMat_curve);
            m_histogram->scope(im_RGB, mMat_histogram, 256, mHistogramScale, mHistogramLog);
            m_MatOut.SetValue(im_RGB);
        }

        return m_Exit;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        changed |= Node::DrawDataTypeSetting("Mat Type:", m_mat_data_type);
        return changed;
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key, bool embedded) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        bool curve_changed = false;
        bool need_update_scope = false;
        ImGuiIO &io = ImGui::GetIO();
        // draw histogram and curve
        ImGui::BeginGroup();
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        std::string label_id = "##mat_color_curve_node@" + std::to_string(m_ID);
        auto pos = ImGui::GetCursorScreenPos();
        static int dragging_dot = -1;
        ImGui::InvisibleButton(label_id.c_str(), scope_view_size);
        ImGui::SetItemUsingMouseWheel();
        if (!mMat_histogram.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::SetCursorScreenPos(pos);
            float height_scale = 1.f;
            float height_offset = 0;
            auto rmat = mMat_histogram.channel(0);
            auto gmat = mMat_histogram.channel(1);
            auto bmat = mMat_histogram.channel(2);
            auto ymat = mMat_histogram.channel(3);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 0.f, 0.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.f, 0.f, 0.f, 0.3f));
            ImGui::PlotLinesEx("##rh", &((float *)rmat.data)[1], mMat_histogram.w - 1, 0, nullptr, 0, mHistogramLog ? 10 : 1000, ImVec2(scope_view_size.x, scope_view_size.y / height_scale), 4, false, mEditIndex == 1);
            ImGui::PopStyleColor(2);
            ImGui::SetCursorScreenPos(pos + ImVec2(0, height_offset));
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 1.f, 0.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.f, 1.f, 0.f, 0.3f));
            ImGui::PlotLinesEx("##gh", &((float *)gmat.data)[1], mMat_histogram.w - 1, 0, nullptr, 0, mHistogramLog ? 10 : 1000, ImVec2(scope_view_size.x, scope_view_size.y / height_scale), 4, false, mEditIndex == 2);
            ImGui::PopStyleColor(2);
            ImGui::SetCursorScreenPos(pos + ImVec2(0, height_offset * 2));
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 0.f, 1.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.f, 0.f, 1.f, 0.3f));
            ImGui::PlotLinesEx("##bh", &((float *)bmat.data)[1], mMat_histogram.w - 1, 0, nullptr, 0, mHistogramLog ? 10 : 1000, ImVec2(scope_view_size.x, scope_view_size.y / height_scale), 4, false, mEditIndex == 3);
            ImGui::PopStyleColor(2);
            ImGui::SetCursorScreenPos(pos + ImVec2(0, height_offset * 3));
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 1.f, 1.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.f, 1.f, 1.f, 0.3f));
            ImGui::PlotLinesEx("##yh", &((float *)ymat.data)[1], mMat_histogram.w - 1, 0, nullptr, 0, mHistogramLog ? 10 : 1000, ImVec2(scope_view_size.x, scope_view_size.y / height_scale), 4, false, mEditIndex == 0);
            ImGui::PopStyleColor(2);
            ImGui::PopStyleColor();
        }
        ImRect scrop_rect = ImRect(pos, pos + scope_view_size);
        ImRect drag_rect = ImRect(scrop_rect.Min - ImVec2(8, 8) , scrop_rect.Max + ImVec2(8, 8));
        draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, IM_COL32(128, 128, 128, 255), 0);

        // draw graticule line        
        float graticule_scale = 1.f;
        auto histogram_step = scope_view_size.x / 10;
        auto histogram_sub_vstep = scope_view_size.x / 50;
        auto histogram_vstep = scope_view_size.y * mHistogramScale * 10 / graticule_scale;
        auto histogram_seg = scope_view_size.y / histogram_vstep / graticule_scale;
        for (int i = 1; i <= 10; i++)
        {
            ImVec2 p0 = scrop_rect.Min + ImVec2(i * histogram_step, 0);
            ImVec2 p1 = scrop_rect.Min + ImVec2(i * histogram_step, scope_view_size.y);
            draw_list->AddLine(p0, p1, IM_COL32(128,  96,   0, 128), 1);
        }
        for (int i = 0; i < histogram_seg; i++)
        {
            ImVec2 pr0 = scrop_rect.Min + ImVec2(0, (scope_view_size.y / graticule_scale) - i * histogram_vstep);
            ImVec2 pr1 = scrop_rect.Min + ImVec2(scope_view_size.x, (scope_view_size.y / graticule_scale) - i * histogram_vstep);
            draw_list->AddLine(pr0, pr1, IM_COL32(128,  96,   0, 128), 1);
        }
        for (int i = 0; i < 50; i++)
        {
            ImVec2 p0 = scrop_rect.Min + ImVec2(i * histogram_sub_vstep, 0);
            ImVec2 p1 = scrop_rect.Min + ImVec2(i * histogram_sub_vstep, 5);
            draw_list->AddLine(p0, p1, IM_COL32(255, 196,   0, 128), 1);
        }

        // spline curve
        if (mCurve.size() == 4)
        {
            int numPoints = mCurve[mEditIndex].size();
            ImGui::ImSpline::cSpline2 splines[numPoints + 1];
            int numSplines = ImGui::ImSpline::SplinesFromPoints(numPoints, mCurve[mEditIndex].data(), numPoints + 1, splines, mTension);
            // draw spline
            for (int p = 0; p < numSplines; p++)
            {
                for (float t = 0.0; t < 1.0; t+= 0.005)
                {
                    ImVec2 ps = ImGui::ImSpline::Position(splines[p], t);
                    ps.x = ImClamp(ps.x, 0.0f, 1.0f);
                    ps.y = ImClamp(ps.y, 0.0f, 1.0f);
                    ps *= scope_view_size;
                    draw_list->AddCircle(pos + ps, 1, mCurveColor[mEditIndex], 0, 0.5);
                }
            }
            // handle spline mouse event
            for (auto p = mCurve[mEditIndex].begin(); p != mCurve[mEditIndex].end();)
            {
                ImVec2 point = *p;
                point *= scope_view_size;
                ImRect DotRect = ImRect(pos + point - ImVec2(4, 4), pos + point + ImVec2(4, 4));
                bool overDot = DotRect.Contains(io.MousePos);
                if (overDot)
                {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    {
                        ImGui::CaptureMouseFromApp(true);
                        dragging_dot = p - mCurve[mEditIndex].begin();
                    }
                    else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                    {
                        p = mCurve[mEditIndex].erase(p);
                        curve_changed = true;
                        continue;
                    }
                    draw_list->AddCircle(pos + point, 5, IM_COL32(255,255,255,255), 0, 3);
                }
                else
                {
                    draw_list->AddCircle(pos + point, 5, IM_COL32(255,255,255,255));
                }
                p++;
            }
            if (drag_rect.Contains(io.MousePos))
            {
                // wheel up/down to change histogram scale
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    mHistogramScale *= 0.9f;
                    if (mHistogramScale < 0.002)
                        mHistogramScale = 0.002;
                    need_update_scope = true;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Scale:%f", mHistogramScale);
                        ImGui::EndTooltip();
                    }
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    mHistogramScale *= 1.1f;
                    if (mHistogramScale > 4.0f)
                        mHistogramScale = 4.0;
                    need_update_scope = true;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Scale:%f", mHistogramScale);
                        ImGui::EndTooltip();
                    }
                }
                
                int index;
                ImVec2 curve_pos = (io.MousePos - pos) / scope_view_size;
                float t = FindClosestPoint(curve_pos, numSplines, splines, &index);
                auto cp = ImGui::ImSpline::Position(splines[index], t);
                auto diff = curve_pos - cp;
                diff *= scope_view_size;
                auto length = sqrtf(diff.x * diff.x + diff.y * diff.y);
                // handle drag point
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && dragging_dot != -1)
                {
                    int point_count = mCurve[mEditIndex].size();
                    float x_min = dragging_dot == 0 ? 0 : mCurve[mEditIndex][dragging_dot - 1].x;
                    float x_max = dragging_dot == point_count - 1 ? 1 : mCurve[mEditIndex][dragging_dot + 1].x;
                    float x_deta = io.MouseDelta.x / scope_view_size.x;
                    float y_deta = io.MouseDelta.y / scope_view_size.y;
                    mCurve[mEditIndex][dragging_dot].x += x_deta;
                    mCurve[mEditIndex][dragging_dot].x = ImClamp(mCurve[mEditIndex][dragging_dot].x, x_min, x_max);
                    mCurve[mEditIndex][dragging_dot].y += y_deta;
                    mCurve[mEditIndex][dragging_dot].y = ImClamp(mCurve[mEditIndex][dragging_dot].y, 0.f, 1.f);
                    curve_changed = true;
                }
                else if (length <= 8)
                {
                    draw_list->AddCircle(pos + cp * scope_view_size, 5, IM_COL32(255, 255, 0, 128), 0, 3);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        mCurve[mEditIndex].insert(mCurve[mEditIndex].begin() + index + 1, cp);
                        curve_changed = true;
                    }
                }
            }
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            ImGui::CaptureMouseFromApp(false);
            dragging_dot = -1;
        }
        
        // draw control bar
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75, 0.75, 0.75, 0.75));
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 16, 0));
        bool edit_y = mEditIndex == 0;
        if (ImGui::CheckButton(u8"\uff39", &edit_y, ImVec4(0.75, 0.75, 0.75, 1.0)))
        {
            if (edit_y)  mEditIndex = 0;
        }
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 40, 0));
        if (ImGui::Button(ICON_RESET "##color_curve_reset_Y"))
        {
            ResetCurve(0);
            curve_changed = true;
        }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::PopStyleColor(2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0, 0, 0.75));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75, 0.0, 0.0, 0.75));
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 16, 24));
        bool edit_r = mEditIndex == 1;
        if (ImGui::CheckButton(u8"\uff32", &edit_r, ImVec4(0.75, 0.0, 0.0, 1.0)))
        {
            if (edit_r) mEditIndex = 1; 
        }
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 40, 24));
        if (ImGui::Button(ICON_RESET "##color_curve_reset_R"))
        {
            ResetCurve(1);
            curve_changed = true;
        }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::PopStyleColor(2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0.5, 0, 0.75));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0, 0.75, 0.0, 0.75));
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 16, 48));
        bool edit_g = mEditIndex == 2;
        if (ImGui::CheckButton(u8"\uff27", &edit_g, ImVec4(0.0, 0.75, 0.0, 1.0)))
        {
            if (edit_g) mEditIndex = 2; 
        }
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 40, 48));
        if (ImGui::Button(ICON_RESET "##color_curve_reset_G"))
        {
            ResetCurve(2);
            curve_changed = true;
        }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::PopStyleColor(2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0.5, 0.75));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0, 0.0, 0.75, 0.75));
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 16, 72));
        bool edit_b = mEditIndex == 3;
        if (ImGui::CheckButton(u8"\uff22", &edit_b, ImVec4(0.0, 0.0, 0.75, 1.0)))
        {
            if (edit_b) mEditIndex = 3;
        }
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 40, 72));
        if (ImGui::Button(ICON_RESET "##color_curve_reset_B"))
        {
            ResetCurve(3);
            curve_changed = true;
        }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::PopStyleColor(2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75, 0.75, 0.75, 0.75));
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 40, 96));
        if (ImGui::Button(ICON_RESET_ALL "##color_curve_reset"))
        {
            ResetCurve();
            mTension = 0;
            curve_changed = true;
        }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::PopStyleColor(2);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 16, 120));
        if (ImGui::VSliderFloat("##curve_tension", ImVec2(16, 128), &mTension, -1.f, 1.f, ""))
        {
            curve_changed = true;
        }
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 32, 120));
        ImGui::TextUnformatted(u8"\ue4af");
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 32, 176));
        ImGui::TextUnformatted(u8"\ue48d");
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 32, 230));
        ImGui::TextUnformatted(u8"\ue48c");

        if (need_update_scope || curve_changed)
        {
            if (curve_changed) SetCurveMat();
            changed = true;
        }
        ImGui::EndGroup();
        return changed;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;
        if (value.contains("mat_type"))
        {
            auto& val = value["mat_type"];
            if (val.is_number()) 
                m_mat_data_type = (ImDataType)val.get<imgui_json::number>();
        }
        if (value.contains("histogram_scale"))
        {
            auto& val = value["histogram_scale"];
            if (val.is_number()) 
                mHistogramScale = val.get<imgui_json::number>();
        }
        if (value.contains("histogram_index"))
        {
            auto& val = value["histogram_index"];
            if (val.is_number()) 
                mEditIndex = val.get<imgui_json::number>();
        }
        if (value.contains("histogram_log"))
        {
            auto& val = value["histogram_log"];
            if (val.is_boolean()) 
                mHistogramLog = val.get<imgui_json::boolean>();
        }
        if (value.contains("curve_tension"))
        {
            auto& val = value["curve_tension"];
            if (val.is_number()) 
                mTension = val.get<imgui_json::number>();
        }
        // load curve
        const imgui_json::array* pointArrayY = nullptr;
        if (imgui_json::GetPtrTo(value, "curve_y", pointArrayY))
        {
            mCurve[0].clear();
            for (auto& point : *pointArrayY)
            {
                if (!point.is_object()) continue;
                if (point.contains("Point"))
                {
                    auto& val = point["Point"];
                    if (val.is_vec2()) mCurve[0].push_back(val.get<imgui_json::vec2>());
                }
            }
        }

        const imgui_json::array* pointArrayR = nullptr;
        if (imgui_json::GetPtrTo(value, "curve_r", pointArrayR))
        {
            mCurve[1].clear();
            for (auto& point : *pointArrayR)
            {
                if (!point.is_object()) continue;
                if (point.contains("Point"))
                {
                    auto& val = point["Point"];
                    if (val.is_vec2()) mCurve[1].push_back(val.get<imgui_json::vec2>());
                }
            }
        }

        const imgui_json::array* pointArrayG = nullptr;
        if (imgui_json::GetPtrTo(value, "curve_g", pointArrayG))
        {
            mCurve[2].clear();
            for (auto& point : *pointArrayG)
            {
                if (!point.is_object()) continue;
                if (point.contains("Point"))
                {
                    auto& val = point["Point"];
                    if (val.is_vec2()) mCurve[2].push_back(val.get<imgui_json::vec2>());
                }
            }
        }

        const imgui_json::array* pointArrayB = nullptr;
        if (imgui_json::GetPtrTo(value, "curve_b", pointArrayB))
        {
            mCurve[3].clear();
            for (auto& point : *pointArrayB)
            {
                if (!point.is_object()) continue;
                if (point.contains("Point"))
                {
                    auto& val = point["Point"];
                    if (val.is_vec2()) mCurve[3].push_back(val.get<imgui_json::vec2>());
                }
            }
        }

        SetCurveMat();
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["histogram_scale"] = imgui_json::number(mHistogramScale);
        value["histogram_index"] = imgui_json::number(mEditIndex);
        value["histogram_log"] = imgui_json::boolean(mHistogramLog);
        value["histogram_log"] = imgui_json::boolean(mHistogramLog);
        value["curve_tension"] = imgui_json::number(mTension);

        imgui_json::value curve_y;
        for (int i = 0; i < mCurve[0].size(); i++)
        {
            imgui_json::value point;
            point["Point"] = imgui_json::vec2(mCurve[0][i]);
            curve_y.push_back(point);
        }
        value["curve_y"] = curve_y;

        imgui_json::value curve_r;
        for (int i = 0; i < mCurve[1].size(); i++)
        {
            imgui_json::value point;
            point["Point"] = imgui_json::vec2(mCurve[1][i]);
            curve_r.push_back(point);
        }
        value["curve_r"] = curve_r;

        imgui_json::value curve_g;
        for (int i = 0; i < mCurve[2].size(); i++)
        {
            imgui_json::value point;
            point["Point"] = imgui_json::vec2(mCurve[2][i]);
            curve_g.push_back(point);
        }
        value["curve_g"] = curve_g;

        imgui_json::value curve_b;
        for (int i = 0; i < mCurve[3].size(); i++)
        {
            imgui_json::value point;
            point["Point"] = imgui_json::vec2(mCurve[3][i]);
            curve_b.push_back(point);
        }
        value["curve_b"] = curve_b;
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\uf55b"));
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatIn}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };
    MatPin    m_MatIn   = { this, "In" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    const ImVec2 scope_view_size = ImVec2(256, 256);
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device                {-1};
    ImGui::Histogram_vulkan *   m_histogram {nullptr};
    ImGui::ColorCurve_vulkan *  m_filter {nullptr};
    ImGui::ImMat                mMat_histogram;
    ImGui::ImMat                mMat_curve;
    float                       mHistogramScale {0.05};
    bool                        mHistogramLog {false};
    int                                     mEditIndex {0};
    float                                   mTension {0};
    std::vector<std::vector<ImVec2>>        mCurve; //YRGB

private:
    const ImU32  mCurveColor[4] = { IM_COL32(255,255,255,128),
                                    IM_COL32(255,  0,  0,128),  
                                    IM_COL32(  0,255,  0,128),
                                    IM_COL32(  0,  0,255,128)};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(ColorCurveNode, "Color Curve", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Color")
