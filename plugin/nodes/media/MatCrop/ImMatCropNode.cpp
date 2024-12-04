#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Crop_vulkan.h>

#define NODE_VERSION    0x01000100

namespace BluePrint
{
struct CropNode final : Node
{
    BP_NODE_WITH_NAME(CropNode, "Crop", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Media")
    CropNode(BP* blueprint): Node(blueprint) { m_Name = "Crop"; m_HasCustomLayout = true; m_Skippable = true; }

    ~CropNode()
    {
        if (m_filter) { delete m_filter; m_filter = nullptr; }
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
        if (entryPoint.m_ID == m_IReset.m_ID)
        {
            Reset(context);
            return m_OReset;
        }
        auto mat_in = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (!mat_in.empty())
        {
            m_in_size = ImVec2(mat_in.w, mat_in.h);
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (m_right - m_left <= 0 || m_bottom - m_top <= 0)
            {
                return {};
            }
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_filter)
            {
                m_filter = new ImGui::Crop_vulkan(gpu);
                if (!m_filter)
                    return {};
            }
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->crop(mat_in, im_RGB, m_left, m_top, m_right - m_left, m_bottom - m_top);
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
        float setting_offset = 348;
        if (!embedded)
        {
            ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 sub_window_size = ImGui::GetWindowSize();
            setting_offset = sub_window_size.x - 80;
        }
        bool changed = false;
        int _left = m_left;
        int _top = m_top;
        int _right = m_right;
        int _bottom = m_bottom;
        auto slider_tooltips = [&]()
        {
            if (m_in_size.x > 0 && m_in_size.y > 0)
            {
                if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
                {
                    ed::Suspend();
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Source: %dx%d", (int)m_in_size.x, (int)m_in_size.y);
                        ImGui::Text("   Top: %d", _top);
                        ImGui::Text("Bottom: %d", _bottom);
                        ImGui::Text("  Left: %d", _left);
                        ImGui::Text(" Right: %d", _right);
                        ImGui::Text("Target: %dx%d", (int)(_right - _left), (int)(_bottom - _top));
                        ImGui::EndTooltip();
                    }
                    ed::Resume();
                }
            }
        };
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(300);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::SliderInt("Top##Crop", &_top, 0, m_in_size.y, "%d", flags); slider_tooltips();
        ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RESET "##reset_top##Crop")) { _top = 0; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderInt("Bottom##Crop", &_bottom, _top, m_in_size.y, "%d", flags); slider_tooltips();
        ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RESET "##reset_bottom##Crop")) { _bottom = m_in_size.y; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderInt("Left##Crop", &_left, 0, m_in_size.x, "%d", flags); slider_tooltips();
        ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RESET "##reset_left##Crop")) { _left = 0; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderInt("Right##Crop", &_right, _left, m_in_size.x, "%d", flags); slider_tooltips();
        ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RESET "##reset_right##Crop")) { _right = m_in_size.x; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_left != m_left) { m_left = _left; changed = true; }
        if (_top != m_top) { m_top = _top; changed = true; }
        if (_right != m_right) { m_right = _right; changed = true; }
        if (_bottom != m_bottom) { m_bottom = _bottom; changed = true; }
        ImGui::EndDisabled();
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
        if (value.contains("left"))
        {
            auto& val = value["left"];
            if (val.is_number()) 
                m_left = val.get<imgui_json::number>();
        }
        if (value.contains("top"))
        {
            auto& val = value["top"];
            if (val.is_number()) 
                m_top = val.get<imgui_json::number>();
        }
        if (value.contains("right"))
        {
            auto& val = value["right"];
            if (val.is_number()) 
                m_right = val.get<imgui_json::number>();
        }
        if (value.contains("bottom"))
        {
            auto& val = value["bottom"];
            if (val.is_number()) 
                m_bottom = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["left"] = imgui_json::number(m_left);
        value["top"] = imgui_json::number(m_top);
        value["right"] = imgui_json::number(m_right);
        value["bottom"] = imgui_json::number(m_bottom);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue434"));
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatIn}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_IReset  = { this, "Reset In" };
    FlowPin   m_Exit    = { this, "Exit" };
    FlowPin   m_OReset  = { this, "Reset Out" };
    MatPin    m_MatIn   = { this, "In" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[3] = { &m_Enter, &m_IReset, &m_MatIn };
    Pin* m_OutputPins[3] = { &m_Exit, &m_OReset, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    ImGui::Crop_vulkan * m_filter {nullptr};
    int m_left {0};
    int m_top {0};
    int m_right {0};
    int m_bottom {0};
    ImVec2 m_in_size {0.f, 0.f};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(CropNode, "Crop", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Media")
