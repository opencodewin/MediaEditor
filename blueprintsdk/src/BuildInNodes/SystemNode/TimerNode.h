#pragma once
#include <imgui.h>
namespace BluePrint
{
struct TimerNode final : Node
{
    BP_NODE(TimerNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Default, "Flow")
    TimerNode(BP* blueprint): Node(blueprint) { m_Name = "Timer"; }
    
    void Reset(Context& context) override
    {
        Node::Reset(context);
        m_current_step = 0;
        m_current_ms = 0;
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        if (entryPoint.m_ID == m_Reset.m_ID)
        {
            m_current_step = 0;
            m_current_ms = 0;
            return {};
        }

        if (m_current_ms > 0)
        {
            uint64_t now_time = ImGui::get_current_time_msec();
            uint64_t delta_time = now_time - m_current_ms;
            if (delta_time >= m_interval_ms)
            {
                if (m_count < 0)
                {
                    m_current_ms = now_time - (delta_time - m_interval_ms);
                    context.PushReturnPoint(entryPoint);
                    return m_TimeOut;
                }
                else if (m_count > 0 && m_current_step < m_count)
                {
                    m_current_step ++;
                    m_current_ms = now_time - (delta_time - m_interval_ms);
                    context.PushReturnPoint(entryPoint);
                    return m_TimeOut;
                }
                else
                {
                    m_current_step = 0;
                    m_current_ms = 0;
                    return m_Exit;
                }
            }
            else
            {
                if (threading)
                    ImGui::sleep((int)(m_interval_ms - delta_time));
                else
                    ImGui::sleep(0);
            }
        }
        else
        {
            m_current_ms = ImGui::get_current_time_msec();
        }

        context.PushReturnPoint(entryPoint);
        
        return {};
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Set Node Name
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();

        // Draw Custom setting
        changed |= ImGui::InputInt("Timer interval", (int *)&m_interval_ms);
        changed |= ImGui::InputInt("Timer count", &m_count);
        return changed;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;

        if (value.contains("interval"))
        {
            auto& interval = value["interval"];
            m_interval_ms = interval.get<imgui_json::number>();
        }

        if (value.contains("count"))
        {
            auto& count = value["count"];
            m_count = count.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) override
    {
        Node::Save(value, MapID);
        value["interval"]   = imgui_json::number(m_interval_ms);
        value["count"]      = imgui_json::number(m_count);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }

    FlowPin  m_Enter    = { this, "Enter" };
    FlowPin  m_Reset    = { this, "Reset" };
    FlowPin  m_Exit     = { this, "Exit" };
    FlowPin  m_TimeOut  = { this, "Event" };

    Pin* m_InputPins[2] = { &m_Enter, &m_Reset };
    Pin* m_OutputPins[2] = { &m_Exit, &m_TimeOut };

    uint32_t m_interval_ms   {0};
    int32_t m_count         {-1};
    uint32_t m_current_step  {0};
    uint64_t m_current_ms    {0};
};
} // namespace BluePrint
