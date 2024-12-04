#pragma once
#include <imgui.h>
namespace BluePrint
{
struct SystemExitPointNode final : Node
{
    BP_NODE(SystemExitPointNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::ExitPoint, NodeStyle::Simple, "System")

    SystemExitPointNode(BP* blueprint): Node(blueprint) { m_Name = "End"; }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        context.m_Callstack.clear();
        return {};
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }

    FlowPin m_Enter = { this, "End" };

    Pin* m_InputPins[1] = { &m_Enter };
};
} // namespace BluePrint
