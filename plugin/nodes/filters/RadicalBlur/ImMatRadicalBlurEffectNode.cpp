#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "RadicalBlur_vulkan.h"
#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct RadicalBlurEffectNode final : Node
{
    BP_NODE_WITH_NAME(RadicalBlurEffectNode, "Radical Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Blur")
    RadicalBlurEffectNode(BP* blueprint): Node(blueprint) { m_Name = "Radical Blur"; m_HasCustomLayout = true; m_Skippable = true; }

    ~RadicalBlurEffectNode()
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
        if (m_RadiusIn.IsLinked()) m_radius = context.GetPinValue<float>(m_RadiusIn);
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
                m_effect = new ImGui::RadicalBlur_vulkan(gpu);
            }
            if (!m_effect)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_effect->effect(mat_in, im_RGB, m_radius, m_dist, m_intensity, m_count);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_RadiusIn.m_ID)
        {
            m_RadiusIn.SetValue(m_radius);
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
        float _radius = m_radius;
        float _dist = m_dist;
        float _intensity = m_intensity;
        float _count = m_count;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_RadiusIn.IsLinked());
        ImGui::SliderFloat("Radius##RadicalBlur", &_radius, 0.0, 1.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_radius##RadicalBlur")) { _radius = 0.38f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (key) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_radius##RadicalBlur", key, ImGui::ImCurveEdit::DIM_X, m_RadiusIn.IsLinked(), "radius##RadicalBlur@" + std::to_string(m_ID), 0.0f, 1.f, 0.38f, m_RadiusIn.m_ID);
        ImGui::EndDisabled();
        ImGui::SliderFloat("Dist##RadicalBlur", &_dist, 0.0, 1.f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_dist##RadicalBlur")) { _dist = 0.25f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderFloat("Intensity##RadicalBlur", &_intensity, 0.0, 1.f, "%.0f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_intensity##RadicalBlur")) { _intensity = 1.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderFloat("Count##RadicalBlur", &_count, 1.f, 40.f, "%.0f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_count##RadicalBlur")) { _count = 40.f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_radius != m_radius) { m_radius = _radius; changed = true; }
        if (_dist != m_dist) { m_dist = _dist; changed = true; }
        if (_intensity != m_intensity) { m_intensity = _intensity; changed = true; }
        if (_count != m_count) { m_count = _count; changed = true; }
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
                m_radius = val.get<imgui_json::number>();
        }
        if (value.contains("dist"))
        {
            auto& val = value["dist"];
            if (val.is_number()) 
                m_dist = val.get<imgui_json::number>();
        }
        if (value.contains("intensity"))
        {
            auto& val = value["intensity"];
            if (val.is_number()) 
                m_intensity = val.get<imgui_json::number>();
        }
        if (value.contains("count"))
        {
            auto& val = value["count"];
            if (val.is_number()) 
                m_count = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["radius"] = imgui_json::number(m_radius);
        value["dist"] = imgui_json::number(m_dist);
        value["intensity"] = imgui_json::number(m_intensity);
        value["count"] = imgui_json::number(m_count);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue01e"));
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
    FloatPin  m_RadiusIn  = { this, "Radius" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[3] = { &m_Enter, &m_MatIn, &m_RadiusIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    float m_radius          {0.38f};
    float m_dist            {0.25f};
    float m_intensity       {1.f};
    float m_count           {40.f};
    ImGui::RadicalBlur_vulkan * m_effect   {nullptr};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(RadicalBlurEffectNode, "Radical Blur", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Blur")