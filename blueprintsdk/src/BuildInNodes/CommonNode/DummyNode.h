#pragma once
#include <imgui.h>
namespace BluePrint
{
struct DummyNode final : Node
{
    BP_NODE(DummyNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Dummy, NodeStyle::Dummy, "Dummy")
    DummyNode(BP* blueprint): Node(blueprint) { m_Name = "Dummy"; }
    ~DummyNode()
    {
        for (auto pin : m_InputPins)  { delete pin; }
        for (auto pin : m_OutputPins) { delete pin; }
    }

    Pin* InsertInputPin(PinType type, const std::string name) override
    {
        Pin* pin = new Pin(this, type, name);
        m_InputPins.push_back(pin);
        return pin;
    }

    Pin* InsertOutputPin(PinType type, const std::string name) override
    {
        Pin* pin = new Pin(this, type, name);
        m_OutputPins.push_back(pin);
        return pin;
    }

    bool InsertInputPins(const imgui_json::value& value)
    {
        const imgui_json::array* inputPinsArray = nullptr;
        if (imgui_json::GetPtrTo(value, "input_pins", inputPinsArray))
        {
            for (auto& pinValue : *inputPinsArray)
            {
                string pinType;
                imgui_json::GetTo<imgui_json::string>(pinValue, "type", pinType);
                string pinName;
                imgui_json::GetTo<imgui_json::string>(pinValue, "name", pinName);
                PinType type;
                PinTypeFromString(pinType, type);
                auto pin = InsertInputPin(type, pinName);
                pin->Load(pinValue);
            }
        }
        return true;
    }

    bool InsertOutputPins(const imgui_json::value& value)
    {
        const imgui_json::array* outputPinsArray = nullptr;
        if (imgui_json::GetPtrTo(value, "output_pins", outputPinsArray))
        {
            for (auto& pinValue : *outputPinsArray)
            {
                string pinType;
                imgui_json::GetTo<imgui_json::string>(pinValue, "type", pinType);
                string pinName;
                imgui_json::GetTo<imgui_json::string>(pinValue, "name", pinName);
                PinType type;
                PinTypeFromString(pinType, type);
                auto pin = InsertOutputPin(type, pinName);
                pin->Load(pinValue);
            }
        }
        return true;
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    int  Load(const imgui_json::value& value) override { m_node_value = value; return BP_ERR_NONE; };
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) override { value = m_node_value; };

    std::vector<Pin *> m_InputPins;
    std::vector<Pin *> m_OutputPins;

    // for save Nodeinfo
    string              m_type_name {"Dummy"};
    string              m_name;
    NodeType            m_type = NodeType::Dummy;
    NodeStyle           m_style = NodeStyle::Dummy;
    string              m_catalog = "Dummy";
    imgui_json::value   m_node_value;
};
} // namespace BluePrint
