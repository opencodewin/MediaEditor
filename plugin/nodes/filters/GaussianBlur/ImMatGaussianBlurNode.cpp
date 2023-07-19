#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "GaussianBlur.h"

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct GaussianBlurNode final : Node
{
    BP_NODE_WITH_NAME(GaussianBlurNode, "Gaussian Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Blur")
    GaussianBlurNode(BP* blueprint): Node(blueprint) { m_Name = "Gaussian Blur"; }

    ~GaussianBlurNode()
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
        if (m_RadiusIn.IsLinked()) m_blurRadius = context.GetPinValue<float>(m_RadiusIn);
        if (m_SigmaIn.IsLinked()) m_sigma = context.GetPinValue<float>(m_SigmaIn);
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
                m_filter = new ImGui::GaussianBlur_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            m_filter->SetParam(m_blurRadius, m_sigma);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_RadiusIn.m_ID) m_RadiusIn.SetValue(m_blurRadius);
        if (receiver.m_ID == m_SigmaIn.m_ID) m_SigmaIn.SetValue(m_sigma);
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
        float _sigma = m_sigma;
        int _blurRadius = m_blurRadius;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp; // ImGuiSliderFlags_NoInput
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_SigmaIn.IsLinked());
        ImGui::SliderFloat("Sigma##GaussianBlur", &_sigma, 0.0, 10.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_sigma##GaussianBlur")) { _sigma = 0; changed = true; }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithID("##add_curve_sigma##GaussianBlur", key, m_SigmaIn.IsLinked(), "sigma##GaussianBlur@" + std::to_string(m_ID), 0.f, 10.f, 0.f, m_SigmaIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_RadiusIn.IsLinked());
        ImGui::SliderInt("Radius##GaussianBlur", &_blurRadius, 0, 20, "%d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_radius##GaussianBlur")) { _blurRadius = 3; changed = true; }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithID("##add_curve_radius##GaussianBlur", key, m_RadiusIn.IsLinked(), "radius##GaussianBlur@" + std::to_string(m_ID), 0.f, 20.f, 3.f, m_RadiusIn.m_ID);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        if (_sigma != m_sigma) { m_sigma = _sigma; changed = true; }
        if (_blurRadius != m_blurRadius) { m_blurRadius = _blurRadius; changed = true; }
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
        if (value.contains("radius"))
        {
            auto& val = value["radius"];
            if (val.is_number()) 
                m_blurRadius = val.get<imgui_json::number>();
        }

        if (value.contains("sigma"))
        {
            auto& val = value["sigma"];
            if (val.is_number()) 
                m_sigma = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["radius"] = imgui_json::number(m_blurRadius);
        value["sigma"] = imgui_json::number(m_sigma);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\uf404"));
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
    FloatPin  m_RadiusIn = { this, "Radius"};
    FloatPin  m_SigmaIn = { this, "Sigma"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatIn, &m_RadiusIn, &m_SigmaIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    int m_blurRadius        {3};
    float m_sigma           {0.0f};
    ImGui::GaussianBlur_vulkan * m_filter   {nullptr};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(GaussianBlurNode, "Gaussian Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Blur")
