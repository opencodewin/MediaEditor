#pragma once
#include <imgui.h>
namespace BluePrint
{
struct ToStringNode final : Node
{
#define FORMAT_TYPE_NONE        0
#define FORMAT_TYPE_HEX         1
#define FORMAT_TYPE_UNSIGNED    2
    BP_NODE(ToStringNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Default, "Flow")

    ToStringNode(BP* blueprint): Node(blueprint)
    {
        SetType(PinType::Any);
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto value = context.GetPinValue(m_Value);

        string result;
        switch (value.GetType())
        {
            case PinType::Void:   break;
            case PinType::Any:    break;
            case PinType::Flow:   break;
            case PinType::Bool:   result = value.As<bool>() ? "true" : "false"; break;
            case PinType::Int32:
                if (m_format_type == FORMAT_TYPE_HEX)
                {
                    char buffer[128] = {0};
                    snprintf(buffer, 128, "0x%08X", value.As<int32_t>());
                    result = string(buffer);
                }
                else
                {
                    char buffer[128] = {0};
                    string format;
                    if (m_zero_count > 0)
                        format = "%0" + std::to_string(m_zero_count) + (m_format_type == FORMAT_TYPE_UNSIGNED ? "u" : "d");
                    else
                        format = m_format_type == FORMAT_TYPE_UNSIGNED ? "%u" : "%d";
                    snprintf(buffer, 128, format.c_str(), value.As<int32_t>());
                    result = string(buffer);
                }
            break;
            case PinType::Int64:
                if (m_format_type == FORMAT_TYPE_HEX)
                {
                    char buffer[128] = {0};
                    snprintf(buffer, 128, "0x%016" PRIX64, value.As<int64_t>());
                    result = string(buffer);
                }
                else
                {
                    char buffer[128] = {0};
                    string format;
                    if (m_zero_count > 0)
                        format = "%0" + std::to_string(m_zero_count) + (m_format_type == FORMAT_TYPE_UNSIGNED ? "lu" : "ld");
                    else
                        format = m_format_type == FORMAT_TYPE_UNSIGNED ? "%lu" : "%ld";
                    snprintf(buffer, 128, format.c_str(), value.As<int64_t>());
                    result = string(buffer);
                }
            break;
            case PinType::Float:
            {
                char buffer[128] = {0};
                string format;
                if (m_floating_decimal > 0)
                    format = "%." + std::to_string(m_floating_decimal) + "f";
                else
                    format = "%f";
                snprintf(buffer, 128, format.c_str(), value.As<float>());
                result = string(buffer);
            }
            break;
            case PinType::Double:
            {
                char buffer[128] = {0};
                string format;
                if (m_floating_decimal > 0)
                    format = "%." + std::to_string(m_floating_decimal) + "f";
                else
                    format = "%f";
                snprintf(buffer, 128, format.c_str(), value.As<double>());
                result = string(buffer);
            }
            break;
            case PinType::String: result = value.As<string>(); break;
            case PinType::Point:  break;
            default:              break;
        }

        context.SetPinValue(m_String, std::move(result));

        return m_Exit;
    }

    std::string GetName() const override
    {
        return m_Name;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        auto type = m_Value.GetValueType();
        bool changed = false;
        switch (type)
        {
            case PinType::Void:
            case PinType::Any:
            case PinType::Flow:
            case PinType::Bool:
            case PinType::String:
            case PinType::Point:
                ImGui::TextUnformatted("Pin hasn't format setting.");
            break;
            case PinType::Int32:
            case PinType::Int64:
                changed |= ImGui::RadioButton("Decimal", &m_format_type, FORMAT_TYPE_NONE); ImGui::SameLine();
                changed |= ImGui::RadioButton("Hex", &m_format_type, FORMAT_TYPE_HEX); ImGui::SameLine();
                changed |= ImGui::RadioButton("Unsigned", &m_format_type, FORMAT_TYPE_UNSIGNED);
                if (m_format_type != FORMAT_TYPE_HEX) changed |= ImGui::InputInt("Max Zero Prefix", &m_zero_count);
            break;
            case PinType::Float:
            case PinType::Double:
                changed |= ImGui::InputInt("Floating decimal", &m_floating_decimal);
            break;
            default:              break;
        }
        return changed;
    }

    void SetType(PinType type)
    {
        if (m_Name.empty())
        {
            if (type != PinType::Any)
                m_Name = string(PinTypeToString(type)) + " To String";
            else
                m_Name = "To String";
        }
        else if (m_Name.compare("To String") == 0)
        {
            if (type != PinType::Any)
                m_Name = string(PinTypeToString(type)) + " "  + m_Name;
        }
        
        m_Value.SetValueType(type);
    }

    void WasLinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_Value.m_ID)
            SetType(provider.GetValueType());
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_Value.m_ID)
            SetType(PinType::Any);
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;

        if (!value.contains("datatype"))
            return BP_ERR_NODE_LOAD;

        auto& typeValue = value["datatype"];
        if (!typeValue.is_string())
            return BP_ERR_NODE_LOAD;

        PinType type;
        if (!PinTypeFromString(typeValue.get<imgui_json::string>().c_str(), type))
            return BP_ERR_NODE_LOAD;

        if (value.contains("formattype"))
        {
            auto& formatType = value["formattype"];
            m_format_type = formatType.get<imgui_json::number>();
        }

        if (value.contains("zerocount"))
        {
            auto& zeros = value["zerocount"];
            m_zero_count = zeros.get<imgui_json::number>();
        }

        if (value.contains("floatdecimal"))
        {
            auto& floating = value["floatdecimal"];
            m_floating_decimal = floating.get<imgui_json::number>();
        }

        SetType(type);

        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) override
    {
        Node::Save(value, MapID);
        value["datatype"]       = PinTypeToString(m_Value.GetValueType());
        value["formattype"]     = imgui_json::number(m_format_type);
        value["zerocount"]      = imgui_json::number(m_zero_count);
        value["floatdecimal"]   = imgui_json::number(m_floating_decimal);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_Value}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_String}; }

    FlowPin   m_Enter  = { this, "Enter" };
    FlowPin   m_Exit   = { this, "Exit" };
    AnyPin    m_Value  = { this, "Value" };
    StringPin m_String = { this, "String", "" };

    Pin* m_InputPins[2] = { &m_Enter, &m_Value };
    Pin* m_OutputPins[2] = { &m_Exit, &m_String };
    int  m_format_type {FORMAT_TYPE_NONE};
    int  m_zero_count {0};
    int  m_floating_decimal {3};
};
} // namespace BluePrint
