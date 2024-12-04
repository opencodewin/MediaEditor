#pragma once
#include <imgui.h>
namespace BluePrint
{
struct SystemEntryPointNode final : Node
{
    BP_NODE(SystemEntryPointNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::EntryPoint, NodeStyle::Simple, "System")

    SystemEntryPointNode(BP* blueprint): Node(blueprint) { m_Name = "Start"; }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        return m_Exit;
    }

    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    FlowPin* GetOutputFlowPin() override { return &m_Exit; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }

    FlowPin m_Exit = { this, "Start" };

    Pin* m_OutputPins[1] = { &m_Exit };
};
} // namespace BluePrint

