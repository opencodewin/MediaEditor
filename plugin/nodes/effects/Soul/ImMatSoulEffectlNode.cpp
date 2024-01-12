#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "Soul_vulkan.h"
#define NODE_VERSION    0x01000100

namespace BluePrint
{
struct SoulEffectNode final : Node
{
    BP_NODE_WITH_NAME(SoulEffectNode, "Soul Effect", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Effect")
    SoulEffectNode(BP* blueprint): Node(blueprint) { m_Name = "Soul Effect"; m_HasCustomLayout = true; m_Skippable = true; }

    ~SoulEffectNode()
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
                m_effect = new ImGui::Soul_vulkan(gpu);
            }
            if (!m_effect)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_effect->effect(mat_in, im_RGB, time, m_count, m_max_scale, m_max_alpha, m_shrink);
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
        float setting_offset = 320;
        if (!embedded)
        {
            ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 sub_window_size = ImGui::GetWindowSize();
            setting_offset = sub_window_size.x - 80;
        }
        bool changed = false;
        int _count = m_count;
        float _max_scale = m_max_scale;
        float _max_alpha = m_max_alpha;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::SliderInt("Count##Soul", &_count, 1, 10, "%d", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_count##Soul")) { _count = 1; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderFloat("Max Scale##Soul", &_max_scale, 1.f, 2.f, "%.1f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_max_scale##Soul")) { _max_scale = 1.8f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::SliderFloat("Max Alpha##Soul", &_max_alpha, 0.f, 1.0f, "%.2f", flags);
        ImGui::SameLine(setting_offset);  if (ImGui::Button(ICON_RESET "##reset_max_alpha##Soul")) { _max_alpha = 0.4f; changed = true; }
        ImGui::ShowTooltipOnHover("Reset");
        if (ImGui::Checkbox("Shrink##Soul", &m_shrink)) { changed = true; }
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        if (_count != m_count) { m_count = _count; changed = true; }
        if (_max_scale != m_max_scale) { m_max_scale = _max_scale; changed = true; }
        if (_max_alpha != m_max_alpha) { m_max_alpha = _max_alpha; changed = true; }
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
        if (value.contains("count"))
        {
            auto& val = value["count"];
            if (val.is_number()) 
                m_count = val.get<imgui_json::number>();
        }
        if (value.contains("max_scale"))
        {
            auto& val = value["max_scale"];
            if (val.is_number()) 
                m_max_scale = val.get<imgui_json::number>();
        }
        if (value.contains("max_alpha"))
        {
            auto& val = value["max_alpha"];
            if (val.is_number()) 
                m_max_alpha = val.get<imgui_json::number>();
        }
        if (value.contains("shrink"))
        {
            auto& val = value["shrink"];
            if (val.is_boolean()) 
                m_shrink = val.get<imgui_json::boolean>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["count"] = imgui_json::number(m_count);
        value["max_scale"] = imgui_json::number(m_max_scale);
        value["max_alpha"] = imgui_json::number(m_max_alpha);
        value["shrink"] = imgui_json::boolean(m_shrink);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue162"));
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
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    int m_count             {1};
    float m_max_scale       {1.8};
    float m_max_alpha       {0.4};
    bool m_shrink           {false};
    ImGui::Soul_vulkan * m_effect   {nullptr};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(SoulEffectNode, "Soul Effect", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Effect")