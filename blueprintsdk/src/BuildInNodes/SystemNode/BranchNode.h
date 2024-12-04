#pragma once
#include <imgui.h>

namespace BluePrint
{
struct BranchNode final : Node
{
    BP_NODE(BranchNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Default, "Flow")

    BranchNode(BP* blueprint): Node(blueprint) { m_Name = "Branch"; }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto value = context.GetPinValue<bool>(m_Condition);
        if (value)
            return m_True;
        else
            return m_False;
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_False; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_Condition}; }

    FlowPin m_Enter     = { this, "Enter" };
    BoolPin m_Condition = { this, "Condition" };
    FlowPin m_True      = { this, "True" };
    FlowPin m_False     = { this, "False" };

    Pin* m_InputPins[2] = { &m_Enter, &m_Condition };
    Pin* m_OutputPins[2] = { &m_True, &m_False };
};
} // namespace BluePrint

