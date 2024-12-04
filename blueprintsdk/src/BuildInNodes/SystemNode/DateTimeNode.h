#pragma once
#include <imgui.h>
namespace BluePrint
{
struct DateTimeNode final : Node
{
    enum DATETIME_FLAGS : int32_t
    {
        DATETIME_STAMP      =      0,
        DATETIME_COUNT      = 1 << 0,
        DATETIME_COUNT_FLOAT= 1 << 1,
        DATETIME_YEAR       = 1 << 2,
        DATETIME_MONTH      = 1 << 3,
        DATETIME_YEAR_DAY   = 1 << 4,
        DATETIME_MONTH_DAY  = 1 << 5,
        DATETIME_WEEK_DAY   = 1 << 6,
        DATETIME_HOUR       = 1 << 7,
        DATETIME_MIN        = 1 << 8,
        DATETIME_SEC        = 1 << 9,
        DATETIME_MSEC       = 1 << 10,
        DATETIME_USEC       = 1 << 11,
        DATETIME_ZONE       = 1 << 12,
    };
    BP_NODE(DateTimeNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Default, "Flow")
    DateTimeNode(BP* blueprint): Node(blueprint) { m_Name = "Date Time"; }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        context.SetPinValue(m_count, 0);
        context.SetPinValue(m_count_float, 0);
        m_start_time = ImGui::get_current_time_usec();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        int64_t hi_time = ImGui::get_current_time_usec();
        int64_t usec = hi_time - (hi_time / 1000000) * 1000000;
        int64_t msec = usec / 1000;
        usec = usec % 1000;
        std::time_t t = std::time(0);
        std::tm* now = std::localtime(&t);
        time_t clock = mktime(now);
        context.SetPinValue(m_Year, now->tm_year + 1900);
        context.SetPinValue(m_Month, now->tm_mon + 1);
        context.SetPinValue(m_DayYear, now->tm_yday + 1);
        context.SetPinValue(m_DayMonth, now->tm_mday);
        context.SetPinValue(m_DayWeek, now->tm_wday + 1);
        context.SetPinValue(m_Hour, now->tm_hour);
        context.SetPinValue(m_Min, now->tm_min);
        context.SetPinValue(m_Sec, now->tm_sec);
        context.SetPinValue(m_mSec, (int32_t)msec);
        context.SetPinValue(m_uSec, (int32_t)usec);
        context.SetPinValue(m_TimeStamp, hi_time);

        auto count_time = hi_time - m_start_time;
        context.SetPinValue(m_count, (int32_t)count_time);
        context.SetPinValue(m_count_float, (float)count_time / 1000000.f);
#ifdef _WIN32
        context.SetPinValue(m_Zone, string("C"));
#else
        context.SetPinValue(m_Zone, string(now->tm_zone));
#endif
        return m_Exit;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Set Node Name
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();

        // Draw Custom Setting
        bool flag_year          = m_out_flags & DATETIME_YEAR;
        bool flag_month         = m_out_flags & DATETIME_MONTH;
        bool flag_year_day      = m_out_flags & DATETIME_YEAR_DAY;
        bool flag_month_day     = m_out_flags & DATETIME_MONTH_DAY;
        bool flag_week_day      = m_out_flags & DATETIME_WEEK_DAY;
        bool flag_hour          = m_out_flags & DATETIME_HOUR;
        bool flag_min           = m_out_flags & DATETIME_MIN;
        bool flag_sec           = m_out_flags & DATETIME_SEC;
        bool flag_msec          = m_out_flags & DATETIME_MSEC;
        bool flag_usec          = m_out_flags & DATETIME_USEC;
        bool flag_zone          = m_out_flags & DATETIME_ZONE;
        bool flag_count         = m_out_flags & DATETIME_COUNT;
        bool flag_count_float   = m_out_flags & DATETIME_COUNT_FLOAT;
        ImGui::SetCurrentContext(ctx);
        ImGui::TextUnformatted("        Year"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_year", &flag_year);
        ImGui::TextUnformatted("       Month"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_month", &flag_month);
        ImGui::TextUnformatted(" Day of Year"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_year_day", &flag_year_day);
        ImGui::TextUnformatted("Day of Month"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_month_day", &flag_month_day);
        ImGui::TextUnformatted(" Day of Week"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_week_day", &flag_week_day);
        ImGui::TextUnformatted("        Hour"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_hour", &flag_hour);
        ImGui::TextUnformatted("      Minute"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_min", &flag_min);
        ImGui::TextUnformatted("      Second"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_sec", &flag_sec);
        ImGui::TextUnformatted(" MilliSecond"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_msec", &flag_msec);
        ImGui::TextUnformatted(" MicroSecond"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_usec", &flag_usec);
        ImGui::TextUnformatted("        Zone"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_zone", &flag_zone);
        ImGui::TextUnformatted("       Count"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_count", &flag_count);
        ImGui::TextUnformatted(" Count Float"); ImGui::SameLine(0.f, 100.f); changed |= ImGui::ToggleButton("##toggle_count_float", &flag_count_float);
        m_out_flags = 0;
        if (flag_year)      m_out_flags |= DATETIME_YEAR;
        if (flag_month)     m_out_flags |= DATETIME_MONTH;
        if (flag_year_day)  m_out_flags |= DATETIME_YEAR_DAY;
        if (flag_month_day) m_out_flags |= DATETIME_MONTH_DAY;
        if (flag_week_day)  m_out_flags |= DATETIME_WEEK_DAY;
        if (flag_hour)      m_out_flags |= DATETIME_HOUR;
        if (flag_min)       m_out_flags |= DATETIME_MIN;
        if (flag_sec)       m_out_flags |= DATETIME_SEC;
        if (flag_msec)      m_out_flags |= DATETIME_MSEC;
        if (flag_usec)      m_out_flags |= DATETIME_USEC;
        if (flag_zone)      m_out_flags |= DATETIME_ZONE;
        if (flag_count)     m_out_flags |= DATETIME_COUNT;
        if (flag_count_float)m_out_flags |= DATETIME_COUNT_FLOAT;
        BuildOutputPin();
        return changed;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if (!value.contains("out_flags"))
            return BP_ERR_NODE_LOAD;

        auto& flags = value["out_flags"];
        if (!flags.is_number())
            return BP_ERR_NODE_LOAD;

        m_out_flags = flags.get<imgui_json::number>();

        BuildOutputPin();

        // dynamic pin node load mast after all pin is created
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;

        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) override
    {
        Node::Save(value, MapID);
        value["out_flags"] = imgui_json::number(m_out_flags);
    }
    
    void BuildOutputPin()
    {
        m_OutputPins.clear();
        m_OutputPins.push_back(&m_Exit);
        m_OutputPins.push_back(&m_TimeStamp);
        if (m_out_flags & DATETIME_YEAR)        { m_OutputPins.push_back(&m_Year); }
        if (m_out_flags & DATETIME_MONTH)       { m_OutputPins.push_back(&m_Month); }
        if (m_out_flags & DATETIME_YEAR_DAY)    { m_OutputPins.push_back(&m_DayYear); }
        if (m_out_flags & DATETIME_MONTH_DAY)   { m_OutputPins.push_back(&m_DayMonth); }
        if (m_out_flags & DATETIME_WEEK_DAY)    { m_OutputPins.push_back(&m_DayWeek); }
        if (m_out_flags & DATETIME_HOUR)        { m_OutputPins.push_back(&m_Hour); }
        if (m_out_flags & DATETIME_MIN)         { m_OutputPins.push_back(&m_Min); }
        if (m_out_flags & DATETIME_SEC)         { m_OutputPins.push_back(&m_Sec); }
        if (m_out_flags & DATETIME_MSEC)        { m_OutputPins.push_back(&m_mSec); }
        if (m_out_flags & DATETIME_USEC)        { m_OutputPins.push_back(&m_uSec); }
        if (m_out_flags & DATETIME_ZONE)        { m_OutputPins.push_back(&m_Zone); }
        if (m_out_flags & DATETIME_COUNT)       { m_OutputPins.push_back(&m_count); }
        if (m_out_flags & DATETIME_COUNT_FLOAT) { m_OutputPins.push_back(&m_count_float); }
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override 
    { 
        if (m_OutputPins.size() == 0) 
            BuildOutputPin();
        return m_OutputPins; 
    }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_TimeStamp}; }


    FlowPin m_Enter = { this, "Enter"};
    FlowPin m_Exit = { this, "Exit" };
    Int64Pin m_TimeStamp = { this, "Time Stamp" };
    Int32Pin m_Year = { this, "Year"};
    Int32Pin m_Month = { this, "Month"};
    Int32Pin m_DayYear = { this, "Day of Year"};
    Int32Pin m_DayMonth = { this, "Day of Month"};
    Int32Pin m_DayWeek = { this, "Day of Week"};
    Int32Pin m_Hour = { this, "Hour"};
    Int32Pin m_Min = { this, "Minute"};
    Int32Pin m_Sec = { this, "Second"};
    Int32Pin m_mSec = { this, "Millisecone"};
    Int32Pin m_uSec = { this, "Microsecond"};
    StringPin m_Zone = { this, "Time Zone", ""};
    Int32Pin m_count = { this, "Count"};
    FloatPin m_count_float = { this, "Count Float"};
    Pin* m_InputPins[1] = { &m_Enter };
    std::vector<Pin *> m_OutputPins;

    int32_t m_out_flags = 0;
    int64_t m_start_time = 0;
};
} // namespace BluePrint
