#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "Hatch_vulkan.h"
#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct HatchNode final : Node
{
    BP_NODE_WITH_NAME(HatchNode, "CrossHatch", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Stylization")
    HatchNode(BP* blueprint): Node(blueprint) { m_Name = "CrossHatch"; m_HasCustomLayout = true; m_Skippable = true; }

    ~HatchNode()
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
        auto mat_in = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (m_SpacingIn.IsLinked()) m_spacing = context.GetPinValue<float>(m_SpacingIn);
        if (m_WidthIn.IsLinked()) m_width = context.GetPinValue<int32_t>(m_WidthIn);
        if (!mat_in.empty())
        {
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_filter || gpu != m_device)
            {
                if (m_filter) { delete m_filter; m_filter = nullptr; }
                m_filter = new ImGui::Hatch_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, m_spacing, m_width);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_SpacingIn.m_ID)
        {
            m_SpacingIn.SetValue(m_spacing);
        }
        if (receiver.m_ID == m_WidthIn.m_ID)
        {
            m_WidthIn.SetValue(m_width);
        }
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
        float setting_offset = 320;
        if (!embedded)
        {
            ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 sub_window_size = ImGui::GetWindowSize();
            setting_offset = sub_window_size.x - 80;
        }
        bool changed = false;
        float _spacing = m_spacing;
        float _width = m_width;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_SpacingIn.IsLinked());
        ImGui::SliderFloat("Spacing##Hatch", &_spacing, 0.0f, 0.1f, "%.3f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_spacing##Hatch")) { _spacing = 0.01f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_spacing##Hatch", key, ImGui::ImCurveEdit::DIM_X, m_SpacingIn.IsLinked(), "Spacing##Hatch@" + std::to_string(m_ID), 0.01f, 1.f, 0.4f, m_SpacingIn.m_ID);
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!m_Enabled || m_WidthIn.IsLinked());
        ImGui::SliderFloat("Width##Hatch", &_width, 0.f, 0.01f, "%.3f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_width##Hatch")) { _width = 0.002f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_width##Hatch", key, ImGui::ImCurveEdit::DIM_X, m_WidthIn.IsLinked(), "Width##Hatch@" + std::to_string(m_ID), 1, 4, 1, m_WidthIn.m_ID);
        ImGui::EndDisabled();

        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_spacing != m_spacing) { m_spacing = _spacing; changed = true; }
        if (_width != m_width) { m_width = _width; changed = true; }
        return m_Enabled ? changed : false;
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
        if (value.contains("spacing"))
        {
            auto& val = value["spacing"];
            if (val.is_number()) 
                m_spacing = val.get<imgui_json::number>();
        }
        if (value.contains("width"))
        {
            auto& val = value["width"];
            if (val.is_number()) 
                m_width = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["spacing"] = imgui_json::number(m_spacing);
        value["width"] = imgui_json::number(m_width);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\uf551"));
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
    FloatPin  m_SpacingIn  = { this, "Spacing" };
    FloatPin  m_WidthIn = { this, "Width" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatIn, &m_SpacingIn, &m_WidthIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    float m_spacing         {0.01f};
    float m_width           {0.002f};
    ImGui::Hatch_vulkan * m_filter   {nullptr};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(HatchNode, "CrossHatch", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Stylization")