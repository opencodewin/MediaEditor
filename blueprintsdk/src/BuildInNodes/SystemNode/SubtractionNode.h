#pragma once
#include <imgui.h>
#if IMGUI_ICONS
#define ICON_SUB_SYMBOL "\u2296"
#else
#define ICON_SUB_SYMBOL "-"
#endif
namespace BluePrint
{
struct SubNode final : Node
{
    BP_NODE(SubNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Simple, "Arithmetic")

    SubNode(BP* blueprint): Node(blueprint)
    {
        SetType(PinType::Any);
    }

    PinValue EvaluatePin(const Context& context, const Pin& pin, bool threading = false) const override
    {
        if (pin.m_ID == m_Result.m_ID)
        {
            auto aValue = context.GetPinValue(m_A);
            auto bValue = context.GetPinValue(m_B);

            if (aValue.GetType() != m_Type ||
                bValue.GetType() != m_Type)
            {
                return {}; // Error: Node values must be of same type
            }

            switch (m_Type)
            {
                case PinType::Int32:
                    return aValue.As<int32_t>() - bValue.As<int32_t>();
                case PinType::Int64:
                    return aValue.As<int64_t>() - bValue.As<int64_t>();
                case PinType::Float:
                    return aValue.As<float>() - bValue.As<float>();
                case PinType::Double:
                    return aValue.As<double>() - bValue.As<double>();
                default:
                    break;
            }

            return {}; // Error: Unsupported type
        }
        else
            return Node::EvaluatePin(context, pin);
    }

    std::string GetName() const override
    {
        return m_Name;
    }

    LinkQueryResult AcceptLink(const Pin& receiver, const Pin& provider) override
    {
        auto result = Node::AcceptLink(receiver, provider);
        if (!result)
            return result;

        if (m_Type == PinType::Void)
        {
            if (receiver.m_Node == this && provider.GetValueType() != m_PendingType && provider.GetType() != PinType::Any)
                return { false, "Provider must match type of the node" };

            if (provider.m_Node == this && receiver.GetValueType() != m_PendingType && receiver.GetType() != PinType::Any)
                return { false, "Receiver must match type of the node" };
        }
        {
            auto candidateType = PinType::Void;
            if (receiver.m_Node != this)
                candidateType = receiver.GetType() != PinType::Any ? receiver.GetValueType() : PinType::Any;
            else if (provider.m_Node != this)
                candidateType = provider.GetType() != PinType::Any ? provider.GetValueType() : PinType::Any;

            switch (candidateType)
            {
                case PinType::Any:
                case PinType::Int32:
                case PinType::Int64:
                case PinType::Float:
                case PinType::Double:
                    return { true, "Other pins will convert to this pin type" };

                default:
                    return { false, "Node do not support pin of this type" };
            }
        }

        return {true};
    }

    void WasLinked(const Pin& receiver, const Pin& provider) override
    {
        if (m_Type == PinType::Void)
            return;

        if (receiver.m_ID == m_A.m_ID || receiver.m_ID == m_B.m_ID)
            SetType(provider.GetValueType());
        else if (provider.m_ID == m_Result.m_ID)
            SetType(receiver.GetValueType());
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
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
        value["datatype"] = PinTypeToString(m_Type);
    }

    void SetType(PinType type)
    {
        m_Name = ICON_SUB_SYMBOL;

        m_Type = PinType::Void;
        m_PendingType = type;

        m_A.SetValueType(type);
        m_B.SetValueType(type);
        m_Result.SetValueType(type);

        m_Type = type;
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_A, &m_B}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_Result}; }

    PinType m_Type = PinType::Any;

    AnyPin m_A = { this, "A" };
    AnyPin m_B = { this, "B" };
    AnyPin m_Result = { this, "Result" };

    Pin* m_InputPins[2] = { &m_A, &m_B };
    Pin* m_OutputPins[1] = { &m_Result };

private:
    PinType m_PendingType = PinType::Any;
};
} // namespace BluePrint
