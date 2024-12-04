#pragma once
#include <imgui.h>
#include <imgui_extra_widget.h>

namespace BluePrint
{
struct FloatCountNode final : Node
{
    BP_NODE(FloatCountNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Default, "Flow")

    FloatCountNode(BP* blueprint): Node(blueprint) { m_Name = "Float Count"; }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        context.SetPinValue(m_Counter, 0.f);
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        if (entryPoint.m_ID == m_Reset.m_ID)
        {
            Reset(context);
            return {};
        }

        auto n = context.GetPinValue<float>(m_N);
        auto c = context.GetPinValue<float>(m_Counter);
        auto s = context.GetPinValue<float>(m_Step);
        if (c >= n)
        {
            Reset(context);
            return m_Completed;
        }

        context.SetPinValue(m_Counter, c + s);

        if (!m_Accumulate)
        {
            context.PushReturnPoint(entryPoint);
            std::this_thread::yield();
        }
        return m_Exit;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Set Node Name
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();

        // Draw Custom Setting
        ImGui::SetCurrentContext(ctx);
        ImGui::TextUnformatted("Accumulate"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_acc", &m_Accumulate);
        return changed;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;

        if (value.contains("accumulate"))
        {
            auto& val = value["accumulate"];
            if (val.is_boolean())
            {
                m_Accumulate = val.get<imgui_json::boolean>();
            }
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) override
    {
        Node::Save(value, MapID);
        value["accumulate"] = imgui_json::boolean(m_Accumulate);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_N, &m_Step}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_Counter}; }

    FlowPin  m_Enter   = { this, "Enter" };
    FlowPin  m_Reset   = { this, "Reset" };
    FloatPin m_N       = { this, "N", 0.f };
    FloatPin m_Step    = { this, "Step", 1.f };
    FlowPin  m_Exit    = { this, "Exit" };
    FloatPin m_Counter = { this, "Counter" };
    FlowPin  m_Completed  = { this, "Completed" };

    Pin* m_InputPins[4] = { &m_Enter, &m_N, &m_Step, &m_Reset };
    Pin* m_OutputPins[3] = { &m_Exit, &m_Counter, &m_Completed };
    bool m_Accumulate {false};
};
} // namespace BluePrint
