#pragma once
#include <imgui.h>
namespace BluePrint
{
struct TransitionEntryPointNode final : Node
{
    BP_NODE(TransitionEntryPointNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::EntryPoint, NodeStyle::Simple, "System")

    TransitionEntryPointNode(BP* blueprint): Node(blueprint) 
    {
        m_Name = "Start"; 
        m_OutputPins.push_back(&m_Exit);
        m_OutputPins.push_back(&m_MatOutFirst);
        m_OutputPins.push_back(&m_MatOutSecond);
        m_OutputPins.push_back(&m_TransitionPos);
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        return m_Exit;
    }

    Pin* InsertOutputPin(PinType type, const std::string name) override
    {
        Pin* pin = new Pin(this, type, name);
        pin->m_Flags |= PIN_FLAG_FORCESHOW;
        m_OutputPins.push_back(pin);
        return pin;
    }

    void DeleteOutputPin(const std::string name) override
    {
        auto iter = std::find_if(m_OutputPins.begin(), m_OutputPins.end(), [name](const Pin * pin)
        {
            return pin->m_Name == name;
        });
        if (iter != m_OutputPins.end())
        {
            (*iter)->Unlink();
            if ((*iter)->m_LinkFrom.size() > 0)
            {
                for (auto from_pin : (*iter)->m_LinkFrom)
                {
                    auto link = m_Blueprint->GetPinFromID(from_pin);
                    if (link)
                    {
                        link->Unlink();
                    }
                }
            }
            m_OutputPins.erase(iter);
        }
    }

    int LoadPins(const imgui_json::array* PinValueArray, std::vector<Pin *>& pinArray)
    {
        for (auto& pinValue : *PinValueArray)
        {
            string pinType;
            PinType type = PinType::Any;
            if (!imgui_json::GetTo<imgui_json::string>(pinValue, "type", pinType)) // check pin type
                continue;
            PinTypeFromString(pinType, type);

            std::string name;
            if (pinValue.contains("name"))
                imgui_json::GetTo<imgui_json::string>(pinValue, "name", name);

            auto iter = std::find_if(m_OutputPins.begin(), m_OutputPins.end(), [name](const Pin * pin)
            {
                return pin->m_Name == name;
            });
            if (iter != m_OutputPins.end())
            {
                if (!(*iter)->Load(pinValue))
                {
                    return BP_ERR_GENERAL;
                }
            }
            else
            {
                Pin* pin = nullptr;
                if (type == PinType::Custom)
                {
                    CustomPin * new_pin = new CustomPin(this, "", "");
                    if (!new_pin->Load(pinValue))
                    {
                        delete new_pin;
                        return BP_ERR_GENERAL;
                    }
                    pin = new_pin;
                }
                else if (type == PinType::Any)
                {
                    AnyPin * new_pin = new AnyPin(this);
                    if (!new_pin->Load(pinValue))
                    {
                        delete new_pin;
                        return BP_ERR_GENERAL;
                    }
                    pin = new_pin;
                }
                else
                {
                    pin = new Pin(this, type, "");
                    if (!pin->Load(pinValue))
                    {
                        delete pin;
                        return BP_ERR_GENERAL;
                    }
                }
                if (pin) pinArray.push_back(pin);
            }
        }
        return BP_ERR_NONE;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if (!value.is_object())
            return BP_ERR_NODE_LOAD;

        if (!imgui_json::GetTo<imgui_json::number>(value, "id", m_ID)) // required
            return BP_ERR_NODE_LOAD;

        if (!imgui_json::GetTo<imgui_json::string>(value, "name", m_Name)) // required
            return BP_ERR_NODE_LOAD;

        imgui_json::GetTo<imgui_json::boolean>(value, "break_point", m_BreakPoint); // optional

        imgui_json::GetTo<imgui_json::number>(value, "group_id", m_GroupID); // optional

        const imgui_json::array* outputPinsArray = nullptr;
        if (imgui_json::GetPtrTo(value, "output_pins", outputPinsArray)) // optional
        {
            if (LoadPins(outputPinsArray, m_OutputPins) != BP_ERR_NONE)
                return BP_ERR_INPIN_LOAD;
        }

        return ret;
    }

    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOutFirst, &m_MatOutSecond, &m_TransitionPos}; }
    FlowPin* GetOutputFlowPin() override { return &m_Exit; }

    FlowPin m_Exit = { this, "Start" };
    MatPin  m_MatOutFirst = { this, "Out First" };
    MatPin  m_MatOutSecond = { this, "Out Second" };
    FloatPin m_TransitionPos = { this, "Pos" };

    std::vector<Pin *> m_OutputPins;
};
} // namespace BluePrint

