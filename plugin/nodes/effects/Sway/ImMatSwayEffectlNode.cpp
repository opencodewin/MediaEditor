#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "Sway_vulkan.h"
#define NODE_VERSION    0x01000100

namespace BluePrint
{
struct SwayEffectNode final : Node
{
    BP_NODE_WITH_NAME(SwayEffectNode, "Sway Effect", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Effect")
    SwayEffectNode(BP* blueprint): Node(blueprint) { m_Name = "Sway Effect"; m_HasCustomLayout = true; m_Skippable = true; }

    ~SwayEffectNode()
    {
        if (m_effect) { delete m_effect; m_effect = nullptr; }
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
        if (m_SpeedIn.IsLinked()) m_speed = context.GetPinValue<float>(m_SpeedIn);
        if (m_StrengthIn.IsLinked()) m_strength = context.GetPinValue<float>(m_StrengthIn);
        if (m_DensityIn.IsLinked()) m_density = context.GetPinValue<float>(m_DensityIn);
        auto time_stamp = m_Blueprint->GetTimeStamp();
        auto durturn = m_Blueprint->GetDurtion();
        float time = (durturn > 0 && time_stamp > 0) ?  (float)time_stamp / (float)durturn : 1.0f;
        if (!mat_in.empty())
        {
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_effect || gpu != m_device)
            {
                if (m_effect) { delete m_effect; m_effect = nullptr; }
                m_effect = new ImGui::Sway_vulkan(gpu);
            }
            if (!m_effect)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_effect->effect(mat_in, im_RGB, time * m_speed, m_strength, m_density, m_horizontal);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_SpeedIn.m_ID)
        {
            m_SpeedIn.SetValue(m_speed);
        }
        if (receiver.m_ID == m_StrengthIn.m_ID)
        {
            m_StrengthIn.SetValue(m_strength);
        }
        if (receiver.m_ID == m_DensityIn.m_ID)
        {
            m_DensityIn.SetValue(m_density);
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
        float _speed = m_speed;
        float _strength = m_strength;
        float _density = m_density;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_SpeedIn.IsLinked());
        ImGui::SliderFloat("Speed##Sway", &_speed, 0.f, 100.f, "%.0f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_speed##Sway")) { _speed = 20.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_speed##Sway", key, ImGui::ImCurveEdit::DIM_X, m_SpeedIn.IsLinked(), "speed##Sway@" + std::to_string(m_ID), 0.0f, 100.f, 20.f, m_SpeedIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_StrengthIn.IsLinked());
        ImGui::SliderFloat("Strength##Sway", &_strength, 0.f, 100.f, "%.1f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_strength##Sway")) { _strength = 20.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_strength##Sway", key, ImGui::ImCurveEdit::DIM_X, m_StrengthIn.IsLinked(), "strength##Sway@" + std::to_string(m_ID), 0.0f, 100.f, 20.f, m_StrengthIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_DensityIn.IsLinked());
        ImGui::SliderFloat("Density##Sway", &_density, 0.f, 100.f, "%.0f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_density##Sway")) { _density = 20.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_density##Sway", key, ImGui::ImCurveEdit::DIM_X, m_DensityIn.IsLinked(), "density##Sway@" + std::to_string(m_ID), 0.0f, 100.f, 20.f, m_DensityIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (ImGui::Checkbox("Horizontal##Sway", &m_horizontal)) { changed = true; }
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_speed != m_speed) { m_speed = _speed; changed = true; }
        if (_strength != m_strength) { m_strength = _strength; changed = true; }
        if (_density != m_density) { m_density = _density; changed = true; }
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
        if (value.contains("speed"))
        {
            auto& val = value["speed"];
            if (val.is_number()) 
                m_speed = val.get<imgui_json::number>();
        }
        if (value.contains("strength"))
        {
            auto& val = value["strength"];
            if (val.is_number()) 
                m_strength = val.get<imgui_json::number>();
        }
        if (value.contains("density"))
        {
            auto& val = value["density"];
            if (val.is_number()) 
                m_density = val.get<imgui_json::number>();
        }
        if (value.contains("horizontal"))
        {
            auto& val = value["horizontal"];
            if (val.is_boolean()) 
                m_horizontal = val.get<imgui_json::boolean>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["speed"] = imgui_json::number(m_speed);
        value["strength"] = imgui_json::number(m_strength);
        value["density"] = imgui_json::number(m_density);
        value["horizontal"] = imgui_json::boolean(m_horizontal);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue176"));
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
    FloatPin  m_SpeedIn  = { this, "Speed" };
    FloatPin  m_StrengthIn  = { this, "Strength" };
    FloatPin  m_DensityIn  = { this, "Density" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[5] = { &m_Enter, &m_MatIn, &m_SpeedIn, &m_StrengthIn, &m_DensityIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    float m_speed           {20};
    float m_strength        {20};
    float m_density         {20};
    bool m_horizontal       {true};
    ImGui::Sway_vulkan * m_effect   {nullptr};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(SwayEffectNode, "Sway Effect", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Effect")