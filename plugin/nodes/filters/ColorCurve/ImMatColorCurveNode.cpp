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
        ImGui::ImDestroyTexture(&m_logo);
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
                        ImGui::SetNextFrameWantCaptureMouse(true);
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
            ImGui::SetNextFrameWantCaptureMouse(false);
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
        // Node::DrawNodeLogo(ctx, size, std::string(u8"\uf55b"));
        if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
        if (!m_logo) m_logo = Node::LoadNodeLogo((void *)logo_data, logo_size);
        Node::DrawNodeLogo(m_logo, m_logo_index, logo_cols, logo_rows, size);
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
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};
    
    const unsigned int logo_width = 100;
    const unsigned int logo_height = 100;
    const unsigned int logo_cols = 1;
    const unsigned int logo_rows = 1;
    const unsigned int logo_size = 5963;
    const unsigned int logo_data[5964/4] =
{
    0xe0ffd8ff, 0x464a1000, 0x01004649, 0x01000001, 0x00000100, 0x8400dbff, 0x02020300, 0x03020203, 0x04030303, 0x05040303, 0x04050508, 0x070a0504, 
    0x0c080607, 0x0b0c0c0a, 0x0d0b0b0a, 0x0d10120e, 0x0b0e110e, 0x1016100b, 0x15141311, 0x0f0c1515, 0x14161817, 0x15141218, 0x04030114, 0x05040504, 
    0x09050509, 0x0d0b0d14, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 0x14141414, 
    0x14141414, 0x14141414, 0xc0ff1414, 0x00081100, 0x03640064, 0x02002201, 0x11030111, 0x01c4ff01, 0x010000a2, 0x01010105, 0x00010101, 0x00000000, 
    0x01000000, 0x05040302, 0x09080706, 0x00100b0a, 0x03030102, 0x05030402, 0x00040405, 0x017d0100, 0x04000302, 0x21120511, 0x13064131, 0x22076151, 
    0x81321471, 0x2308a191, 0x15c1b142, 0x24f0d152, 0x82726233, 0x17160a09, 0x251a1918, 0x29282726, 0x3635342a, 0x3a393837, 0x46454443, 0x4a494847, 
    0x56555453, 0x5a595857, 0x66656463, 0x6a696867, 0x76757473, 0x7a797877, 0x86858483, 0x8a898887, 0x95949392, 0x99989796, 0xa4a3a29a, 0xa8a7a6a5, 
    0xb3b2aaa9, 0xb7b6b5b4, 0xc2bab9b8, 0xc6c5c4c3, 0xcac9c8c7, 0xd5d4d3d2, 0xd9d8d7d6, 0xe3e2e1da, 0xe7e6e5e4, 0xf1eae9e8, 0xf5f4f3f2, 0xf9f8f7f6, 
    0x030001fa, 0x01010101, 0x01010101, 0x00000001, 0x01000000, 0x05040302, 0x09080706, 0x00110b0a, 0x04020102, 0x07040304, 0x00040405, 0x00770201, 
    0x11030201, 0x31210504, 0x51411206, 0x13716107, 0x08813222, 0xa1914214, 0x2309c1b1, 0x15f05233, 0x0ad17262, 0xe1342416, 0x1817f125, 0x27261a19, 
    0x352a2928, 0x39383736, 0x4544433a, 0x49484746, 0x5554534a, 0x59585756, 0x6564635a, 0x69686766, 0x7574736a, 0x79787776, 0x8483827a, 0x88878685, 
    0x93928a89, 0x97969594, 0xa29a9998, 0xa6a5a4a3, 0xaaa9a8a7, 0xb5b4b3b2, 0xb9b8b7b6, 0xc4c3c2ba, 0xc8c7c6c5, 0xd3d2cac9, 0xd7d6d5d4, 0xe2dad9d8, 
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xf6003f00, 0x2f695b3b, 0x2a712724, 0xc36b72bd, 0xa35b69bf, 
    0x13a0b926, 0xf5319ed4, 0xcf1ae915, 0x16fe938d, 0x2c869f1d, 0xed5857e4, 0x37de79de, 0x01c68304, 0x6b5e4fc7, 0x596a3fcb, 0x78aec7be, 0x96042070, 
    0xf915f523, 0x58521aae, 0x63ebbf88, 0x57543cf5, 0x6b6f9bb0, 0xe133677e, 0x1cda3fc6, 0x5bde15f4, 0x3c00304c, 0xf03baf76, 0xa2dbcef5, 0xd491644a, 
    0xcedeae76, 0xec0b4ee1, 0x37cb718d, 0xef8c5e41, 0x114e66cd, 0x930d4d2e, 0xe3ec063e, 0xf28555e9, 0xddf022c9, 0xcb746970, 0x809b7bcb, 0x70e8a07c, 
    0xef773b14, 0x0f7badd3, 0x177df480, 0x3e11b35c, 0x39276593, 0x6615327f, 0x040cdd35, 0xd6fcc6ea, 0x6956fc3c, 0x1cb40f4f, 0x70b81d8f, 0xe11f030f, 
    0x63f19a58, 0x6b87cc02, 0x709fcac7, 0x78445f33, 0xb52de0df, 0xf7259dde, 0xafadd185, 0xd36656a1, 0xe01279be, 0xab979f01, 0x0200762b, 0xb50e0c3a, 
    0xa58b7ff3, 0x2e6fd1bb, 0xd642dd74, 0x02fe3e5d, 0x945b5b56, 0x70e42229, 0x0407b076, 0x20030972, 0x69784a83, 0x42f7dde1, 0xe3c38fa3, 0xaf47be69, 
    0x856f7b66, 0xa0ad066e, 0x3f4cdc59, 0xd880e57b, 0x4d7cd53e, 0x15759f2e, 0x96ecb3c5, 0xf6840e27, 0x1757e420, 0xa7a14be1, 0xedce56f8, 0x503e6c9b, 
    0x01e02467, 0x4b694dea, 0x37d89ba9, 0x60a0c144, 0x410e5d19, 0x6bf25a1f, 0x2eb929c5, 0x7e0aabe7, 0x7ed399e5, 0x8f1b1ed2, 0xd1155ec5, 0xaae9277e, 
    0x2c959122, 0xc6895835, 0x338c704e, 0xc7910dd7, 0xe72b3d62, 0xbaf8123f, 0x1bfec0c3, 0x3f75d7bb, 0x85c1da30, 0xb45ce158, 0xa3ba1f92, 0xe2b927d3, 
    0x79f893be, 0xf0472bf1, 0x43af9906, 0xd3ed09e3, 0xb276b7c1, 0x2a77d10b, 0x70da9946, 0xb9ab7201, 0x8c04e8be, 0xb7f22b92, 0x9b337ec7, 0xbd295ec7, 
    0x762cf3d4, 0x9dae3309, 0xd9cf2167, 0x837cf7ed, 0x4fc638d3, 0x5db65e73, 0x2c57e241, 0x8fd55fb6, 0xffe2c78c, 0x2ae5b200, 0x556f5771, 0x4f4d3af3, 
    0x8bf884f6, 0x3e4fa879, 0x476de29d, 0xcb91b341, 0x5ca76147, 0x7f8ce218, 0x5a4fea53, 0x0b00ffab, 0xfd9fe2ef, 0x00fffa0f, 0xf8b706fe, 0xe16d1fd7, 
    0xe8ea102f, 0x3b3c2c6e, 0x9270eaac, 0xc2c69641, 0xba5b9149, 0xf4a80396, 0xfc2f15f7, 0x7f17ef2b, 0x5fe299d0, 0xff4b14fc, 0xd6d7c400, 0xd1712a7b, 
    0xf51f1f45, 0xeebd549c, 0xf0837d76, 0xfbe2fa66, 0xbabd39e2, 0x3c77cd94, 0x1f92b432, 0xfbbb35e2, 0x115f9557, 0x77776f78, 0x2bee33ce, 0x11d4e02f, 
    0x096dc27f, 0x3fdc8f41, 0xfdde6a7f, 0x3551adae, 0x1c0c00ff, 0xfd3d9664, 0x34177cc5, 0x5f363256, 0x22d58fd5, 0x77a5d49f, 0x7014fcaf, 0x97ca149a, 
    0xb8b09905, 0xa7265deb, 0xe9f0822c, 0xc9f040f3, 0xf8e10c18, 0x1f03f520, 0x9e730d42, 0x756f811b, 0xc7212bab, 0x008e67da, 0x176efb4f, 0xfca7d7fa, 
    0xd365d174, 0x97ea2abe, 0x9f621528, 0xb3a108c9, 0x142c00ff, 0x7400ff77, 0x9d4beed6, 0x5c4e8a5c, 0x8df9b19c, 0xb2602dd1, 0x48dc5c80, 0x9fe0babb, 
    0xbdd2bfba, 0x8d2ee1c7, 0x5cdfb47b, 0x4bdaa603, 0xc433aefe, 0x1472bdce, 0x4f7dd0fd, 0x79bd833e, 0xc4748527, 0xb9b9e0f3, 0x2772e41b, 0x293c45b7, 
    0xefed9d53, 0xcf918cd3, 0x032df74a, 0x02741ec4, 0x51a52565, 0xa397d9bd, 0x2bf28f67, 0xf2d0a1db, 0x2797f171, 0xd1644f24, 0x35de86a6, 0x542d1158, 
    0xc901c59d, 0x9ea48e73, 0x00ffe6b5, 0x2dc07fb4, 0x874fe337, 0x9a2dd666, 0x69185f7b, 0x090b1356, 0xb78f762d, 0x5bd18c10, 0x7cf7914b, 0x40dd00cc, 
    0xdaab7724, 0x120c896f, 0x37f376ec, 0x7fbd8273, 0xb7dd562c, 0x424c9b8d, 0x3698d959, 0x3b180be5, 0xfa395007, 0x07bdd77b, 0xd8d28a0a, 0xaa949af8, 
    0x743c97d0, 0x27d1463e, 0x8200ff5b, 0x5d1943e6, 0x47ee7bda, 0x908383e0, 0xb8e6a847, 0x6f92d85f, 0xf058fc15, 0xdaa3a3ae, 0xd077f7e8, 0x8b62cee8, 
    0x09b67152, 0x3b2c9f7a, 0x410f331c, 0x15670c9c, 0x079f14f6, 0x5e118f74, 0x36b66e5d, 0x6fe41dda, 0x9da0cd25, 0x922b49b7, 0x85989f5f, 0x6138d96d, 
    0xee8af18c, 0xdfe1053c, 0xd01dfc0b, 0xe80a3fb4, 0x68a58d36, 0x6bcd7eea, 0x3649e66d, 0x16c4e206, 0x7aea6676, 0xc26b9e93, 0x555280a1, 0xe86b5f21, 
    0x67c5a8cf, 0xd5e88c33, 0xeea26ec3, 0xf0d374ba, 0xfb41f9b9, 0xbf40fc5f, 0x5d21fed5, 0x96e11178, 0xdcf04fc7, 0x25c83be6, 0xcc6b5a1b, 0x2afb4f72, 
    0xc9f7140e, 0xfecfeb3d, 0xbc2ffc09, 0x28e3a1f8, 0x2c19116d, 0xed3d712d, 0x33115ec0, 0x62f5a8f2, 0x6bea0378, 0xf1b0f65b, 0xff103f6c, 0x88cf6900, 
    0xe986a5da, 0xc935af2d, 0xbbdee12d, 0x08b1080b, 0xcb673ce3, 0x577b7d3d, 0xb7b35fd8, 0x187e48c2, 0x790b1278, 0xe0561d50, 0x54f2a2fd, 0xddb4ba94, 
    0x1500e080, 0x7a3d1d48, 0xd05aaff5, 0x3eb552c1, 0x3a9f799b, 0x8a6faebd, 0xf7bd6b75, 0x772400ff, 0x6ef01c7e, 0x16dde199, 0x59b64fdb, 0x7085a12c, 
    0xd5b6dbb0, 0xa9e35c5f, 0x69ad27f5, 0x596400ff, 0xc5cf00ff, 0x34fcfddf, 0xf3582ef7, 0xd77af349, 0xdccc2783, 0x48faec9e, 0x4a5284d3, 0xdfcfc72a, 
    0x7c486006, 0xdef00366, 0xabfced53, 0xf9c2f65f, 0xb7095f35, 0x7ec4ccdd, 0x00ffb9a2, 0x00ff1386, 0xc7b4c2c5, 0x00ff2578, 0xfda56bd0, 0xa8dde2b1, 
    0x18274f78, 0x3100ff9c, 0xfa2b175e, 0x917f7edc, 0xfa92d2f4, 0xe8d65f9b, 0xea0e1ff3, 0x06f14563, 0x9732aa91, 0x2ed81b4b, 0x4975479d, 0x81fc8815, 
    0x68bfa6af, 0xb56f695b, 0x63d3bfd5, 0x65d3bab6, 0xe5223f96, 0x22ee904a, 0xa7bb6165, 0xe6abfc66, 0x26cd12dd, 0x74df60ba, 0xc919e329, 0xaf3900ff, 
    0x397e6d54, 0x73f8173e, 0x607802c3, 0x1af34c83, 0x9395cbe4, 0x70e769cc, 0x64b6e973, 0x3ef6b7e2, 0xba2db4d2, 0x944e4a66, 0xd564ef5c, 0xafaf51bf, 
    0xc69669db, 0xb3af6be2, 0x64dc215b, 0xd8dee597, 0x093d71e3, 0x96573bec, 0x9af69378, 0x674d7c68, 0x30739706, 0xf35d86b5, 0x06a484b4, 0xdd71c115, 
    0x030072b0, 0x6b3d920d, 0x7c26fec6, 0x557cd55d, 0xfdd3e674, 0x78b54f07, 0x997a48e4, 0x9e936139, 0x0f064e72, 0xe043fab9, 0xdfd7879f, 0x1d6bf515, 
    0x6b0d4707, 0x2e7249a8, 0x22299c26, 0x984f02de, 0x90c4e841, 0x728e03a4, 0xf4e30079, 0x960e0ebc, 0x55ad0e16, 0x917f76ef, 0x9b63d6f9, 0x54d4c454, 
    0xd547cf70, 0xe000ff6e, 0x4b78401f, 0x9fc57fe2, 0xb723be89, 0xde82f7f0, 0x514ed8da, 0x88b8baa6, 0x1848f64b, 0x1db725b7, 0x73fd02ce, 0xbee6c0c9, 
    0xa27df8dc, 0x3fbc863f, 0xdcacc1a7, 0x81f8687f, 0xee2e8d60, 0x9856ed82, 0xed40de28, 0x8aebc996, 0xbf852ff3, 0x813f740d, 0x1a96449a, 0x7f99d76d, 
    0x146d0c7b, 0x64efb7f2, 0x1c8001c8, 0x93a07c64, 0x7bb547ea, 0x926c824f, 0xf13a3ff6, 0x7cc4ccb7, 0x7e4eb7a7, 0xd8f1b8a6, 0x6aa9338a, 0x26bcc84a, 
    0xdcec1713, 0x00ff4aea, 0x114f398a, 0x1f1ad0fe, 0xad4b3587, 0x788c4607, 0xfe30c58b, 0xbe24dae8, 0x464a305c, 0xc8314b42, 0x27971d07, 0xbf7cc5b1, 
    0xd1fe47fb, 0x2f81efda, 0xd55a9765, 0x7b7c6e2d, 0x69f16675, 0x98d21fbe, 0x361a4d8b, 0xedcc0552, 0xb8415e92, 0x9f8c4394, 0x750ee098, 0xa6686a7f, 
    0x5cfb61f8, 0xdae63278, 0xf14d2b5f, 0xc6522c32, 0xf9ef2f84, 0xe77a7c45, 0xbfc23e6f, 0xb9c5753f, 0xe6428d6b, 0x509fe5f9, 0x3c73b9b9, 0x676731f3, 
    0x491277dc, 0xb1d6efc9, 0x358af651, 0xaf5bf422, 0xfabb9a33, 0xaf1635af, 0x5f7ef426, 0xec3f7be6, 0xfee7f06d, 0x7f1f6f12, 0x4ba24e6b, 0x4677215d, 
    0xed74675c, 0x00dfa790, 0x577c1f33, 0xf9dbd3de, 0x00bdaa48, 0x96fd1daf, 0x813f1ffe, 0xb3e91dbe, 0x17ea2aba, 0x2bb3f911, 0x04384b70, 0x8f76c18f, 
    0xdcaed7c0, 0xd911c9ea, 0xc3c21373, 0xe615debf, 0x2aed6762, 0x1e9aae72, 0x342a05c6, 0xa496f755, 0x8c534626, 0x79d2f891, 0xf9e3fdad, 0x083579d5, 
    0x1004c91d, 0xfda6ee7b, 0x7ff61fba, 0xe5e4aaef, 0xcedf995d, 0x187e9bcf, 0x277e9b6a, 0x66c79669, 0x1dcf091e, 0xd26be67e, 0x375b6cbf, 0x2ac2477d, 
    0xbf9f1908, 0x9fcf57d4, 0xe4eeaf06, 0xc6a4d1f8, 0x661fe6e5, 0x70b025b9, 0x455f433e, 0x33b2d77e, 0x270b9eea, 0x2bf05a21, 0x32da1938, 0xb522af39, 
    0x04154329, 0x86b767b7, 0x49f81aab, 0x00fff7c9, 0x31bccd23, 0x5ef8a477, 0xf155ebf2, 0x34d2c97d, 0x481a784b, 0xf7dc092e, 0x1252a932, 0x222d8038, 
    0x00ff4a90, 0x3c47dc08, 0x888ff21a, 0xf8ba313e, 0xeb1be385, 0x2d3e7d9b, 0x63493d3a, 0x13ca58bc, 0x84458423, 0x86a48ef9, 0xdb8533e6, 0xc6732afc, 
    0x3de26f2b, 0xd4217ed4, 0x1879dad2, 0xf1ced9db, 0x44438e46, 0x9f5ca8b1, 0x2e49612c, 0x39237ec0, 0xf83eaf38, 0x2ff63f87, 0x60880bc6, 0xb4c9febe, 
    0xdc58348e, 0x33525895, 0x4fb56308, 0xfee482cb, 0xd5aff724, 0x0c3d59b0, 0x757194b6, 0xb3ede67d, 0xfcd0bff9, 0x31ce33c7, 0xc2b3da18, 0x5b8c7653, 
    0xceeaa77f, 0x6d4cc623, 0x4b4bed35, 0x10d7029e, 0x4b55f6cb, 0x5c741966, 0x0eb6f608, 0x081d851f, 0xaf0010c3, 0xebd92fd0, 0x00ffd6c0, 0x0d1afe01, 
    0x6a1f564f, 0xe288bff1, 0x95656f2f, 0x50d8fd38, 0x89e5c649, 0x72723246, 0x6cbee2c4, 0xfc820ff0, 0x8b6fe233, 0x5587f810, 0x8ff02649, 0x6de36c85, 
    0x5395547b, 0x2877d7ec, 0x4ec80083, 0x811c23c1, 0xe52d81b9, 0xd16b92f1, 0x577c11be, 0xf82b7ed6, 0x46f15397, 0x6d6fafaa, 0x9ae9a43c, 0xa7934f66, 
    0x902a865a, 0x01030f44, 0xcc076241, 0xf99a27d9, 0xed18d3fc, 0xf56f7429, 0x018f6c3d, 0x5398b80a, 0x9df8e197, 0xc407bad4, 0x1a77891f, 0xb9365efc, 
    0x5bd6b4d2, 0xfb275d07, 0x55e2ce3e, 0xa39b4469, 0x2484bc5c, 0x63dcdd78, 0xa3bdd623, 0xbfe6f5e0, 0xf1237ca6, 0x57f91014, 0xa781fe7a, 0xd27c6d18, 
    0xc23233c6, 0x6cbe19e5, 0xe583dcec, 0x010e0e52, 0xa1d641ef, 0x8bcb051a, 0x87e8cc78, 0xb11b6d12, 0xe093828e, 0x152d8a9f, 0x4d5b20be, 0x7efe6e71, 
    0xfc882399, 0x1870c8d5, 0xf3eb1963, 0xc6ab8057, 0x33df4a52, 0x73992af4, 0xcaa951c3, 0xdb5b5bfb, 0xf11d7c54, 0x6ed75d83, 0x59782f3e, 0xedbf8bf5, 
    0xb6bda129, 0x45fa92bb, 0x15981594, 0xc88c6269, 0xf0608cc5, 0xe55aaf3f, 0xb6060ff5, 0xc942fbb9, 0x3fe29ce1, 0xec7c79ed, 0x91a0f201, 0x703af09d, 
    0xfed215a3, 0xbaed7ad4, 0x0b8648fc, 0x4c5b2d2f, 0x1aa97bb5, 0x2d57b92d, 0x38746cc0, 0x5eebf300, 0xc0cff0a5, 0xb53fea90, 0xd9b58d17, 0xa16df360, 
    0x72e06e48, 0xc4b590a5, 0xc669e76a, 0xfde9063a, 0x5b21f6ea, 0xdd3e8792, 0x9831cf4f, 0x547afd68, 0x4b93ee62, 0x479f5aee, 0x56e92259, 0xc7295b50, 
    0xd4e7bc96, 0x73af8af3, 0x4bf7bca8, 0xdd036f6f, 0xa476485d, 0x3896a428, 0x176aa6f4, 0x80fb2c18, 0x2bbdd6ef, 0xe382b7e0, 0xd51ab2b9, 0xab5b02d4, 
    0x4b433ea7, 0xc7987c1e, 0x0447ae00, 0xf06a8ffa, 0x1e8e552a, 0xbb5ad63e, 0x64f62cfe, 0x70d4fdb9, 0x88f7e063, 0x95272075, 0xbf19ad2d, 0xc84993e5, 
    0x7f14f81e, 0x7fd790c2, 0xfdafb5e7, 0x5fe13ffc, 0x0083a650, 0x7d529551, 0xf62fad5b, 0xcef31f14, 0xf8ebcb3a, 0x74e2c8be, 0x253f77e8, 0xff0b653f, 
    0x894fed00, 0x2892747e, 0x7dea6809, 0xe48f0489, 0xdacfe86b, 0xb816d6ce, 0x75cf52f0, 0x58fbb01d, 0x7c02e6db, 0xf1f291c3, 0xc0531fd0, 0x017e99af, 
    0xc2b7bae9, 0x4647895f, 0xdb8b15f1, 0x8676d59b, 0xac185e55, 0x710f9264, 0xf647fa8a, 0xaf4bb2c9, 0x94da690e, 0x9766f386, 0xe85ff714, 0xd895ab51, 
    0x6239b7a5, 0x838e5100, 0x0d0e7b2c, 0xffa55674, 0xe9140a00, 0x1c387bb4, 0xea5c5942, 0x0ffd5775, 0x7d00ff93, 0x3388afe2, 0x8231da59, 0x66b73c13, 
    0x88cc4878, 0xb115c97c, 0x4e3eaaec, 0x5eef3983, 0x846fe1a1, 0xb516bfb6, 0x5f2b5d4b, 0xfdb4bab5, 0x84da2cd3, 0x9ae58831, 0x0dd62525, 0xcaf938a7, 
    0x3fd171be, 0x033ff11a, 0x83783a5c, 0x1aeed64e, 0x78366d6b, 0x22ae2996, 0xb079bc53, 0xd7d341b4, 0x3a601969, 0x7dafd80e, 0xaf887ff0, 0xc5fe6896, 
    0x24de2c7d, 0xbb675a85, 0xa93ceed2, 0x05b39314, 0x5bfd2388, 0x23c7e10f, 0x14edd79c, 0x2918b1e0, 0xdad68b52, 0xb2ca8f1f, 0x67558aa7, 0x7f3cf5b5, 
    0x9cf887f6, 0x2fbde69a, 0x8254f486, 0x93b6c3cf, 0x0d6beb26, 0x672c13a2, 0xf940a66a, 0x8edc0282, 0xc061960b, 0xb35f755c, 0x74ed88cf, 0x92cdfee4, 
    0xcd3cbb40, 0xbc00ff9e, 0xd5e40762, 0x85f67ae0, 0x7ba58727, 0xa1790849, 0x10d4dbdc, 0x6b9785ae, 0x1ec17900, 0x6a470ebd, 0xda6ff0d3, 0x189b35ee, 
    0x494b9e61, 0x17619d64, 0x8f8df531, 0xa7d3ef43, 0x1fc7fc7a, 0xcee99486, 0x59f6999c, 0x98f06046, 0xaf6262c8, 0xe8a7deae, 0xf2aa781b, 0x2d29daca, 
    0x81b9e32d, 0xc903d9ce, 0x60e094b5, 0xb59e8381, 0x55f17fb1, 0xebf01dbc, 0x4a93bd54, 0x694944d2, 0xd0d43d71, 0x60e3e6cc, 0x50107c46, 0x852dd802, 
    0x7ee46acf, 0x95cea51c, 0xd5eb8b02, 0xf90815d5, 0x3f21d89a, 0xbaa61e28, 0xb3bf8b7f, 0x45fbafce, 0xdfda0e78, 0xb4add746, 0x2a636039, 0xd9014fda, 
    0x55b67c74, 0x283f7546, 0x0f92b13b, 0xaf383807, 0x73c5c29e, 0x9fd1d2fb, 0xb598e6ac, 0x18a7341c, 0xd69f2e37, 0x54e7a7be, 0x309ef717, 0xa70ef1bd, 
    0x6add4b76, 0xdd627719, 0x7b00b737, 0x57d0070e, 0xad031fde, 0xa27df809, 0xb901ceea, 0x56db34d4, 0x9092fb91, 0xfe2763ac, 0x7de46b02, 0x888fe14b, 
    0x8f780e7e, 0x8ceff05a, 0x5d7499f4, 0x0a2d6c53, 0x883b1abe, 0x43de6f7e, 0x735df920, 0xa1e341c6, 0xfea9afc1, 0xa69aea1e, 0xc2dbf089, 0xc7d9d9b0, 
    0x991e6573, 0x2487530f, 0x8cedb8e5, 0x156faf30, 0x7f57391d, 0xc02b3ff3, 0x724af349, 0x67f1cfde, 0xe449ae67, 0xb0c54c5b, 0x5feb0915, 0x4207785e, 
    0xf4837f1a, 0x6d52f888, 0x838e2422, 0xd7fa07e5, 0x8f973ac2, 0x741dd661, 0x6d2e2df9, 0x37d29127, 0x03e01e0f, 0x1d41a63a, 0x8ff63579, 0xfe8c1fc6, 
    0xe013f81f, 0x615ded4f, 0xdba21af1, 0x33c65959, 0x7081cc35, 0xf1dc8ba8, 0x4d32e893, 0xc5e87878, 0xaf569c4a, 0x52a3d053, 0x89bee773, 0x2cf13b75, 
    0xffd9fb38, 0xf9528000, 0x00ff7ab0, 0x95f995e3, 0xffddfeaa, 0x8b351a00, 0x5bec6ee9, 0xb21dfec3, 0x9e0e9390, 0xf1702df6, 0x16d26da7, 0x15803f19, 
    0x731bfe53, 0x07fd97e3, 0x00ff3bbc, 0xcefe8f82, 0xbff5eaaf, 0x7e99f7bb, 0x31eecfef, 0x3f41bc6d, 0xfc13be8a, 0xde45f113, 0xae29f5c9, 0x6eee6d6f, 
    0xb738de4f, 0x00ff1997, 0x8f7a45be, 0xb6aad1ed, 0x18eaf097, 0x577932a3, 0x8d70e245, 0xca58feb7, 0xa9e77932, 0xcc6bbee3, 0xcbe0177c, 0x5cf6179d, 
    0x1757dcf0, 0xa573cf11, 0xbcb2d26b, 0x45928247, 0x00ff3126, 0xe9b5fe7d, 0xd14cb77f, 0x081be0c7, 0xa289db64, 0x0371979d, 0x18b011e7, 0x8dbe00ff, 
    0x73bd587a, 0xfaed375a, 0xb6d3e531, 0xf48d8955, 0xf868f34f, 0x1e65ce22, 0x356f9a1d, 0x7197dc45, 0x7c0e712c, 0x243942a8, 0x42bc00ff, 0xe7c01da9, 
    0xec177bb5, 0x79e200ff, 0xdc6e0d75, 0x5db0f95b, 0x9ae59538, 0x400cf269, 0x3b75ab63, 0xafa08f82, 0x9aa3d316, 0xac28bcf0, 0x83a10ef7, 0x851c9c01, 
    0xae830e04, 0x03bdd607, 0x5d12cce0, 0x6f9e2ede, 0x1d39f42e, 0xf6de9264, 0x6e924cd5, 0x986ccc18, 0x9e43a5fb, 0xdea75007, 0xaacb45bf, 0x7a5435ca, 
    0x00fffc58, 0x3e281515, 0x617c2ef7, 0xd4548fb1, 0xbbb0252d, 0x8aa5d5b6, 0xaeee48c2, 0x1b02982d, 0x184912ed, 0x94322464, 0x4710a288, 0xb19a3c55, 
    0xb7c2c7f0, 0x3a141e37, 0xe706a8cb, 0xdb8eb356, 0xc6959f03, 0xf9f3fa48, 0x1aeb9b57, 0xbc8eef9d, 0x9677f66d, 0x638bd718, 0xbe960770, 0x76287f54, 
    0x6420493d, 0xbe26c993, 0xa475f8a6, 0xa47baba6, 0xff21d8da, 0x44778400, 0xca70e95d, 0x6ddcdd42, 0xaa670ada, 0x77772c28, 0xf1958e27, 0x3a29ce19, 
    0x37d1e592, 0xf6997ff9, 0xf6803dfc, 0x7df53df5, 0x7e7df534, 0xeaeeef4a, 0x898e267b, 0x40201a67, 0x471258aa, 0xbdd68173, 0xc67ac08b, 0x398cb6d5, 
    0x67001400, 0xa0bca28e, 0x4244e743, 0x8fae1875, 0xb791fac2, 0xedd82896, 0xfc186565, 0x2a457c45, 0x9f4969b6, 0x2ae863b1, 0x7f47b494, 0x24fc6bfb, 
    0xfbc1f8b7, 0xea827834, 0x126f15de, 0xf5d96e78, 0x07e12a8d, 0xbbc614ef, 0xbb1e88a5, 0x1ec3305e, 0xf16a4fa1, 0xae5b0f1f, 0x12ede099, 0x364c72d9, 
    0xda072030, 0x2bfd1835, 0x0100ffec, 0xb6fa16ea, 0x907bf695, 0x982245b3, 0x0cb98de5, 0x3d8260a4, 0xb5f93508, 0xf1582bf1, 0x170fc3af, 0xb64b9bf8, 
    0xf4fd8ebd, 0x716b42dd, 0xcb17756b, 0xb72931b0, 0xa954801c, 0xc7fa5a1f, 0x5b9b4279, 0x4ff05f7f, 0xe67eabc9, 0xb35a9373, 0xfbd9afe9, 0x8e9fc5e1, 
    0xaa7755fc, 0xa543e0df, 0x008e09cb, 0xdd24ef07, 0x36ec13b8, 0x2baff7f4, 0x6bbcb0fd, 0x2dfe8e07, 0x2671760b, 0x5af8e193, 0x1aee4628, 0x01237162, 
    0xfa5c78ea, 0x3f5deb03, 0x1a8c7fc2, 0x83afc377, 0xbc14bfb6, 0xd449b74b, 0xa0eeacee, 0x8b854187, 0xc54af73c, 0x4fce0554, 0x1a004fae, 0x937840f9, 
    0xa456f151, 0x0ffa8dba, 0x9ef91bed, 0xdcd7ad58, 0x61cf71e7, 0xe135dfdb, 0x1357a7d3, 0xad48958d, 0x8e3d9f16, 0x46a5d5e8, 0x97ea5392, 0xb9e797f9, 
    0x2e528ab4, 0xc5df3c94, 0xb70732b5, 0xb7fd51e1, 0x9e00ff4c, 0xbc0ef95f, 0x9ec500ff, 0x752cd527, 0xd7f4edc9, 0x7cd1ec8b, 0x58126c99, 0x47d63c75, 
    0x233e26fc, 0xf73ffcfe, 0x54a7afc8, 0x3e71b4a0, 0xf3e95872, 0xcfea333b, 0x1dce8ad9, 0xf141f66f, 0x7eecc545, 0xa86bd07c, 0x94133bb1, 0x3f304400, 
    0x3f54a3ef, 0x12f7a3e0, 0x4be09d2e, 0xe644c470, 0x80dc5762, 0x4cf6572b, 0x379300ff, 0x87fd7ff1, 0x6800ff93, 0x14fc3756, 0x8047fd87, 0xf4afed7f, 
    0xe69a3aae, 0xe41f7d94, 0x9634387a, 0x75b78853, 0x1fa500ff, 0x5136e82f, 0x0430412b, 0x826daf48, 0x2edc3a0e, 0xf901f47f, 0xbd35e89a, 0x4b7b7b4a, 
    0x56c2109f, 0x1639520d, 0x428d25dd, 0x710642ec, 0xdee7a98f, 0xf53fb4b1, 0x6bbf7f51, 0x15a500ff, 0xf21fc47f, 0xd77f5e26, 0x2500ffda, 0xee61d3af, 
    0x3a3fbad3, 0x3e4bbd93, 0x74e400ff, 0x7c0b00ff, 0xf85ba309, 0xad915abb, 0x74c3cb45, 0xf3713352, 0x8f0707ec, 0x277dadcf, 0x288ca0a5, 0x0028b451, 
    0x33bcda01, 0x8dfcb7e1, 0x00ff205e, 0xb5e8dfaf, 0x7e75baee, 0x9b549c0f, 0x7a8bd5c7, 0x457fec26, 0x7f38c570, 0xb5955267, 0xd2ab536b, 0xa7cf4986, 
    0xb4b556f8, 0x0c04264a, 0xc99a831c, 0xf39fbad2, 0xff165be9, 0x2a7feb00, 0xfac886e5, 0x435f573c, 0x4c1a7edc, 0x1ec3b0f1, 0x7e2c5f31, 0xd9b0f6d5, 
    0x8fbad3fe, 0xf7e21a97, 0x759eb34f, 0xc23e436e, 0x7f91fc99, 0xe197fa2a, 0xabfcdebf, 0xe7da0fe6, 0xbf754efe, 0xff6515ec, 0xda57b300, 0xc807fed1, 
    0x5f3087fc, 0xcaa3f9be, 0xdaa37dbc, 0x724fa047, 0x92481262, 0xe4ccd718, 0x0327e32a, 0x70cd71b6, 0x2ffa17da, 0xaf4d7c86, 0x5ba9cdaf, 0x1f2912c0, 
    0x808e8090, 0xf846af76, 0x2200ff87, 0x7cfdcfad, 0xbc06fd1f, 0x00ff4be7, 0xdfe26792, 0xca9fe6fa, 0xe9ddcab3, 0x259d91dd, 0xb147c91a, 0xc12bf8c1, 
    0xd12f3e56, 0x4ba8a506, 0x4a52d772, 0xed728cc1, 0x7b4cbf07, 0x2afcf7d6, 0x7afe0f2d, 0xdf00ff5e, 0xb5fe00ff, 0xf91fc22f, 0x00ff6213, 0x2bfdafae, 
    0x2461afb5, 0x36f3d9d5, 0x00d9ff3f, 
};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(ColorCurveNode, "Color Curve", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Color")
