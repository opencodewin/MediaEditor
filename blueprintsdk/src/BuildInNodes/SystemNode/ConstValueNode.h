#pragma once
#include <imgui.h>
#include <imgui_extra_widget.h>
namespace BluePrint
{
struct ConstValueNode final : Node
{
    BP_NODE(ConstValueNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Default, "Flow")

    ConstValueNode(BP* blueprint): Node(blueprint) 
    {
        SetType(PinType::Any);
        m_HasCustomLayout = true;
    }

    bool DrawSettingLayout(ImGuiContext * ctx) override
    {
        // We don't set node name for this Node
        // Draw Custom Setting
        bool changed = false;
        changed |= ImGui::RadioButton("Bool", (int *)&m_pintype, (int)PinType::Bool); ImGui::SameLine();
        changed |= ImGui::RadioButton("Int32", (int *)&m_pintype, (int)PinType::Int32); ImGui::SameLine();
        changed |= ImGui::RadioButton("Int64", (int *)&m_pintype, (int)PinType::Int64); ImGui::SameLine();
        changed |= ImGui::RadioButton("Float", (int *)&m_pintype, (int)PinType::Float); ImGui::SameLine();
        changed |= ImGui::RadioButton("Double", (int *)&m_pintype, (int)PinType::Double); ImGui::SameLine();
        changed |= ImGui::RadioButton("String", (int *)&m_pintype, (int)PinType::String);
        SetType(m_pintype);
        ImGui::Separator();
        switch (m_pintype)
        {
            case PinType::Bool:
            {
                ImGui::TextUnformatted("Bool Value:"); ImGui::SameLine(0.f, 50.f);
                if (ImGui::Checkbox("##bool_value", &m_value_bool))
                {
                    m_Value.SetValue(m_value_bool);
                    changed = true;
                }
            }
            break;
            case PinType::Int32: 
            {
                ImGui::TextUnformatted("Int32 Value:"); ImGui::SameLine(0.f, 50.f);
                if (ImGui::InputInt("##int32_value", &m_value_int32))
                {
                    m_Value.SetValue(m_value_int32);
                    changed = true;
                }
            }
            break;
            case PinType::Int64: 
            {
                ImGui::TextUnformatted("Int64 Value:"); ImGui::SameLine(0.f, 50.f);
                if (ImGui::InputInt64("##int64_value", &m_value_int64))
                {
                    m_Value.SetValue(m_value_int64);
                    changed = true;
                }
            }
            break;
            case PinType::Float: 
            {
                ImGui::TextUnformatted("Float Value:"); ImGui::SameLine(0.f, 50.f);
                if (ImGui::InputFloat("##float_value", &m_value_float))
                {
                    m_Value.SetValue(m_value_float);
                    changed = true;
                }
            }
            break;
            case PinType::Double:
            {
                ImGui::TextUnformatted("Double Value:"); ImGui::SameLine(0.f, 50.f);
                if (ImGui::InputDouble("##double_value", &m_value_double))
                {
                    m_Value.SetValue(m_value_double);
                    changed = true;
                }
            }
            break;
            case PinType::String:
            {
                ImGui::TextUnformatted("String Value:"); ImGui::SameLine(0.f, 50.f);
                if (ImGui::InputText("##string_value", (char*)m_value_string.data(), m_value_string.size() + 1, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackResize, [](ImGuiInputTextCallbackData* data) -> int
                {
                    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                    {
                        auto& stringValue = *static_cast<string*>(data->UserData);
                        ImVector<char>* my_str = (ImVector<char>*)data->UserData;
                        IM_ASSERT(stringValue.data() == data->Buf);
                        stringValue.resize(data->BufSize);
                        data->Buf = (char*)stringValue.data();
                    }
                    else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
                    {
                        auto& stringValue = *static_cast<string*>(data->UserData);
                        stringValue = std::string(data->Buf);
                    }
                    return 0;
                }, &m_value_string))
                {
                    m_value_string.resize(strlen(m_value_string.c_str()));
                    m_Value.SetValue(m_value_string);
                    changed = true;
                }
            }
            break;
            default:
            break;
        }
        return changed;
    }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key, bool embedded) override
    {
        ImGui::SetCurrentContext(ctx);
        auto type = m_Value.GetValueType();
        auto value = m_Value.GetValue();
        switch (type)
        {
            case PinType::Bool:
                if (value.As<bool>())
                    ImGui::TextUnformatted("true");
                else
                    ImGui::TextUnformatted("false");
            break;
            case PinType::Int32:
                ImGui::Text("%d", value.As<int32_t>());
            break;
            case PinType::Int64:
                ImGui::Text("%" PRId64, value.As<int64_t>());
            break;
            case PinType::Float:
                ImGui::Text("%g", value.As<float>());
            break;
            case PinType::Double:
                ImGui::Text("%g", value.As<double>());
            break;
            case PinType::String:
                if (value.As<string>().compare(" ") == 0)
                    ImGui::TextUnformatted("space");
                else
                    ImGui::Text("%s", value.As<string>().c_str());
            break;
            default:
            break;
        }
        return false;
    }

    void SetType(PinType type)
    {
        if (type != PinType::Any)
            m_Name = "Const " + string(PinTypeToString(type));
        else
            m_Name = "Const";
        
        m_Value.SetValueType(type);
        m_pintype = type;
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

        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) override
    {
        Node::Save(value, MapID);
        value["datatype"] = PinTypeToString(m_Value.GetValueType());
    }

    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_Value}; }

    AnyPin m_Value = { this };

    Pin* m_OutputPins[1] = { &m_Value };

    PinType m_pintype;
    bool    m_value_bool  {false};
    int32_t m_value_int32 {0};
    int64_t m_value_int64 {0};
    float   m_value_float {0.f};
    double  m_value_double{0.};
    string  m_value_string{""};
};
} // namespace BluePrint
