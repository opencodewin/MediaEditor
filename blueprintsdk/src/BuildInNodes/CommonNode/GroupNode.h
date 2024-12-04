#pragma once
#include <imgui.h>
#include <Utils.h>
#include <imgui_node_editor_internal.h>
namespace edd = ax::NodeEditor::Detail;

#define EXPORT_PIN_NAME(pin_name, node_name, node_type) \
        pin_name + "$" + node_name + "$" + node_type
namespace BluePrint
{
struct GroupNode final : Node
{
    BP_NODE(GroupNode, VERSION_BLUEPRINT, VERSION_BLUEPRINT_API, NodeType::Internal, NodeStyle::Group, "System")
    GroupNode(BP* blueprint): Node(blueprint) { m_Name = "Group"; }
    ~GroupNode()
    {
        /*
        // we don't clean GroupNode here, if we remove Group node, clear will be done at OnNodeDeleted
        m_mutex.lock();
        for (auto node : m_GroupNodes) { node->m_GroupID = 0; ed::SetNodeGroupID(m_ID, ed::NodeId::Invalid); }
        for (auto pin : m_InputBridgePins)  { delete pin; }
        for (auto pin : m_InputShadowPins)  { delete pin; }
        for (auto pin : m_OutputBridgePins) { delete pin; }
        for (auto pin : m_OutputShadowPins) { delete pin; }
        m_mutex.unlock();
        */
    }

    inline bool PinIsBridgeIn(const Pin& pin)
    {
        return (pin.m_Flags & PIN_FLAG_BRIDGE) && (pin.m_Flags & PIN_FLAG_IN);
    }

    inline bool PinIsShadowIn(const Pin& pin)
    {
        return (pin.m_Flags & PIN_FLAG_SHADOW) && (pin.m_Flags & PIN_FLAG_IN);
    }

    inline bool PinIsBridgeOut(const Pin& pin)
    {
        return (pin.m_Flags & PIN_FLAG_BRIDGE) && (pin.m_Flags & PIN_FLAG_OUT);
    }

    inline bool PinIsShadowOut(const Pin& pin)
    {
        return (pin.m_Flags & PIN_FLAG_SHADOW) && (pin.m_Flags & PIN_FLAG_OUT);
    }

    inline bool InputLinkIsInside(const Pin& pin)
    {
        if (pin.m_Type == PinType::Flow)
        {
            for (auto from_pin : pin.m_LinkFrom)
            {
                auto link = m_Blueprint->GetPinFromID(from_pin);
                if (link && PinIsBridgeOut(*link) && link->m_Node == this)
                {
                    return true;
                }
            }
        }
        else
        {
            auto link = pin.GetLink(m_Blueprint);
            if (link && PinIsBridgeOut(*link) && link->m_Node == this)
            {
                return true;
            }
        }
        return false;
    }

    inline void AddInputMapPin(Pin * pin)
    {
        if (std::find(m_InputMapPins.begin(), m_InputMapPins.end(), pin) == m_InputMapPins.end())
        {
            m_InputMapPins.push_back(pin);
        }
    }

    inline void AddOutputMapPin(Pin * pin)
    {
        if (std::find(m_OutputMapPins.begin(), m_OutputMapPins.end(), pin) == m_OutputMapPins.end())
        {
            m_OutputMapPins.push_back(pin);
        }
    }

    inline bool AddInputPin(Pin * pin, Pin **bridge_pin, Pin **shadow_pin)
    {
        bool is_exist = false;
        string pin_name = EXPORT_PIN_NAME(pin->m_Name, pin->m_Node->m_Name, pin->m_Node->GetTypeInfo().m_NodeTypeName);
        std::vector<Pin *>::iterator it;
        ID_TYPE pid = pin->m_ID;
        // Try to Create Bridge Pin
        std::string bridge_name = pin_name + "$Bridge$In";
        it = std::find_if(m_InputBridgePins.begin(), m_InputBridgePins.end(), [pid](Pin * const pin)
        {
            return pin->m_MappedPin == pid;
        });
        if (it == m_InputBridgePins.end())
        {
            if (pin->m_Type == PinType::Custom)
            {
                CustomPin* cuspin = reinterpret_cast<CustomPin*>(pin);
                *bridge_pin = new CustomPin(this, cuspin->GetPinEx().GetTypeEx().GetName(), bridge_name);
            }
            else if (pin->m_Type == PinType::Any)
            {
                AnyPin * any_pin = reinterpret_cast<AnyPin *>(pin);
                AnyPin * b_pin = new AnyPin(this, bridge_name);
                if (any_pin->m_InnerPin) b_pin->SetValueType(any_pin->GetValueType());
                *bridge_pin = b_pin;
            }
            else
            {
                *bridge_pin = new Pin(this, pin->m_Type, bridge_name);
            }
            (*bridge_pin)->m_MappedPin = pin->m_ID;
            (*bridge_pin)->m_Flags = PIN_FLAG_BRIDGE | PIN_FLAG_IN;
            m_InputBridgePins.push_back(*bridge_pin);
        }
        else
        {
            *bridge_pin = *it;
            is_exist = true;
        }
        // Try to Create Shadow Pin
        std::string shadow_name = pin_name + "$Shadow$In";
        it = std::find_if(m_InputShadowPins.begin(), m_InputShadowPins.end(), [pid](Pin * const pin)
        {
            return pin->m_MappedPin == pid;
        });
        if (it == m_InputShadowPins.end())
        {
            if (pin->m_Type == PinType::Custom)
            {
                CustomPin* cuspin = reinterpret_cast<CustomPin*>(pin);
                *shadow_pin = new CustomPin(this, cuspin->GetPinEx().GetTypeEx().GetName(), shadow_name);
            }
            else if (pin->m_Type == PinType::Any)
            {
                AnyPin * any_pin = reinterpret_cast<AnyPin *>(pin);
                AnyPin * s_pin = new AnyPin(this, shadow_name);
                if (any_pin->m_InnerPin) s_pin->SetValueType(any_pin->GetValueType());
                *shadow_pin = s_pin;
            }
            else
            {
                *shadow_pin = new Pin(this, pin->m_Type, shadow_name);;
            }
            (*shadow_pin)->m_MappedPin = pin->m_ID;
            (*shadow_pin)->m_Flags = PIN_FLAG_SHADOW | PIN_FLAG_IN;
            m_InputShadowPins.push_back(*shadow_pin);
        }
        else
        {
            *shadow_pin = *it;
            is_exist = true;
        }

        std::sort(m_InputBridgePins.begin(), m_InputBridgePins.end(), [](const Pin * a, const Pin * b) { return a->m_MappedPin < b->m_MappedPin; });

        return is_exist;
    }

    inline void RemoveInputPin(Pin * pin, bool rebuild_link = true)
    {
        std::vector<Pin *>::iterator it;
        Pin * bridge_pin = nullptr;
        Pin * shadow_pin = nullptr;
        ID_TYPE pid = pin->m_ID;
        // Try to Remove Bridge Pin
        it = std::find_if(m_InputBridgePins.begin(), m_InputBridgePins.end(), [pid](Pin * const pin)
        {
            return pin->m_MappedPin == pid;
        });
        if (it != m_InputBridgePins.end())
        {
            bridge_pin = *it;
            m_InputBridgePins.erase(it);
        }
        // Try to Remove Shadow Pin
        it = std::find_if(m_InputShadowPins.begin(), m_InputShadowPins.end(), [pid](Pin * const pin)
        {
            return pin->m_MappedPin == pid;
        });
        if (it != m_InputShadowPins.end())
        {
            shadow_pin = *it;
            m_InputShadowPins.erase(it);
        }
        if (!bridge_pin || !shadow_pin)
            return;
        
        if (rebuild_link)
        {
            // Try to rebuild input link
            if (pin->m_Type == PinType::Flow)
            {
                auto link_from = bridge_pin->m_LinkFrom; // Unlink will change m_LinkFrom, so we clone it
                for (auto from_pin : link_from)
                {
                    auto link = m_Blueprint->GetPinFromID(from_pin);
                    if (link)
                    {
                        link->Unlink();
                        link->LinkTo(*pin);
                    }
                }
                shadow_pin->Unlink();
            }
            else
            {
                pin->Unlink();
                auto link = bridge_pin->GetLink(m_Blueprint);
                if (link)
                {
                    pin->LinkTo(*link);
                }
                bridge_pin->Unlink();
            }
        }
        delete shadow_pin;
        delete bridge_pin;
        std::sort(m_InputBridgePins.begin(), m_InputBridgePins.end(), [](const Pin * a, const Pin * b) { return a->m_MappedPin < b->m_MappedPin; });
    }

    inline bool AddOutputPin(Pin * pin, Pin **bridge_pin, Pin **shadow_pin)
    {
        bool is_exist = false;
        string pin_name = EXPORT_PIN_NAME(pin->m_Name, pin->m_Node->m_Name, pin->m_Node->GetTypeInfo().m_NodeTypeName);
        std::vector<Pin *>::iterator it;
        ID_TYPE pid = pin->m_ID;
        // Try to Create Bridge Pin
        std::string bridge_name = pin_name + "$Bridge$Out";
        it = std::find_if(m_OutputBridgePins.begin(), m_OutputBridgePins.end(), [pid](Pin * const pin)
        {
            return pin->m_MappedPin == pid;
        });
        if (it == m_OutputBridgePins.end())
        {
            if (pin->m_Type == PinType::Custom)
            {
                CustomPin* cuspin = reinterpret_cast<CustomPin*>(pin);
                *bridge_pin = new CustomPin(this, cuspin->GetPinEx().GetTypeEx().GetName(), bridge_name);
            }
            else if (pin->m_Type == PinType::Any)
            {
                AnyPin * any_pin = reinterpret_cast<AnyPin *>(pin);
                AnyPin * b_pin = new AnyPin(this, bridge_name);
                if (any_pin->m_InnerPin) b_pin->SetValueType(any_pin->GetValueType());
                *bridge_pin = b_pin;
            }
            else
            {
                *bridge_pin = new Pin(this, pin->m_Type, bridge_name);;
            }
            (*bridge_pin)->m_MappedPin = pin->m_ID;
            (*bridge_pin)->m_Flags = PIN_FLAG_BRIDGE | PIN_FLAG_OUT;
            m_OutputBridgePins.push_back(*bridge_pin);
        }
        else
        {
            *bridge_pin = *it;
            is_exist = true;
        }
        // Try to Create Shadow Pin
        std::string shadow_name = pin_name + "$Shadow$Out";
        it = std::find_if(m_OutputShadowPins.begin(), m_OutputShadowPins.end(), [pid](Pin * const pin)
        {
            return pin->m_MappedPin == pid;
        });
        if (it == m_OutputShadowPins.end())
        {
            if (pin->m_Type == PinType::Custom)
            {
                CustomPin* cuspin = reinterpret_cast<CustomPin*>(pin);
                *shadow_pin = new CustomPin(this, cuspin->GetPinEx().GetTypeEx().GetName(), shadow_name);
            }
            else if (pin->m_Type == PinType::Any)
            {
                AnyPin * any_pin = reinterpret_cast<AnyPin *>(pin);
                AnyPin * s_pin = new AnyPin(this, shadow_name);
                if (any_pin->m_InnerPin) s_pin->SetValueType(any_pin->GetValueType());
                *shadow_pin = s_pin;
            }
            else
            {
                *shadow_pin = new Pin(this, pin->m_Type, shadow_name);
            }
            (*shadow_pin)->m_MappedPin = pin->m_ID;
            (*shadow_pin)->m_Flags = PIN_FLAG_SHADOW | PIN_FLAG_OUT;
            m_OutputShadowPins.push_back(*shadow_pin);
        }
        else
        {
            *shadow_pin = *it;
            is_exist = true;
        }
        std::sort(m_OutputBridgePins.begin(), m_OutputBridgePins.end(), [](const Pin * a, const Pin * b) { return a->m_MappedPin < b->m_MappedPin; });
        return is_exist;
    }

    inline void RemoveOutputPin(Pin * pin, bool rebuild_link = true)
    {
        std::vector<Pin *>::iterator it;
        ID_TYPE pid = pin->m_ID;
        Pin * bridge_pin = nullptr;
        Pin * shadow_pin = nullptr;
        // Try to Remove Bridge Pin
        it = std::find_if(m_OutputBridgePins.begin(), m_OutputBridgePins.end(), [pid](Pin * const pin)
        {
            return pin->m_MappedPin == pid;
        });
        if (it != m_OutputBridgePins.end())
        {
            bridge_pin = *it;
            m_OutputBridgePins.erase(it);
        }
        // Try to Remove Shadow Pin
        it = std::find_if(m_OutputShadowPins.begin(), m_OutputShadowPins.end(), [pid](Pin * const pin)
        {
            return pin->m_MappedPin == pid;
        });
        if (it != m_OutputShadowPins.end())
        {
            shadow_pin = *it;
            m_OutputShadowPins.erase(it);
        }
        if (!bridge_pin || !shadow_pin)
            return;

        if (rebuild_link)
        {
            // Try to rebuild input link
            if (pin->m_Type == PinType::Flow)
            {
                pin->Unlink();
                auto link = bridge_pin->GetLink(m_Blueprint);
                if (link)
                {
                    bridge_pin->Unlink();
                    pin->LinkTo(*link);
                }
            }
            else
            {
                auto link_from = bridge_pin->m_LinkFrom; // relink will change m_LinkFrom, so we clone it
                for (auto from_pin : link_from)
                {
                    auto link = m_Blueprint->GetPinFromID(from_pin);
                    if (link)
                    {
                        link->LinkTo(*pin);
                    }
                }
                shadow_pin->Unlink();
            }
        }
        delete shadow_pin;
        delete bridge_pin;
        std::sort(m_OutputBridgePins.begin(), m_OutputBridgePins.end(), [](const Pin * a, const Pin * b) { return a->m_MappedPin < b->m_MappedPin; });
    }

    void ScanAllPins()
    {
        m_mutex.lock();
        auto nodes = m_Dragging ? m_GroupNodes : GetGroupedNodes(*this);
        for (auto node : nodes)
        {
            // mark node
            if (!m_Dragging)
            {
                if (std::find(m_GroupNodes.begin(), m_GroupNodes.end(), node) == m_GroupNodes.end())
                {
                    node->m_GroupID = m_ID;
                    ed::SetNodeGroupID(node->m_ID, m_ID);
                    m_GroupNodes.push_back(node);
                }
            }
            // scan all node input pins 
            auto input_pins = node->GetInputPins();
            for (auto pin : input_pins)
            {
                if (pin->m_Type == PinType::Flow)
                {
                    bool export_pin = false;
                    // Check extra link first
                    auto link_from = pin->m_LinkFrom; // clone link vector
                    for (auto from_pin : link_from)
                    {
                        auto link = m_Blueprint->GetPinFromID(from_pin);
                        if (link)
                        {
                            auto linked_node = link->m_Node;
                            if (PinIsBridgeOut(*link) && linked_node == this)
                            {
                                // flow input pin link with inside, pin node is just move in the group
                                // 1. find shadow pin(single link)
                                if (link->m_LinkFrom.size() != 1)
                                    continue;
                                auto shadow_pin = m_Blueprint->GetPinFromID(link->m_LinkFrom[0]);
                                if (!shadow_pin)
                                    continue;
                                // 2. find link from with shadow(single link)
                                if (shadow_pin->m_LinkFrom.size() != 1)
                                    continue;
                                auto in_link = m_Blueprint->GetPinFromID(shadow_pin->m_LinkFrom[0]);
                                if (!in_link)
                                    continue;
                                // 3. unlink inside pin with shadow pin, unlink bridge pin with current pin
                                in_link->Unlink();
                                // 4. link inside pin with current pin
                                in_link->LinkTo(*pin);
                                // 5. delete shadow/bridge pin
                                auto it_b = std::find(m_OutputBridgePins.begin(), m_OutputBridgePins.end(), link);
                                if (it_b != m_OutputBridgePins.end())
                                {
                                    m_OutputBridgePins.erase(it_b);
                                    link->Unlink();
                                    delete link;
                                }
                                auto it_s = std::find(m_OutputShadowPins.begin(), m_OutputShadowPins.end(), shadow_pin);
                                if (it_s != m_OutputShadowPins.end())
                                {
                                    m_OutputShadowPins.erase(it_s);
                                    shadow_pin->Unlink();
                                    delete shadow_pin;
                                }
                            }
                            else if (std::find(nodes.begin(), nodes.end(), linked_node) != nodes.end())
                            {
                                // flow input pin link with inside
                                if (link->m_Link != pin->m_ID)
                                    link->LinkTo(*pin);
                            }
                            else if (PinIsBridgeOut(*link))
                            {
                                // flow input pin link with other group
                                export_pin = true;
                                AddInputMapPin(pin);
                                // 1. create input pin for current group if not exist
                                Pin *bridge_pin = nullptr, *shadow_pin = nullptr;
                                bool already_link = AddInputPin(pin, &bridge_pin, &shadow_pin);
                                if (!bridge_pin || !shadow_pin)
                                    continue;
                                if (link->m_Link != bridge_pin->m_ID)
                                {
                                    // 2. unlink other group bridge out pin link with current pin
                                    link->Unlink();
                                    // 3. link other group bridge out pin with current group bridge in pin
                                    link->LinkTo(*bridge_pin);
                                }
                                if (!already_link)
                                {
                                    // 4. link current group bridge in pin with shadow pin
                                    bridge_pin->LinkTo(*shadow_pin);
                                    // 5. link current group shadow pin with current pin
                                    shadow_pin->LinkTo(*pin);
                                }
                            }
                            else if (PinIsShadowIn(*link) && linked_node == this)
                            {
                                // flow input alread link with export pin
                                // 1. get bridge pin(single link)
                                if (link->m_LinkFrom.size() != 1)
                                    continue;
                                auto bridge_pin = m_Blueprint->GetPinFromID(link->m_LinkFrom[0]);
                                if (!bridge_pin)
                                    continue;
                                if (!(pin->m_Flags & PIN_FLAG_PUBLICIZED) && bridge_pin->m_LinkFrom.size() == 0)
                                {
                                    // 2. if pin isn't public and bridge link from is 0, delete input pin
                                    export_pin = false;
                                    RemoveInputPin(pin);
                                }
                                else
                                {
                                    // 3. if pin is public or bridge has link from, insert input pin
                                    export_pin = true;
                                    AddInputMapPin(pin);
                                }
                            }
                            else
                            {
                                // flow input pin link with outside
                                export_pin = true;
                                AddInputMapPin(pin);
                                // 1. create input pin for current group if not exist
                                Pin *bridge_pin = nullptr, *shadow_pin = nullptr;
                                bool already_link = AddInputPin(pin, &bridge_pin, &shadow_pin);
                                if (!bridge_pin || !shadow_pin)
                                    continue;
                                
                                if (link->m_Link != bridge_pin->m_ID)
                                {
                                    // 2. unlink outside pin with current pin if link isn't exist
                                    link->Unlink();
                                    // 3. link outside pin with current group bridge in pin
                                    link->LinkTo(*bridge_pin);
                                }
                                if (!already_link)
                                {
                                    // 4. link current group bridge in pin with shadow pin
                                    bridge_pin->LinkTo(*shadow_pin);
                                    // 5. link current group shadow pin with current pin
                                    shadow_pin->LinkTo(*pin);
                                }
                            }
                        }
                    }
                    // check public flags
                    if ((pin->m_Flags & PIN_FLAG_PUBLICIZED) && !export_pin)
                    {
                        // flow input pin is publicized but not link
                        export_pin = true;
                        AddInputMapPin(pin);
                        // 1. create input pin for current group if not exist
                        Pin *bridge_pin = nullptr, *shadow_pin = nullptr;
                        bool already_link = AddInputPin(pin, &bridge_pin, &shadow_pin);
                        if (!bridge_pin || !shadow_pin)
                            continue;
                        if (!already_link)
                        {
                            // 2. link current group bridge in pin with shadow pin
                            bridge_pin->LinkTo(*shadow_pin);
                            // 3. link current group shadow pin with current pin
                            shadow_pin->LinkTo(*pin);
                        }
                    }
                    if (export_pin) pin->m_Flags |= PIN_FLAG_EXPORTED;
                    else pin->m_Flags &= ~PIN_FLAG_EXPORTED;
                }
                else
                {
                    // Check extra link first
                    auto link = pin->GetLink(m_Blueprint);
                    if (link)
                    {
                        auto linked_node = link->m_Node;
                        if (PinIsBridgeOut(*link) && linked_node == this)
                        {
                            // data input pin link with inside
                            pin->m_Flags &= ~PIN_FLAG_EXPORTED;
                            // 1. get shadow pin(single link)
                            auto shadow_pin = link->GetLink(m_Blueprint);
                            if (!shadow_pin)
                                continue;
                            // 2. get inside pin with shadow pin(single link)
                            auto in_link = shadow_pin->GetLink(m_Blueprint);
                            if (!in_link)
                                continue;
                            // 3. unlink current pin with bridge out pin
                            pin->Unlink();
                            // 4. link current pin with inside pin
                            pin->LinkTo(*in_link);
                            // 5. if bridge out pin linkfrom size is 0, delete shadow/bridge pin
                            if (link->m_LinkFrom.size() <= 0)
                            {
                                auto it_b = std::find(m_OutputBridgePins.begin(), m_OutputBridgePins.end(), link);
                                if (it_b != m_OutputBridgePins.end())
                                {
                                    m_OutputBridgePins.erase(it_b);
                                    link->Unlink();
                                    delete link;
                                }
                                auto it_s = std::find(m_OutputShadowPins.begin(), m_OutputShadowPins.end(), shadow_pin);
                                if (it_s != m_OutputShadowPins.end())
                                {
                                    m_OutputShadowPins.erase(it_s);
                                    shadow_pin->Unlink();
                                    delete shadow_pin;
                                }
                            }
                        }
                        else if (std::find(nodes.begin(), nodes.end(), linked_node) != nodes.end())
                        {
                            // data input pin link with inside pin
                            pin->m_Flags &= ~PIN_FLAG_EXPORTED;
                            if (pin->m_Link != link->m_ID)
                                pin->LinkTo(*link);
                        }
                        else if (PinIsBridgeOut(*link))
                        {
                            // data input pin link with other group
                            pin->m_Flags |= PIN_FLAG_EXPORTED;
                            AddInputMapPin(pin);
                            // 1. create input pin for current group if not exist
                            Pin *bridge_pin = nullptr, *shadow_pin = nullptr;
                            bool already_link = AddInputPin(pin, &bridge_pin, &shadow_pin);
                            if (!bridge_pin || !shadow_pin)
                                continue;
                            if (bridge_pin->m_Link != link->m_ID)
                            {
                                // 2. unlink current pin with other group bridge out pin link
                                pin->Unlink();
                                // 3. link current group bridge in pin with other group bridge out pin
                                bridge_pin->LinkTo(*link);
                            }
                            if (!already_link)
                            {
                                // 4. link current shadow pin with group bridge in pin
                                shadow_pin->LinkTo(*bridge_pin);
                                // 5. link current pin with current group shadow pin
                                pin->LinkTo(*shadow_pin);
                            }
                        }
                        else if (PinIsShadowIn(*link) && linked_node == this)
                        {
                            // data input alread link with export pin
                            // 1. get bridge pin(single link)
                            if (!link->m_Link)
                                continue;
                            auto bridge_pin = link->GetLink(m_Blueprint);
                            if (!bridge_pin)
                                continue;
                            if (!(pin->m_Flags & PIN_FLAG_PUBLICIZED) && !bridge_pin->m_Link)
                            {
                                // 2. if pin isn't public and bridge link from is 0, delete input pin
                                pin->m_Flags &= ~PIN_FLAG_EXPORTED;
                                RemoveInputPin(pin);
                            }
                            else
                            {
                                // 3. if pin is public or bridge has link from, insert input pin
                                pin->m_Flags |= PIN_FLAG_EXPORTED;
                                AddInputMapPin(pin);
                            }
                        }
                        else
                        {
                            // data input pin link with outside
                            pin->m_Flags |= PIN_FLAG_EXPORTED;
                            AddInputMapPin(pin);
                            // 1. create input pin for current group if not exist
                            Pin *bridge_pin = nullptr, *shadow_pin = nullptr;
                            bool already_link = AddInputPin(pin, &bridge_pin, &shadow_pin);
                            if (!bridge_pin || !shadow_pin)
                                continue;
                            if (pin->m_Link != shadow_pin->m_ID)
                            {
                                // 2. unlink current pin with outside pin
                                pin->Unlink();
                                // 3. link current pin with current group shadow pin
                                pin->LinkTo(*shadow_pin);
                            }
                            if (!already_link)
                            {
                                // 4. link shadow pin with current group bridge in pin
                                shadow_pin->LinkTo(*bridge_pin);
                                // 5. link current group bridge in pin outside pin
                                bridge_pin->LinkTo(*link);
                            }
                        }
                    }
                    // check public flags
                    if ((pin->m_Flags & PIN_FLAG_PUBLICIZED) && !(pin->m_Flags & PIN_FLAG_EXPORTED) && !pin->m_Link)
                    {
                        // data input pin is public without link
                        pin->m_Flags |= PIN_FLAG_EXPORTED;
                        AddInputMapPin(pin);
                        // 1. create input pin for current group if not exist
                        Pin *bridge_pin = nullptr, *shadow_pin = nullptr;
                        bool already_link = AddInputPin(pin, &bridge_pin, &shadow_pin);
                        if (!bridge_pin || !shadow_pin)
                            continue;
                        if (!already_link)
                        {
                            // 2. link shadow pin with current pin
                            pin->LinkTo(*shadow_pin);
                            // 3. link shadow pin with current group bridge in pin
                            shadow_pin->LinkTo(*bridge_pin);
                        }
                    }
                }
            }
            // scan all node output pins 
            auto output_pins = node->GetOutputPins();
            for (auto pin : output_pins)
            {
                if (pin->m_Type == PinType::Flow)
                {
                    // Check extra link first
                    auto link = pin->GetLink(m_Blueprint);
                    if (link)
                    {
                        auto linked_node = link->m_Node;
                        if (PinIsBridgeIn(*link) && linked_node == this)
                        {
                            // flow output pin link with inside pin
                            pin->m_Flags &= ~PIN_FLAG_EXPORTED;
                            // 1. unlink current pin with bridge in pin
                            pin->Unlink();
                            // 2. get shadow pin with bridge in pin
                            auto shadow_pin = link->GetLink(m_Blueprint);
                            if (!shadow_pin)
                                continue;
                            // 3. get inside pin link with shadow pin
                            auto in_link = shadow_pin->GetLink(m_Blueprint);
                            if (!in_link)
                                continue;
                            // 4. link with current pin with inside pin
                            pin->LinkTo(*in_link);
                            // 5. if bridge pin linkfrom is 0, delete bridge/shadow pin
                            if (link->m_LinkFrom.size() == 0)
                            {
                                auto it_b = std::find(m_InputBridgePins.begin(), m_InputBridgePins.end(), link);
                                if (it_b != m_InputBridgePins.end())
                                {
                                    m_InputBridgePins.erase(it_b);
                                    link->Unlink();
                                    delete link;
                                }
                                auto it_s = std::find(m_InputShadowPins.begin(), m_InputShadowPins.end(), shadow_pin);
                                if (it_s != m_InputShadowPins.end())
                                {
                                    m_InputShadowPins.erase(it_s);
                                    shadow_pin->Unlink();
                                    delete shadow_pin;
                                }
                            }
                        }
                        else if (std::find(nodes.begin(), nodes.end(), linked_node) != nodes.end())
                        {
                            // flow output pin link with inside pin
                            pin->m_Flags &= ~PIN_FLAG_EXPORTED;
                            if (pin->m_Link != link->m_ID)
                                pin->LinkTo(*link);
                        }
                        else if (PinIsBridgeIn(*link))
                        {
                            // flow output pin link with other group
                            pin->m_Flags |= PIN_FLAG_EXPORTED;
                            AddOutputMapPin(pin);
                            // 1. unlink current pin with other group Bridge in pin
                            pin->Unlink();
                            // 2. create current group output pin if not exist
                            Pin *bridge_pin = nullptr, *shadow_pin = nullptr;
                            bool already_link = AddOutputPin(pin, &bridge_pin, &shadow_pin);
                            if (!bridge_pin || !shadow_pin)
                                continue;
                            if (pin->m_Link != shadow_pin->m_ID)
                            {
                                // 3. link current pin with current group shadow out pin
                                pin->LinkTo(*shadow_pin);
                            }
                            if (!already_link)
                            {
                                // 4. link current group shadow out pin with bridge out pin
                                shadow_pin->LinkTo(*bridge_pin);
                                // 5. link current group shadow out pin with other group bridge in pin
                                bridge_pin->LinkTo(*link);
                            }
                        }
                        else if (PinIsShadowOut(*link) && linked_node == this)
                        {
                            // flow output alread link with export pin
                            // 1. get bridge pin(single link)
                            if (!link->m_Link)
                                continue;
                            auto bridge_pin = link->GetLink(m_Blueprint);
                            if (!bridge_pin)
                                continue;
                            if (!(pin->m_Flags & PIN_FLAG_PUBLICIZED) && !bridge_pin->m_Link)
                            {
                                // 2. if pin isn't public and bridge link from is 0, delete input pin
                                pin->m_Flags &= ~PIN_FLAG_EXPORTED;
                                RemoveOutputPin(pin);
                            }
                            else
                            {
                                // 3. if pin is public or bridge has link from, insert input pin
                                pin->m_Flags |= PIN_FLAG_EXPORTED;
                                AddOutputMapPin(pin);
                            }
                        }
                        else
                        {
                            // flow output pin link with outside pin
                            pin->m_Flags |= PIN_FLAG_EXPORTED;
                            AddOutputMapPin(pin);
                            // 1. create current group output pin if not exist
                            Pin *bridge_pin = nullptr, *shadow_pin = nullptr;
                            bool already_link = AddOutputPin(pin, &bridge_pin, &shadow_pin);
                            if (!bridge_pin || !shadow_pin)
                                continue;
                            
                            if (pin->m_Link != shadow_pin->m_ID)
                            {
                                // 2. unlink current pin with outside pin if link isn't exist
                                pin->Unlink();
                                // 3. link current pin with current group shadow out pin
                                pin->LinkTo(*shadow_pin);
                            }
                            if (!already_link)
                            {
                                // 4. link current group shadow out pin with bridge out pin
                                shadow_pin->LinkTo(*bridge_pin);
                                // 5. link current group shadow out pin with outside pin
                                bridge_pin->LinkTo(*link);
                            }
                        }
                    }
                    // check public flags
                    if ((pin->m_Flags & PIN_FLAG_PUBLICIZED) && !(pin->m_Flags & PIN_FLAG_EXPORTED) && !pin->m_Link)
                    {
                        // flow pin is public without link
                        pin->m_Flags |= PIN_FLAG_EXPORTED;
                        AddOutputMapPin(pin);
                        // 1. create current group output pin if not exist
                        Pin *bridge_pin = nullptr, *shadow_pin = nullptr;
                        bool already_link = AddOutputPin(pin, &bridge_pin, &shadow_pin);
                        if (!bridge_pin || !shadow_pin)
                            continue;
                        if (!already_link)
                        {
                            // 2. link current pin to shadow pin
                            pin->LinkTo(*shadow_pin);
                            // 3. link current group shadow out pin with bridge out pin
                            shadow_pin->LinkTo(*bridge_pin);
                        }
                    }
                }
                else
                {
                    bool export_pin = false;
                    // Check extra link first
                    auto link_from = pin->m_LinkFrom; // clone link vector
                    for (auto from_pin : link_from)
                    {
                        auto link = m_Blueprint->GetPinFromID(from_pin);
                        if (link)
                        {
                            auto linked_node = link->m_Node;
                            if (PinIsBridgeIn(*link) && linked_node == this)
                            {
                                // data output pin link with inside pin
                                // 1. get shadow pin link with bridge in pin(single link)
                                if (link->m_LinkFrom.size() != 1)
                                    continue;
                                auto shadow_pin = m_Blueprint->GetPinFromID(link->m_LinkFrom[0]);
                                if (!shadow_pin)
                                    continue;
                                // 2. get inside pin linkfrom pin(single link)
                                if (shadow_pin->m_LinkFrom.size() != 1)
                                    continue;
                                auto in_link = m_Blueprint->GetPinFromID(shadow_pin->m_LinkFrom[0]);
                                if (!in_link)
                                    continue;
                                // 3. unlink inside pin linkfrom pin with shadow pin
                                in_link->Unlink();
                                // 4. link inside pin linkfrom pin with current pin
                                in_link->LinkTo(*pin);
                                // 5. if shadow pin linkfrom size is 0, delete bridge/shadow pin
                                auto it_b = std::find(m_InputBridgePins.begin(), m_InputBridgePins.end(), link);
                                if (it_b != m_InputBridgePins.end())
                                {
                                    m_InputBridgePins.erase(it_b);
                                    link->Unlink();
                                    delete link;
                                }
                                auto it_s = std::find(m_InputShadowPins.begin(), m_InputShadowPins.end(), shadow_pin);
                                if (it_s != m_InputShadowPins.end())
                                {
                                    m_InputShadowPins.erase(it_s);
                                    shadow_pin->Unlink();
                                    delete shadow_pin;
                                }
                            }
                            else if (std::find(nodes.begin(), nodes.end(), linked_node) != nodes.end())
                            {
                                // data output pin link with inside pin
                                if (link->m_Link != pin->m_ID)
                                    link->LinkTo(*pin);
                            }
                            else if (PinIsBridgeIn(*link))
                            {
                                // data output pin link with other group
                                export_pin = true;
                                AddOutputMapPin(pin);
                                // 1. unlink other group bridge in pin with current pin(single link)
                                link->Unlink();
                                // 2. create current group out pin if not exist
                                Pin *bridge_pin = nullptr, *shadow_pin = nullptr;
                                bool already_link = AddOutputPin(pin, &bridge_pin, &shadow_pin);
                                if (!bridge_pin || !shadow_pin)
                                    continue;
                                if (link->m_Link != bridge_pin->m_ID)
                                {
                                    // 3. link other group bridge in pin with current group bridge out pin
                                    link->LinkTo(*bridge_pin);
                                }
                                if (!already_link)
                                {
                                    // 4. link current group bridge out pin with current group shadow out pin
                                    bridge_pin->LinkTo(*shadow_pin);
                                    // 5. link current group shadow out pin with current pin
                                    shadow_pin->LinkTo(*pin);
                                }
                            }
                            else if (PinIsShadowOut(*link) && linked_node == this)
                            {
                                // data output alread link with export pin
                                // 1. get bridge pin(single link)
                                if (link->m_LinkFrom.size() != 1)
                                    continue;
                                auto bridge_pin = m_Blueprint->GetPinFromID(link->m_LinkFrom[0]);
                                if (!bridge_pin)
                                    continue;
                                if (!(pin->m_Flags & PIN_FLAG_PUBLICIZED) && bridge_pin->m_LinkFrom.size() == 0)
                                {
                                    // 2. if pin isn't public and bridge link from is 0, delete input pin
                                    export_pin = false;
                                    RemoveOutputPin(pin);
                                }
                                else
                                {
                                    // 3. if pin is public or bridge has link from, insert input pin
                                    export_pin = true;
                                    AddOutputMapPin(pin);
                                }
                            }
                            else
                            {
                                // data output pin link with outside
                                export_pin = true;
                                AddOutputMapPin(pin);
                                // 1. create current group out pin if not exist
                                Pin *bridge_pin = nullptr, *shadow_pin = nullptr;
                                bool already_link = AddOutputPin(pin, &bridge_pin, &shadow_pin);
                                if (!bridge_pin || !shadow_pin)
                                    continue;
                                if (link->m_Link != bridge_pin->m_ID)
                                {
                                    // 2. unlink outside pin with current pin
                                    link->Unlink();
                                    // 3. link outside pin link with current group bridge out pin
                                    link->LinkTo(*bridge_pin);
                                }
                                if (!already_link)
                                {
                                    // 4. link current group bridge out pin with current group shadow out pin if not exist
                                    bridge_pin->LinkTo(*shadow_pin);
                                    // 5. link current group shadow out pin with current pin
                                    shadow_pin->LinkTo(*pin);
                                }
                            }
                        }
                    }
                    // check public flags
                    if ((pin->m_Flags & PIN_FLAG_PUBLICIZED) && !export_pin)
                    {
                        // data output pin is public without link
                        export_pin = true;
                        AddOutputMapPin(pin);
                        // 1. create current group out pin if not exist
                        Pin *bridge_pin = nullptr, *shadow_pin = nullptr;
                        bool already_link = AddOutputPin(pin, &bridge_pin, &shadow_pin);
                        if (!bridge_pin || !shadow_pin)
                            continue;
                        if (!already_link)
                        {
                            // 2. link current group bridge out pin with current group shadow out pin if not exist
                            bridge_pin->LinkTo(*shadow_pin);
                            // 3. link current group shadow out pin with current pin
                            shadow_pin->LinkTo(*pin);
                        }
                    }
                    if (export_pin) pin->m_Flags |= PIN_FLAG_EXPORTED;
                    else pin->m_Flags &= ~PIN_FLAG_EXPORTED;
                }
            }
        }
        // remove ungrouped node
        for (auto iter = m_GroupNodes.begin(); iter != m_GroupNodes.end();)
        {
            if (std::find(nodes.begin(), nodes.end(), *iter) == nodes.end())
            {
                auto node = *iter;
                if (node->m_GroupID == m_ID)
                {
                    node->m_GroupID = 0;
                    ed::SetNodeGroupID(node->m_ID, ed::NodeId::Invalid);
                    ed::SetNodeZPosition(node->m_ID, 0);
                }
                iter = m_GroupNodes.erase(iter);
                for (auto pin : node->GetInputPins())
                {
                    auto it = std::find(m_InputMapPins.begin(), m_InputMapPins.end(), pin);
                    if (it != m_InputMapPins.end())
                    {
                        pin->m_Flags &= ~PIN_FLAG_EXPORTED;
                        m_InputMapPins.erase(it);
                        RemoveInputPin(pin);
                    }
                }
                for (auto pin : node->GetOutputPins())
                {
                    auto it = std::find(m_OutputMapPins.begin(), m_OutputMapPins.end(), pin);
                    if (it != m_OutputMapPins.end())
                    {
                        pin->m_Flags &= ~PIN_FLAG_EXPORTED;
                        m_OutputMapPins.erase(it);
                        RemoveOutputPin(pin);
                    }
                }
            }
            else
            {
                iter ++;
            }
        }

        ed::SetNodeZPosition(m_ID, m_ZPos); 
        // re-order Z position
        for (auto iter = m_GroupNodes.begin(); iter != m_GroupNodes.end();iter ++)
        {
            auto node = *iter;
            ed::SetNodeZPosition(node->m_ID, m_ZPos + 1);
        }
        m_mutex.unlock();
    }

    void OnNodeDelete(Node * node) override
    {
        m_mutex.lock();
        if (node)
        {
            // delete node inside group
            // remove node pin from group
            for (auto pin : node->GetInputPins())
            {
                auto iter = std::find(m_InputMapPins.begin(), m_InputMapPins.end(), pin);
                if (iter != m_InputMapPins.end())
                {
                    Pin * pin = *iter;
                    m_InputMapPins.erase(iter);
                    RemoveInputPin(pin, false);
                }
            }
            for (auto pin : node->GetOutputPins())
            {
                auto iter = std::find(m_OutputMapPins.begin(), m_OutputMapPins.end(), pin);
                if (iter != m_OutputMapPins.end())
                {
                    Pin * pin = *iter;
                    m_OutputMapPins.erase(iter);
                    RemoveOutputPin(pin, false);
                }
            }
            // remove node from m_GroupNodes
            auto iter = std::find(m_GroupNodes.begin(), m_GroupNodes.end(), node);
            if (iter != m_GroupNodes.end())
            {
                m_GroupNodes.erase(iter);
            }
        }
        else
        {
            // delete self
            for (auto pin : m_InputMapPins)
            {
                pin->m_Flags &= ~PIN_FLAG_EXPORTED;
                RemoveInputPin(pin);
            }
            for (auto pin : m_OutputMapPins)
            {
                pin->m_Flags &= ~PIN_FLAG_EXPORTED;
                RemoveOutputPin(pin);
            }
            // mark all inside node as no group
            for (auto node : m_GroupNodes)
            {
                node->m_GroupID = 0;
                ed::SetNodeGroupID(node->m_ID, ed::NodeId::Invalid);
                ed::SetNodeZPosition(node->m_ID, 0);
            }
        }
        m_mutex.unlock();
    }
    
    void Update() override
    {
        ScanAllPins();
    }

    void OnDragStart(const Context& context) override
    {
        m_Dragging = true;
        ScanAllPins();
    }

    void OnDragEnd(const Context& context) override
    {
        ScanAllPins();
        m_Dragging = false;
    }

    void OnResize(const Context& context) override
    {
        ScanAllPins();
    }

    int LoadPins(const imgui_json::array* PinValueArray, std::vector<Pin *>& pinArray)
    {
        pinArray.clear();
        for (auto& pinValue : *PinValueArray)
        {
            string pinType;
            PinType type = PinType::Any;
            if (!imgui_json::GetTo<imgui_json::string>(pinValue, "type", pinType)) // check pin type
                continue;
            PinTypeFromString(pinType, type);

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

        const imgui_json::array* inputPinsArray = nullptr;
        if (imgui_json::GetPtrTo(value, "input_pins", inputPinsArray)) // optional
        {
            if (LoadPins(inputPinsArray, m_InputBridgePins) != BP_ERR_NONE)
                return BP_ERR_INPIN_LOAD;
        }

        const imgui_json::array* inputShadowPinsArray = nullptr;
        if (imgui_json::GetPtrTo(value, "input_shadow_pins", inputShadowPinsArray)) // optional
        {
            if (LoadPins(inputShadowPinsArray, m_InputShadowPins) != BP_ERR_NONE)
                return BP_ERR_INPIN_LOAD;
        }

        const imgui_json::array* outputPinsArray = nullptr;
        if (imgui_json::GetPtrTo(value, "output_pins", outputPinsArray)) // optional
        {
            if (LoadPins(outputPinsArray, m_OutputBridgePins) != BP_ERR_NONE)
                return BP_ERR_INPIN_LOAD;
        }

        const imgui_json::array* outputShadowPinsArray = nullptr;
        if (imgui_json::GetPtrTo(value, "output_shadow_pins", outputShadowPinsArray)) // optional
        {
            if (LoadPins(outputShadowPinsArray, m_OutputShadowPins) != BP_ERR_NONE)
                return BP_ERR_INPIN_LOAD;
        }

        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) override
    {
        Node::Save(value, MapID);
        auto& inputPinsValue = value["input_shadow_pins"]; // optional
        for (auto& pin : m_InputShadowPins)
        {
            imgui_json::value pinValue;
            pin->Save(pinValue, MapID);
            inputPinsValue.push_back(pinValue);
        }
        if (inputPinsValue.is_null())
            value.erase("input_shadow_pins");

        auto& outputPinsValue = value["output_shadow_pins"]; // optional
        for (auto& pin : m_OutputShadowPins)
        {
            imgui_json::value pinValue;
            pin->Save(pinValue, MapID);
            outputPinsValue.push_back(pinValue);
        }
        if (outputPinsValue.is_null())
            value.erase("output_shadow_pins");
    }

    inline void SetPinIDToMap(Pin * pin, std::map<ID_TYPE, ID_TYPE>& MapID, ID_TYPE &index)
    {
        MapID[pin->m_ID] = index++;
        if (pin->GetType() == PinType::Any)
        {
            AnyPin * apin = (AnyPin *)pin;
            if (apin->m_InnerPin)
            {
                MapID[apin->m_InnerPin->m_ID] = index++;
            }
        }
    }

    inline void SetNodeIDToMap(Node * node, std::map<ID_TYPE, ID_TYPE>& MapID, ID_TYPE &index)
    {
        MapID[node->m_ID] = index++;
    }

    void SaveGroup(std::string path_name)
    {
        imgui_json::value result;
        // Create ID Map
        ID_TYPE object_id = 1;
        std::map<ID_TYPE, ID_TYPE> IDMaps;
        SetNodeIDToMap(this, IDMaps, object_id);
        for (auto pin : m_InputBridgePins)
        {
            SetPinIDToMap(pin, IDMaps, object_id);
        }
        for (auto pin : m_OutputBridgePins)
        {
            SetPinIDToMap(pin, IDMaps, object_id);
        }
        for (auto pin : m_InputShadowPins)
        {
            SetPinIDToMap(pin, IDMaps, object_id);
        }
        for (auto pin : m_OutputShadowPins)
        {
            SetPinIDToMap(pin, IDMaps, object_id);
        }
        for (auto node : m_GroupNodes)
        {
            SetNodeIDToMap(node, IDMaps, object_id);
            for (auto pin : node->GetInputPins())
            {
                SetPinIDToMap(pin, IDMaps, object_id);
            }
            for (auto pin : node->GetOutputPins())
            {
                SetPinIDToMap(pin, IDMaps, object_id);
            }
        }

        // save group node
        imgui_json::value group_value;
        group_value["type_id"] = imgui_json::number(GetTypeInfo().m_ID);
        group_value["type_name"] = GetTypeInfo().m_Name;
        Save(group_value, IDMaps);
        result["group"] = group_value;

        // save groupped nodes
        auto& nodesValue = result["nodes"];
        nodesValue = imgui_json::array();
        for (auto node : m_GroupNodes)
        {
            imgui_json::value nodeValue;
            nodeValue["type_id"] = imgui_json::number(node->GetTypeInfo().m_ID);
            nodeValue["type_name"] = node->GetTypeInfo().m_Name;
            node->Save(nodeValue, IDMaps);
            nodesValue.push_back(nodeValue);
        }

        // save group node status and set location to 0,0
        auto& nodesStatus = result["status"];
        auto GroupStatus = ed::GetState(ed::StateType::Node, m_ID);
        ImVec2 group_location;
        edd::Serialization::Parse(GroupStatus["location"], group_location);
        GroupStatus["location"] = edd::Serialization::ToJson(ImVec2(0, 0));
        nodesStatus[edd::Serialization::ToString((const ed::NodeId)(IDMaps.at(m_ID)))] = GroupStatus;

        // save all nodes status and modify location
        for (auto node : m_GroupNodes)
        {
            auto nodeStatus = ed::GetState(ed::StateType::Node, node->m_ID);
            ImVec2 node_location;
            edd::Serialization::Parse(nodeStatus["location"], node_location);
            node_location -= group_location;
            nodeStatus["location"] = edd::Serialization::ToJson(node_location);
            nodesStatus[edd::Serialization::ToString((const ed::NodeId)(IDMaps.at(node->m_ID)))] = nodeStatus;
        }
        result.save(path_name);
    }

    inline void GetPinIDMap(const imgui_json::value& pinValue, std::map<ID_TYPE, ID_TYPE>& IDMaps)
    {
        ID_TYPE object_id;
        imgui_json::GetTo<imgui_json::number>(pinValue, "id", object_id);
        IDMaps[object_id] = m_Blueprint->MakePinID(nullptr);
        if (pinValue.contains("inner"))
        {
            auto innerValue = pinValue["inner"];
            imgui_json::GetTo<imgui_json::number>(innerValue, "id", object_id);
            IDMaps[object_id] = m_Blueprint->MakePinID(nullptr);
        }
    }

    inline void AdjestPinID(Pin * pin, std::map<ID_TYPE, ID_TYPE>& IDMaps)
    {
        pin->m_ID = GetIDFromMap(pin->m_ID, IDMaps);
        if (pin->m_MappedPin) pin->m_MappedPin = GetIDFromMap(pin->m_MappedPin, IDMaps);
        if (pin->m_Link) pin->m_Link = GetIDFromMap(pin->m_Link, IDMaps);
        for (int i = 0; i < pin->m_LinkFrom.size(); i++)
        {
            if (pin->m_LinkFrom[i]) pin->m_LinkFrom[i] = GetIDFromMap(pin->m_LinkFrom[i], IDMaps);
        }
        if (pin->m_Type == PinType::Any)
        {
            AnyPin * apin = (AnyPin *)pin;
            if (apin->m_InnerPin)
            {
                apin->m_InnerPin->m_ID = GetIDFromMap(apin->m_InnerPin->m_ID, IDMaps);
            }
        }
    }

    void LoadGroup(const imgui_json::value& value, ImVec2 pos)
    {
        // rebuild ID Maps
        std::map<ID_TYPE, ID_TYPE> IDMaps;
        auto& groupValue = value["group"];
        auto& statusValue = value["status"];
        ID_TYPE object_id;
        imgui_json::GetTo<imgui_json::number>(groupValue, "id", object_id);
        IDMaps[object_id] = m_ID;
        const imgui_json::array* inputPinsArray = nullptr;
        if (imgui_json::GetPtrTo(groupValue, "input_pins", inputPinsArray))
        {
            for (auto& pinValue : *inputPinsArray)
            {
                GetPinIDMap(pinValue, IDMaps);
            }
        }
        const imgui_json::array* outputPinsArray = nullptr;
        if (imgui_json::GetPtrTo(groupValue, "output_pins", outputPinsArray))
        {
            for (auto& pinValue : *outputPinsArray)
            {
                GetPinIDMap(pinValue, IDMaps);
            }
        }
        const imgui_json::array* inputShadowPinsArray = nullptr;
        if (imgui_json::GetPtrTo(groupValue, "input_shadow_pins", inputShadowPinsArray)) // optional
        {
            for (auto& pinValue : *inputShadowPinsArray)
            {
                GetPinIDMap(pinValue, IDMaps);
            }
        }
        const imgui_json::array* outputShadowPinsArray = nullptr;
        if (imgui_json::GetPtrTo(groupValue, "output_shadow_pins", outputShadowPinsArray)) // optional
        {
            for (auto& pinValue : *outputShadowPinsArray)
            {
                GetPinIDMap(pinValue, IDMaps);
            }
        }

        const imgui_json::array* nodeArray = nullptr;
        if (imgui_json::GetPtrTo(value, "nodes", nodeArray))
        {
            for (auto& nodeValue : *nodeArray)
            {
                imgui_json::GetTo<imgui_json::number>(nodeValue, "id", object_id);
                IDMaps[object_id] = m_Blueprint->MakeNodeID(nullptr);
                const imgui_json::array* nodeInputPinsArray = nullptr;
                if (imgui_json::GetPtrTo(nodeValue, "input_pins", nodeInputPinsArray))
                {
                    for (auto& pinValue : *nodeInputPinsArray)
                    {
                        GetPinIDMap(pinValue, IDMaps);
                    }
                }
                const imgui_json::array* nodeOutputPinsArray = nullptr;
                if (imgui_json::GetPtrTo(nodeValue, "output_pins", nodeOutputPinsArray))
                {
                    for (auto& pinValue : *nodeOutputPinsArray)
                    {
                        GetPinIDMap(pinValue, IDMaps);
                    }
                }
            }
        }
        // Load Group Value
        Load(groupValue);
        auto GroupStatus = statusValue[edd::Serialization::ToString((const ed::NodeId)(m_ID))];
        m_ID = GetIDFromMap(m_ID, IDMaps);
        for (auto pin : m_InputBridgePins)
        {
            AdjestPinID(pin, IDMaps);
        }
        for (auto pin : m_OutputBridgePins)
        {
            AdjestPinID(pin, IDMaps);
        }
        for (auto pin : m_InputShadowPins)
        {
            AdjestPinID(pin, IDMaps);
        }
        for (auto pin : m_OutputShadowPins)
        {
            AdjestPinID(pin, IDMaps);
        }
        // Set group node status
        auto base_pos = ed::ScreenToCanvas(pos);
        ed::SetNodePosition(m_ID, base_pos);
        ImVec2 group_node_size;
        imgui_json::GetTo<imgui_json::number>(GroupStatus["size"], "x", group_node_size.x);
        imgui_json::GetTo<imgui_json::number>(GroupStatus["size"], "y", group_node_size.y);
        ed::SetNodeSize(m_ID, group_node_size);
        ImVec2 group_size;
        imgui_json::GetTo<imgui_json::number>(GroupStatus["group_size"], "x", group_size.x);
        imgui_json::GetTo<imgui_json::number>(GroupStatus["group_size"], "y", group_size.y);
        ed::SetGroupSize(m_ID, group_size);

        // Create Group In-Nodes
        const imgui_json::array* groupNodeArray = nullptr;
        if (imgui_json::GetPtrTo(value, "nodes", groupNodeArray))
        {
            for (auto& nodeValue : *groupNodeArray)
            {
                ID_TYPE typeId;
                if (!imgui_json::GetTo<imgui_json::number>(nodeValue, "type_id", typeId))
                    continue;
                auto node = m_Blueprint->CreateNode(typeId);
                if (!node)
                    continue;
                node->Load(nodeValue);
                auto nodeStatus = statusValue[edd::Serialization::ToString((const ed::NodeId)(node->m_ID))];
                node->m_ID = GetIDFromMap(node->m_ID, IDMaps);
                node->m_GroupID = GetIDFromMap(node->m_GroupID, IDMaps);
                ed::SetNodeGroupID(node->m_ID, node->m_GroupID);
                for (auto pin : node->GetInputPins())
                {
                    AdjestPinID(pin, IDMaps);
                }
                for (auto pin : node->GetOutputPins())
                {
                    AdjestPinID(pin, IDMaps);
                }
                ImVec2 node_location;
                imgui_json::GetTo<imgui_json::number>(nodeStatus["location"], "x", node_location.x);
                imgui_json::GetTo<imgui_json::number>(nodeStatus["location"], "y", node_location.y);
                node_location += base_pos;
                ed::SetNodePosition(node->m_ID, node_location);
                ImVec2 node_size;
                imgui_json::GetTo<imgui_json::number>(nodeStatus["size"], "x", node_size.x);
                imgui_json::GetTo<imgui_json::number>(nodeStatus["size"], "y", node_size.y);
                ed::SetNodeSize(node->m_ID, node_size);
                m_GroupNodes.push_back(node);
            }
        }
    }

    span<Pin*> GetInputPins() override { return m_InputBridgePins; }
    span<Pin*> GetOutputPins() override { return m_OutputBridgePins; }
    
    std::vector<Node *> m_GroupNodes;
    
    std::vector<Pin *> m_InputMapPins;
    std::vector<Pin *> m_OutputMapPins;

    std::vector<Pin *> m_InputBridgePins;
    std::vector<Pin *> m_OutputBridgePins;

    std::vector<Pin *> m_InputShadowPins;
    std::vector<Pin *> m_OutputShadowPins;

    bool m_Dragging {false};
    float m_ZPos {1.f};
    std::mutex m_mutex;
};
} // namespace BluePrint
