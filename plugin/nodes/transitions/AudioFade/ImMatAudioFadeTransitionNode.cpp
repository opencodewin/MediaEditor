#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>

#define NODE_VERSION    0x01000000

namespace BluePrint
{
struct AudioFadeNode final : Node
{
    BP_NODE_WITH_NAME(AudioFadeNode, "Audio Fade", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, NodeType::External, NodeStyle::Default, "Transition#Audio")
    AudioFadeNode(BP* blueprint): Node(blueprint) { m_Name = "Audio Fade"; m_Skippable = true; }

    ~AudioFadeNode()
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
        auto mat_first = context.GetPinValue<ImGui::ImMat>(m_MatInFirst);
        auto mat_second = context.GetPinValue<ImGui::ImMat>(m_MatInSecond);
        float fade = 1.0f - context.GetPinValue<float>(m_FadeIn);
        if (!mat_first.empty() && !mat_second.empty())
        {
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_first);
                return m_Exit;
            }
            ImGui::ImMat im_mat;
            im_mat = mat_first * fade + mat_second * (1 - fade);
            im_mat.clip(-1.f, 1.f);
            im_mat.copy_attribute(mat_first);
            m_MatOut.SetValue(im_mat);
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
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const override
    {
        Node::DrawNodeLogo(ctx, size, std::string(u8"\ue487"));
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatInFirst, &m_MatInSecond, &m_FadeIn}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter       = { this, "Enter" };
    FlowPin   m_Exit        = { this, "Exit" };
    MatPin    m_MatInFirst  = { this, "In 1" };
    MatPin    m_MatInSecond = { this, "In 1" };
    FloatPin  m_FadeIn      = { this, "Fade"};
    MatPin    m_MatOut      = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_FadeIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
};
} // namespace BluePrint

BP_NODE_DYNAMIC_WITH_NAME(AudioFadeNode, "Audio Fade", "CodeWin", NODE_VERSION, VERSION_BLUEPRINT_API, BluePrint::NodeType::External, BluePrint::NodeStyle::Default, "Transition#Audio")
