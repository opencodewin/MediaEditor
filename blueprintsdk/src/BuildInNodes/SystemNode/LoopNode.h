#pragma once
#include <imgui.h>
namespace BluePrint
{
struct LoopNode final : Node
{
    BP_NODE(LoopNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Default, "Flow")

    LoopNode(BP* blueprint): Node(blueprint) { m_Name = "Loop"; }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        auto firstIndex = context.GetPinValue<int32_t>(m_FirstIndex);
        context.SetPinValue(m_Index, firstIndex);
        m_current_index = firstIndex;
    }
    
    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        if (entryPoint.m_ID == m_Reset.m_ID)
        {
            Reset(context);
            return {};
        }

        //auto index      = context.GetPinValue<int32_t>(m_Index);
        auto lastIndex  = context.GetPinValue<int32_t>(m_LastIndex);
        auto step       = context.GetPinValue<int32_t>(m_Step);
        if (m_current_index <= lastIndex)
        {
            context.SetPinValue(m_Index, m_current_index);
            m_current_index += step;
            context.PushReturnPoint(entryPoint);
            std::this_thread::yield();
            return m_LoopBody;
        }
        Reset(context);
        return m_Completed;
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Completed; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_FirstIndex, &m_LastIndex, &m_Step}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_Index}; }

    FlowPin  m_Enter      = { this, "Enter" };
    Int32Pin m_FirstIndex = { this, "From" };
    Int32Pin m_LastIndex  = { this, "To" };
    Int32Pin m_Step       = { this, "Step", 1 };
    FlowPin  m_Reset      = { this, "Reset" };
    FlowPin  m_LoopBody   = { this, "Loop Body" };
    Int32Pin m_Index      = { this, "Index" };
    FlowPin  m_Completed  = { this, "Completed" };

    Pin* m_InputPins[5] = { &m_Enter, &m_FirstIndex, &m_LastIndex, &m_Step, &m_Reset };
    Pin* m_OutputPins[3] = { &m_LoopBody, &m_Index, &m_Completed };

    int32_t m_current_index {0};
};
} // namespace BluePrint
