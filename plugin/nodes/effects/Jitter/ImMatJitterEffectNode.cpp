#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "Jitter_vulkan.h"
#define NODE_VERSION    0x01000100

namespace BluePrint
{
struct JitterEffectNode final : Node
{
    BP_NODE_WITH_NAME(JitterEffectNode, "Jitter Effect", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Video#Effect")
    JitterEffectNode(BP* blueprint): Node(blueprint) { m_Name = "Jitter Effect"; }

    ~JitterEffectNode()
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
                m_effect = new ImGui::Jitter_vulkan(gpu);
            }
            if (!m_effect)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_effect->effect(mat_in, im_RGB, time, m_count, m_max_scale, m_offset, m_shrink);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
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
        int _count = m_count;
        float _max_scale = m_max_scale;
        float _offset = m_offset;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp; // ImGuiSliderFlags_NoInput
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::SliderInt("Count##Jitter", &_count, 1, 10, "%d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_count##Jitter")) { _count = 1; changed = true; }
        ImGui::SliderFloat("Max Scale##Jitter", &_max_scale, 1.f, 2.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_max_scale##Jitter")) { _max_scale = 1.1f; changed = true; }
        ImGui::SliderFloat("Offset##Jitter", &_offset, 0.f, 0.1f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_offset##Jitter")) { _offset = 0.02f; changed = true; }
        if (ImGui::Checkbox("Shrink##Jitter", &m_shrink)) { changed = true; }
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        if (_count != m_count) { m_count = _count; changed = true; }
        if (_max_scale != m_max_scale) { m_max_scale = _max_scale; changed = true; }
        if (_offset != m_offset) { m_offset = _offset; changed = true; }
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
        if (value.contains("offset"))
        {
            auto& val = value["offset"];
            if (val.is_number()) 
                m_offset = val.get<imgui_json::number>();
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
        value["offset"] = imgui_json::number(m_offset);
        value["shrink"] = imgui_json::boolean(m_shrink);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\uf198"));
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
    float m_max_scale       {1.1};
    float m_offset          {0.02};
    bool m_shrink           {false};
    ImGui::Jitter_vulkan * m_effect   {nullptr};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(JitterEffectNode, "Jitter Effect", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Video#Effect")