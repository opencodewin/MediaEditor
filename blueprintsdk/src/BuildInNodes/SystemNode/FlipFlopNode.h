#pragma once
#include <imgui.h>

namespace BluePrint
{
struct FlipFlopNode final : Node
{
    BP_NODE(FlipFlopNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Default, "Flow")

    FlipFlopNode(BP* blueprint): Node(blueprint) { m_Name = "Flip Flop"; }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        context.SetPinValue(m_IsA, false);
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto isA = !context.GetPinValue<bool>(m_IsA);
        context.SetPinValue(m_IsA, isA);
        if (isA)
            return m_A;
        else
            return m_B;
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_IsA; }

    FlowPin m_Enter = { this, "Enter" };
    FlowPin m_A     = { this, "A" };
    FlowPin m_B     = { this, "B" };
    BoolPin m_IsA   = { this, "Is A" };

    Pin* m_InputPins[1] = { &m_Enter };
    Pin* m_OutputPins[3] = { &m_A, &m_B, &m_IsA };
};
} // namespace BluePrint

