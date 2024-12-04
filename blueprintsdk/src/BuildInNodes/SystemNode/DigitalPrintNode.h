#pragma once
#include <imgui.h>
#if IMGUI_ICONS
#define DIGITAL_0   ICON_FAD_DIGITAL0
#define DIGITAL_1   ICON_FAD_DIGITAL1
#define DIGITAL_2   ICON_FAD_DIGITAL2
#define DIGITAL_3   ICON_FAD_DIGITAL3
#define DIGITAL_4   ICON_FAD_DIGITAL4
#define DIGITAL_5   ICON_FAD_DIGITAL5
#define DIGITAL_6   ICON_FAD_DIGITAL6
#define DIGITAL_7   ICON_FAD_DIGITAL7
#define DIGITAL_8   ICON_FAD_DIGITAL8
#define DIGITAL_9   ICON_FAD_DIGITAL9
#else
#define DIGITAL_0   "0"
#define DIGITAL_1   "1"
#define DIGITAL_2   "2"
#define DIGITAL_3   "3"
#define DIGITAL_4   "4"
#define DIGITAL_5   "5"
#define DIGITAL_6   "6"
#define DIGITAL_7   "7"
#define DIGITAL_8   "8"
#define DIGITAL_9   "9"
#endif

static std::vector<string> DIGITALS = {DIGITAL_0, DIGITAL_1, DIGITAL_2, DIGITAL_3, DIGITAL_4, DIGITAL_5, DIGITAL_6, DIGITAL_7, DIGITAL_8, DIGITAL_9};

namespace BluePrint
{
struct PrintNode final : Node
{
    using PrintFunction = void(*)(const PrintNode& node, std::string message);

    BP_NODE(PrintNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Default, "Debug")

    PrintNode(BP* blueprint): Node(blueprint) { m_Name = "Print"; m_HasCustomLayout = true; }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        m_string = context.GetPinValue<std::string>(m_String);
        if (!m_print_to_layout)
        {
            if (s_PrintFunction)
            {
                s_PrintFunction(*this, m_string);
            }
            LOGD("PrintNode: %s\n", m_string.c_str()); // need disable on run thread mode
        }
        return m_Exit;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;
        
        if (value.contains("layout"))
        {
            auto& val = value["layout"];

            if (!val.is_boolean())
                return BP_ERR_NODE_LOAD;

            m_print_to_layout = val.get<imgui_json::boolean>();
        }
        if (value.contains("tube_digital"))
        {
            auto& tube = value["tube_digital"];

            if (!tube.is_boolean())
                return BP_ERR_NODE_LOAD;

            m_tube_digital = tube.get<imgui_json::boolean>();
        }
        if (value.contains("text_color"))
        {
            auto& color = value["text_color"];
            if (!color.is_number())
                return BP_ERR_NODE_LOAD;
            ImU32 c = color.get<imgui_json::number>();
            m_text_color = ImGui::ColorConvertU32ToFloat4(c);
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["layout"] = m_print_to_layout;
        value["tube_digital"] = m_tube_digital;
        value["text_color"] = imgui_json::number(ImGui::GetColorU32(m_text_color));
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        ImGui::SetCurrentContext(ctx); // External Node must set context

        // Draw Set Node Name
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();

        // Custom Node Setting
        ImGui::TextUnformatted("Print To Layout"); ImGui::SameLine();
        ImGui::ToggleButton("##toggle_print_layout", &m_print_to_layout);

        ImGui::Separator();
        ImGui::TextUnformatted("Print tube digital"); ImGui::SameLine();
        ImGui::ToggleButton("##toggle_tube_digital", &m_tube_digital);

        ImGuiColorEditFlags misc_flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_AlphaBar;
        ImGui::Separator();
        ImGui::Text("Text Color:");
        ImGui::SameLine();
        ImGui::ColorEdit4("##TextColor##PrintNode", (float*)&m_text_color, misc_flags);
        return changed;
    }

    static string ReplaceDigital(const string str)
    {
        if (str.empty())
            return str;
        string result = "";
        for (auto c : str)
        {
            if (c >= 0x30 && c <= 0x39)
                result += DIGITALS[c - 0x30];
            else
                result += c;
        }
        return result;
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key, bool embedded) override
    {
        ImGui::SetCurrentContext(ctx);

        string show_text = m_string;
        if (m_tube_digital)
        {
            show_text = ReplaceDigital(m_string);
        }
        auto cursorPos = ImGui::GetCursorScreenPos();
        auto drawList  = ImGui::GetWindowDrawList();
        const char* text_end = show_text.data() + show_text.size();
        auto color = ImGui::GetColorU32(m_text_color);
        auto draw_size = ImGui::CalcTextSize(show_text.data()) * 2;
        ImGui::Dummy(draw_size);
        drawList->AddText(ctx->Font, ctx->FontSize * 2, cursorPos, color, show_text.data(), text_end);
        return false;
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_String}; }

    FlowPin   m_Enter  = { this, "Enter" };
    FlowPin   m_Exit   = { this, "Exit" };
    StringPin m_String = { this, "String", "" };

    Pin* m_InputPins[2] = { &m_Enter, &m_String };
    Pin* m_OutputPins[1] = { &m_Exit };

    ImVec4 m_text_color             {ImVec4(0,0,0,128)};
    PrintFunction s_PrintFunction   {nullptr};
    bool    m_print_to_layout       {false};
    bool    m_tube_digital          {true};
    std::string  m_string;
};
} // namespace BluePrint
