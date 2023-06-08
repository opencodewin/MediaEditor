#include <UI.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "ALM_vulkan.h"

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct AlmNode final : Node
{
    BP_NODE_WITH_NAME(AlmNode, "ALM Enhancement", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Enhance")
    AlmNode(BP* blueprint): Node(blueprint) { m_Name = "ALM Enhancement"; }
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

    void DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        ImGui::TextUnformatted("Mat Type:"); ImGui::SameLine();
        ImGui::RadioButton("AsInput", (int *)&m_mat_data_type, (int)IM_DT_UNDEFINED); ImGui::SameLine();
        ImGui::RadioButton("Int8", (int *)&m_mat_data_type, (int)IM_DT_INT8); ImGui::SameLine();
        ImGui::RadioButton("Int16", (int *)&m_mat_data_type, (int)IM_DT_INT16); ImGui::SameLine();
        ImGui::RadioButton("Float16", (int *)&m_mat_data_type, (int)IM_DT_FLOAT16); ImGui::SameLine();
        ImGui::RadioButton("Float32", (int *)&m_mat_data_type, (int)IM_DT_FLOAT32);
    }

    bool CustomLayout() const override { return true; }
    bool Skippable() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        float _strength = m_strength;
        float _bias = m_bias;
        float _gamma = m_gamma;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp; // ImGuiSliderFlags_NoInput
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_StrengthIn.IsLinked());
        ImGui::SliderFloat("Strength##ALM", &_strength, 0, 1.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_strength##ALM")) { _strength = 0.5; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_strength##ALM", key, "strength##ALM", 0.f, 1.f, 0.5f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_BiasIn.IsLinked());
        ImGui::SliderFloat("Bias##ALM", &_bias, 0, 1.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_bias##ALM")) { _bias = 0.7; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_bias##ALM", key, "bias##ALM", 0.f, 1.f, 0.7f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_GammaIn.IsLinked());
        ImGui::SliderFloat("Gamma##ALM", &_gamma, 0, 4.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_gamma##ALM")) { _gamma = 2.2; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_gamma##ALM", key, "gamma##ALM", 0.f, 4.f, 2.2f);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
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
