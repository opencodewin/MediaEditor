#include <UI.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "Canny_vulkan.h"

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct CannyNode final : Node
{
    BP_NODE_WITH_NAME(CannyNode, "Canny Edge", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Edge")
    CannyNode(BP* blueprint): Node(blueprint) { m_Name = "Canny Edge"; m_HasCustomLayout = true; m_Skippable = true; }
    ~CannyNode()
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
        if (m_MinIn.IsLinked()) m_minThreshold = context.GetPinValue<float>(m_MinIn);
        if (m_MaxIn.IsLinked()) m_maxThreshold = context.GetPinValue<float>(m_MaxIn);
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
                m_filter = new ImGui::Canny_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, m_blurRadius, m_minThreshold, m_maxThreshold);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_RadiusIn.m_ID) m_RadiusIn.SetValue(m_blurRadius);
        if (receiver.m_ID == m_MinIn.m_ID) m_MinIn.SetValue(m_minThreshold);
        if (receiver.m_ID == m_MinIn.m_ID) m_MaxIn.SetValue(m_maxThreshold);
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
        int _blurRadius = m_blurRadius;
        float _minThreshold = m_minThreshold;
        float _maxThreshold = m_maxThreshold;
        ImGui::Dummy(ImVec2(160, 8));
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(160);
        ImGui::BeginDisabled(!m_Enabled || m_RadiusIn.IsLinked());
        ImGui::SliderInt("Blur Radius##Canny", &_blurRadius, 0, 10, "%d", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_radius##Canny")) { _blurRadius = 3; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_radius##Canny", key, ImGui::ImCurveEdit::DIM_X, m_RadiusIn.IsLinked(), "radius##Canny@" + std::to_string(m_ID), 0.f, 10.f, 3.f, m_RadiusIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_MinIn.IsLinked());
        ImGui::SliderFloat("Min Threshold##Canny", &_minThreshold, 0, 1.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_min##Canny")) { _minThreshold = 0.1; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_min##Canny", key, ImGui::ImCurveEdit::DIM_X, m_MinIn.IsLinked(), "min##Canny@" + std::to_string(m_ID), 0.f, 1.f, 0.1f, m_MinIn.m_ID);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_MaxIn.IsLinked());
        ImGui::SliderFloat("Max Threshold##Canny", &_maxThreshold, _minThreshold, 1.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_max##Canny")) { _maxThreshold = 0.45; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_max##Canny", key, ImGui::ImCurveEdit::DIM_X, m_MaxIn.IsLinked(), "max##Canny@" + std::to_string(m_ID), 0.f, 1.f, 0.45f, m_MaxIn.m_ID);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (m_blurRadius != _blurRadius) { m_blurRadius = _blurRadius; changed = true; }
        if (m_minThreshold != _minThreshold) { m_minThreshold = _minThreshold; changed = true; }
        if (m_maxThreshold != _maxThreshold) { m_maxThreshold = _maxThreshold; changed= true; }
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
        if (value.contains("Radius"))
        {
            auto& val = value["Radius"];
            if (val.is_number()) 
                m_blurRadius = val.get<imgui_json::number>();
        }
        if (value.contains("minThreshold"))
        {
            auto& val = value["minThreshold"];
            if (val.is_number()) 
                m_minThreshold = val.get<imgui_json::number>();
        }
        if (value.contains("maxThreshold"))
        {
            auto& val = value["maxThreshold"];
            if (val.is_number()) 
                m_maxThreshold = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["Radius"] = imgui_json::number(m_blurRadius);
        value["minThreshold"] = imgui_json::number(m_minThreshold);
        value["maxThreshold"] = imgui_json::number(m_maxThreshold);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue155"));
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
    FloatPin  m_RadiusIn = { this, "Radius" };
    FloatPin  m_MinIn = { this, "Min Threshold" };
    FloatPin  m_MaxIn = { this, "Max Threshold" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[5] = { &m_Enter, &m_MatIn, &m_RadiusIn, &m_MinIn, &m_MaxIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    float m_minThreshold    {0.1};
    float m_maxThreshold    {0.45};
    int m_blurRadius        {3};
    ImGui::Canny_vulkan * m_filter {nullptr};
};
} //namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(CannyNode, "Canny Edge", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Edge")
