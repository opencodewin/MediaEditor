#pragma once
#include <imgui.h>

enum CompareType : int32_t
{
    Equal = 0,
    Greater,
    Less,
    GreaterEqual,
    LessEqual,
    NotEqual,
};

namespace BluePrint
{
struct ComparatorNode final : Node
{
    BP_NODE(ComparatorNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Default, "Flow")

    ComparatorNode(BP* blueprint): Node(blueprint) { m_Name = "Comparator"; m_HasCustomLayout = true; }

    int ComparePinValue(PinValue& a, PinValue& b)
    {
        switch (m_Type)
        {
            case PinType::Int32:
                if (a.As<int32_t>() > b.As<int32_t>())
                    return 1;
                else if (a.As<int32_t>() < b.As<int32_t>())
                    return -1;
                else
                    return 0;
            case PinType::Int64:
                if (a.As<int64_t>() > b.As<int64_t>())
                    return 1;
                else if (a.As<int64_t>() < b.As<int64_t>())
                    return -1;
                else
                    return 0;
            case PinType::Float:
                if (a.As<float>() > b.As<float>())
                    return 1;
                else if (a.As<float>() < b.As<float>())
                    return -1;
                else
                    return 0;
            case PinType::Double:
                if (a.As<double>() > b.As<double>())
                    return 1;
                else if (a.As<double>() < b.As<double>())
                    return -1;
                else
                    return 0;
            case PinType::String:
                return a.As<string>().compare(b.As<string>());
            case PinType::Bool:
                if (a.As<bool>() == b.As<bool>())
                    return 0;
                else if (a.As<bool>() == true)
                    return 1;
                else
                    return -1;
            default:
                break;
        }
        return -2;
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto aValue = context.GetPinValue(m_A);
        auto bValue = context.GetPinValue(m_B);
        if (aValue.GetType() != m_Type ||
            bValue.GetType() != m_Type)
        {
            return m_False; // Error: Node values must be of same type
        }
        switch (m_CompareType)
        {
            case Equal:        if (ComparePinValue(aValue, bValue) == 0) return m_True;
            case Greater:      if (ComparePinValue(aValue, bValue) == 1) return m_True;
            case Less:         if (ComparePinValue(aValue, bValue) == -1) return m_True;
            case GreaterEqual: if ((ComparePinValue(aValue, bValue) == 0) || (ComparePinValue(aValue, bValue) == 1)) return m_True;
            case LessEqual:    if ((ComparePinValue(aValue, bValue) == 0) || (ComparePinValue(aValue, bValue) == -1)) return m_True;
            case NotEqual:     if ((ComparePinValue(aValue, bValue) != 0) && (ComparePinValue(aValue, bValue) != -2)) return m_True;
            default: break;
        }
        return m_False;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        ImGui::SetCurrentContext(ctx); // External Node must set context

        // Draw Set Node Name
        auto changed = Node::DrawSettingLayout(ctx);
        ImGui::Separator();

        changed |= ImGui::RadioButton("==", (int *)&m_CompareType, Equal); ImGui::SameLine();
        changed |= ImGui::RadioButton(">", (int *)&m_CompareType, Greater); ImGui::SameLine();
        changed |= ImGui::RadioButton("<", (int *)&m_CompareType, Less); ImGui::SameLine();
        changed |= ImGui::RadioButton(">=", (int *)&m_CompareType, GreaterEqual); ImGui::SameLine();
        changed |= ImGui::RadioButton("<=", (int *)&m_CompareType, LessEqual); ImGui::SameLine();
        changed |= ImGui::RadioButton("!=", (int *)&m_CompareType, NotEqual);
        return changed;
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key, bool embedded) override
    {
        ImGui::SetCurrentContext(ctx);
        std::string title_str;
        switch (m_CompareType)
        {
            case Equal:         title_str = "="; break;
            case Greater:       title_str = ">"; break;
            case Less:          title_str = "<"; break;
            case GreaterEqual:  title_str = ">="; break;
            case LessEqual:     title_str = "<="; break;
            case NotEqual:      title_str = "!="; break;
            default:            title_str = "?"; break;
        }
        auto draw_size = ImGui::CalcTextSize(title_str.data());
        draw_size.x *= 2;
        ImGui::Dummy(draw_size);
        ImGui::TextUnformatted(title_str.data());
        return false;
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

        SetType(type);

        if (value.contains("comparetype"))
        {
            auto& val = value["comparetype"];
            if (!val.is_number())
                return BP_ERR_NODE_LOAD;
            m_CompareType = (CompareType)val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) override
    {
        Node::Save(value, MapID);
        value["datatype"] = PinTypeToString(m_Type);
        value["comparetype"] = imgui_json::number(m_CompareType);
    }

    void SetType(PinType type)
    {
        m_Name = "Comparator";

        m_Type = PinType::Void;
        m_PendingType = type;

        m_A.SetValueType(type);
        m_B.SetValueType(type);

        m_Type = type;
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_A, &m_B}; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_False; }

    PinType m_Type = PinType::Any;

    AnyPin      m_A     = { this, "A" };
    AnyPin      m_B     = { this, "B" };
    FlowPin     m_True  = { this, "True" };
    FlowPin     m_False = { this, "False" };

    Pin* m_InputPins[2] = { &m_A, &m_B };
    Pin* m_OutputPins[2] = { &m_True, &m_False };

private:
    PinType m_PendingType = PinType::Any;
    CompareType m_CompareType = Equal;

};
} // namespace BluePrint