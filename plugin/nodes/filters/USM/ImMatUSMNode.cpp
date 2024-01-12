#include <UI.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "USM_vulkan.h"

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct USMNode final : Node
{
    BP_NODE_WITH_NAME(USMNode, "USM Sharpen", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Enhance")
    USMNode(BP* blueprint): Node(blueprint) { m_Name = "USM Sharpen"; m_HasCustomLayout = true; m_Skippable = true; }
    ~USMNode()
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
        if (m_SigmaIn.IsLinked()) m_sigma = context.GetPinValue<float>(m_SigmaIn);
        if (m_ThresholdIn.IsLinked()) m_threshold = context.GetPinValue<float>(m_ThresholdIn);
        if (m_AmountIn.IsLinked()) m_amount = context.GetPinValue<float>(m_AmountIn);
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
                m_filter = new ImGui::USM_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, m_sigma, m_amount, m_threshold);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_SigmaIn.m_ID) m_SigmaIn.SetValue(m_sigma);
        if (receiver.m_ID == m_ThresholdIn.m_ID) m_ThresholdIn.SetValue(m_threshold);
        if (receiver.m_ID == m_AmountIn.m_ID) m_AmountIn.SetValue(m_amount);
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
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        float _sigma = m_sigma;
        float _amount = m_amount;
        float _threshold = m_threshold;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_SigmaIn.IsLinked());
        ImGui::SliderFloat("Sigma##USM", &_sigma, 0, 10.f, "%.1f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_sigma##USM")) { _sigma = 3; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_sigma##USM", key, ImGui::ImCurveEdit::DIM_X, m_SigmaIn.IsLinked(), "sigma##USM@" + std::to_string(m_ID), 0.f, 10.f, 3.f, m_SigmaIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_AmountIn.IsLinked());
        ImGui::SliderFloat("Amount##USM", &_amount, 0, 3.f, "%.1f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_amount##USM")) { _amount = 1.5f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_amount##USM", key, ImGui::ImCurveEdit::DIM_X, m_AmountIn.IsLinked(), "amount##USM@" + std::to_string(m_ID), 0.f, 3.f, 1.5f, m_AmountIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_ThresholdIn.IsLinked());
        ImGui::SliderFloat("Threshold##USM", &_threshold, 0, 1.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_threshold##USM")) { _threshold = 1.0f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_threshold##USM", key, ImGui::ImCurveEdit::DIM_X, m_ThresholdIn.IsLinked(), "threshold##USM@" + std::to_string(m_ID), 0.f, 1.f, 1.f, m_ThresholdIn.m_ID);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (m_sigma != _sigma) { m_sigma = _sigma; changed = true; }
        if (m_amount != _amount) { m_amount = _amount; changed = true; }
        if (m_threshold != _threshold) { m_threshold = _threshold; changed = true; }
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
        if (value.contains("sigma"))
        {
            auto& val = value["sigma"];
            if (val.is_number()) 
                m_sigma = val.get<imgui_json::number>();
        }
        if (value.contains("threshold"))
        {
            auto& val = value["threshold"];
            if (val.is_number()) 
                m_threshold = val.get<imgui_json::number>();
        }
        if (value.contains("amount"))
        {
            auto& val = value["amount"];
            if (val.is_number()) 
                m_amount = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["sigma"] = imgui_json::number(m_sigma);
        value["threshold"] = imgui_json::number(m_threshold);
        value["amount"] = imgui_json::number(m_amount);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue919"));
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
    FloatPin  m_SigmaIn = { this, "Sigma"};
    FloatPin  m_ThresholdIn = { this, "Threshold"};
    FloatPin  m_AmountIn = { this, "Amount"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[5] = { &m_Enter, &m_MatIn, &m_SigmaIn, &m_AmountIn, &m_ThresholdIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    float m_sigma       {3.f};
    float m_threshold   {1.f};
    float m_amount      {1.5f};
    ImGui::USM_vulkan * m_filter {nullptr};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(USMNode, "USM Sharpen", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Enhance")
