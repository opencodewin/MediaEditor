#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "Star_vulkan.h"
#define NODE_VERSION    0x01000100

namespace BluePrint
{
struct StarEffectNode final : Node
{
    BP_NODE_WITH_NAME(StarEffectNode, "Star Effect", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Effect")
    StarEffectNode(BP* blueprint): Node(blueprint) { m_Name = "Star Effect"; m_HasCustomLayout = true; m_Skippable = true; }

    ~StarEffectNode()
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
                m_effect = new ImGui::Star_vulkan(gpu);
            }
            if (!m_effect)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_effect->effect(mat_in, im_RGB, time, m_speed, m_layers, m_color);
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
        int _layers = m_layers;
        ImPixel _color = m_color;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_SpeedIn.IsLinked());
        ImGui::SliderFloat("Speed##Star", &_speed, 0.0, 100.f, "%.0f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_speed##Star")) { _speed = 10.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_speed##Star", key, ImGui::ImCurveEdit::DIM_X, m_SpeedIn.IsLinked(), "speed##Star@" + std::to_string(m_ID), 0.0f, 100.f, 1.f, m_SpeedIn.m_ID);
        ImGui::EndDisabled();
        ImGui::SliderInt("Layers##Star", &_layers, 2, 20, "%d", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_layers##Star")) { _layers = 2; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::PopItemWidth();
        if (ImGui::ColorEdit4("Color##Star", (float*)&_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
        {
            m_color = _color; changed = true;
        } ImGui::SameLine(); ImGui::TextUnformatted("Shadow Color");
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_color##DStar")) { m_color = {1.0f, 0.1f, 0.9f, 1.0f}; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::PopStyleColor();
        if (_speed != m_speed) { m_speed = _speed; changed = true; }
        if (_layers != m_layers) { m_layers = _layers; changed = true; }
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
        if (value.contains("layers"))
        {
            auto& val = value["layers"];
            if (val.is_number()) 
                m_layers = val.get<imgui_json::number>();
        }
        if (value.contains("color"))
        {
            auto& val = value["color"];
            if (val.is_vec4())
            {
                ImVec4 val4 = val.get<imgui_json::vec4>();
                m_color = ImPixel(val4.x, val4.y, val4.z, val4.w);
            }
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["speed"] = imgui_json::number(m_speed);
        value["layers"] = imgui_json::number(m_layers);
        value["color"] = imgui_json::vec4(ImVec4(m_color.r, m_color.g, m_color.b, m_color.a));
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue3ea"));
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
    FloatPin  m_SpeedIn = { this, "Speed" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[3] = { &m_Enter, &m_MatIn, &m_SpeedIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    float m_speed           {10.f};
    int m_layers            {2};
    ImPixel m_color         {1.0f, 0.1f, 0.9f, 1.0f};
    ImGui::Star_vulkan * m_effect   {nullptr};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(StarEffectNode, "Star Effect", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Effect")