#include <UI.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "ALM_vulkan.h"

#define NODE_VERSION    0x01000100

namespace BluePrint
{
struct AlmNode final : Node
{
    BP_NODE_WITH_NAME(AlmNode, "ALM Enhancement", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Enhance")
    AlmNode(BP* blueprint): Node(blueprint) { m_Name = "ALM Enhancement"; m_HasCustomLayout = true; m_Skippable = true; }
    ~AlmNode()
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
        if (m_StrengthIn.IsLinked()) m_strength = context.GetPinValue<float>(m_StrengthIn);
        if (m_BiasIn.IsLinked()) m_bias = context.GetPinValue<float>(m_BiasIn);
        if (m_GammaIn.IsLinked()) m_gamma = context.GetPinValue<float>(m_GammaIn);
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
                m_filter = new ImGui::ALM_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            m_filter->SetParam(m_strength, m_bias, m_gamma);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_StrengthIn.m_ID) m_StrengthIn.SetValue(m_strength);
        if (receiver.m_ID == m_BiasIn.m_ID) m_BiasIn.SetValue(m_bias);
        if (receiver.m_ID == m_GammaIn.m_ID) m_GammaIn.SetValue(m_gamma);
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        changed |= Node::DrawDataTypeSetting("Mat Type:", m_mat_data_type);
        return changed;
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve* pCurve, bool embedded) override
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
        float _strength = m_strength;
        float _bias = m_bias;
        float _gamma = m_gamma;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_StrengthIn.IsLinked());
        ImGui::SliderFloat("Strength##ALM", &_strength, 0, 1.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_strength##ALM")) { _strength = 0.5; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (pCurve) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_strength##ALM", pCurve, ImGui::ImCurveEdit::DIM_X, m_StrengthIn.IsLinked(), "strength##ALM" + std::to_string(m_ID), 0.f, 1.f, 0.5f, m_StrengthIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_BiasIn.IsLinked());
        ImGui::SliderFloat("Bias##ALM", &_bias, 0, 1.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_bias##ALM")) { _bias = 0.7; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (pCurve) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_bias##ALM", pCurve, ImGui::ImCurveEdit::DIM_X, m_BiasIn.IsLinked(), "bias##ALM" + std::to_string(m_ID), 0.f, 1.f, 0.7f, m_BiasIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_GammaIn.IsLinked());
        ImGui::SliderFloat("Gamma##ALM", &_gamma, 0, 4.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_gamma##ALM")) { _gamma = 2.2; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (pCurve) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_gamma##ALM", pCurve, ImGui::ImCurveEdit::DIM_X, m_GammaIn.IsLinked(), "gamma##ALM" + std::to_string(m_ID), 0.f, 4.f, 2.2f, m_GammaIn.m_ID);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_strength != m_strength) { m_strength = _strength; changed = true; }
        if (_bias != m_bias) { m_bias = _bias; changed = true; }
        if (_gamma != m_gamma) { m_gamma = _gamma; changed = true; }
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
        if (value.contains("strength"))
        {
            auto& val = value["strength"];
            if (val.is_number()) 
                m_strength = val.get<imgui_json::number>();
        }
        if (value.contains("bias"))
        {
            auto& val = value["bias"];
            if (val.is_number()) 
                m_bias = val.get<imgui_json::number>();
        }
        if (value.contains("gamma"))
        {
            auto& val = value["gamma"];
            if (val.is_number()) 
                m_gamma = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["strength"] = imgui_json::number(m_strength);
        value["bias"] = imgui_json::number(m_bias);
        value["gamma"] = imgui_json::number(m_gamma);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue42e"));
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
    FloatPin  m_StrengthIn = { this, "Strength"};
    FloatPin  m_BiasIn  = { this, "Bias"};
    FloatPin  m_GammaIn = { this, "Gamma"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[5] = { &m_Enter, &m_MatIn, &m_StrengthIn, &m_BiasIn, &m_GammaIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    float m_strength    {0.5};
    float m_bias        {0.7};
    float m_gamma       {2.2};
    ImGui::ALM_vulkan * m_filter {nullptr};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(AlmNode, "ALM Enhancement", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Enhance")
