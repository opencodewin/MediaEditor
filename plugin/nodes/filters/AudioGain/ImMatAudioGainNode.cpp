#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct AudioGainNode final : Node
{
    BP_NODE_WITH_NAME(AudioGainNode, "Audio Gain", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Filter#Audio")
    AudioGainNode(BP* blueprint): Node(blueprint) { m_Name = "Audio Gain"; m_HasCustomLayout = true; m_Skippable = true; }

    ~AudioGainNode()
    {
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
        if (m_GainIn.IsLinked())
        {
            m_gain = context.GetPinValue<float>(m_GainIn);
        }
        if (!mat_in.empty())
        {
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            ImGui::ImMat im_mat;
            im_mat = mat_in * m_gain;
            im_mat.clip(-1.f, 1.f);
            im_mat.copy_attribute(mat_in);
            m_MatOut.SetValue(im_mat);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_GainIn.m_ID)
        {
            m_GainIn.SetValue(m_gain);
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
        float val = m_gain;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Stick;
        ImGui::PushStyleColor(ImGuiCol_Button, 0);
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_GainIn.IsLinked());
        ImGui::SliderFloat("##slider_gain##Gain", &val, 0.0, 2.f, "%.2f", flags);
        ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RESET "##reset_gain##Gain")) { val = 1.0; }
        ImGui::ShowTooltipOnHover("Reset");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        if (pCurve) ImGui::ImCurveCheckEditKeyWithIDByDim("##add_curve_gain##Gain", pCurve, ImGui::ImCurveEdit::DIM_X, m_GainIn.IsLinked(), "gain##Gain@" + std::to_string(m_ID), 0.f, 2.f, 1.f, m_GainIn.m_ID);
        ImGui::EndDisabled();
        if (val != m_gain) { m_gain = val; changed = true; }
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
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
        if (value.contains("gain"))
        {
            auto& val = value["gain"];
            if (val.is_number()) 
                m_gain = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["gain"] = imgui_json::number(m_gain);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue4e8"));
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
    FloatPin  m_GainIn   = { this, "Gain"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[3] = { &m_Enter, &m_MatIn, &m_GainIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    float m_gain        {1.0f};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(AudioGainNode, "Audio Gain", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Filter#Audio")
