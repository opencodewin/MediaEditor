#include <BluePrint.h>
#include <Node.h>
#include <Utils.h>
#include <imgui_node_editor.h>
#include <imgui_node_editor_internal.h>

namespace ed = ax::NodeEditor;
namespace BluePrint
{
string PinTypeToString(PinType type)
{
    switch (type)
    {
        default:                return "UnKnown";
        case PinType::Void:     return "Void";
        case PinType::Any:      return "Any";
        case PinType::Flow:     return "Flow";
        case PinType::Bool:     return "Bool";
        case PinType::Int32:    return "Int32";
        case PinType::Int64:    return "Int64";
        case PinType::Float:    return "Float";
        case PinType::Double:   return "Double";
        case PinType::String:   return "String";
        case PinType::Point:    return "Point";
        case PinType::Vec2:     return "ImVec2";
        case PinType::Vec4:     return "ImVec4";
        case PinType::Mat:      return "ImMat";
        case PinType::Array:    return "Array";
        case PinType::Custom:   return "Custom";
    }
}

bool PinTypeFromString(string str, PinType& type)
{
    if (str.compare("Void") == 0)
        type = PinType::Void;
    else if (str.compare("Any") == 0)
        type = PinType::Any;
    else if (str.compare("Flow") == 0)
        type = PinType::Flow;
    else if (str.compare("Bool") == 0)
        type = PinType::Bool;
    else if (str.compare("Int32") == 0)
        type = PinType::Int32;
    else if (str.compare("Int64") == 0)
        type = PinType::Int64;
    else if (str.compare("Float") == 0)
        type = PinType::Float;
    else if (str.compare("Double") == 0)
        type = PinType::Double;
    else if (str.compare("String") == 0)
        type = PinType::String;
    else if (str.compare("Point") == 0)
        type = PinType::Point;
    else if (str.compare("ImVec2") == 0)
        type = PinType::Vec2;
    else if (str.compare("ImVec4") == 0)
        type = PinType::Vec4;
    else if (str.compare("ImMat") == 0)
        type = PinType::Mat;
    else if (str.compare("Array") == 0)
        type = PinType::Array;
    else if (str.compare("Custom") == 0)
        type = PinType::Custom;
    else
        return false;
    return true;
}

// ---------------------
// -------[ Pin ]-------
// ---------------------

Pin::Pin(Node* node, PinType type, std::string name)
    : m_ID(0)
    , m_Node(node)
    , m_Type(type)
    , m_Name(name)
{
    if (node && node->m_Blueprint)
    {
        m_ID = node->m_Blueprint->MakePinID(this);
    }
}

Pin::~Pin()
{
    if (m_Node && m_Node->m_ID && m_Node->m_Blueprint)
        m_Node->m_Blueprint->ForgetPin(this);
}

PinType Pin::GetType() const
{
    return m_Type;
}

LinkQueryResult Pin::CanLinkTo(const Pin& pin) const
{
    auto result = m_Node->AcceptLink(*this, pin);
    if (!result)
        return result;

    auto result2 = pin.m_Node->AcceptLink(*this, pin);
    if (!result2)
        return result2;

    if (result.Reason().empty())
        return result2;

    return result;
}

bool Pin::LinkTo(Pin& pin)
{
    if (!CanLinkTo(pin))
        return false;

    if (m_Link)
        Unlink();

    m_Link = pin.m_ID;

    m_Node->WasLinked(*this, pin);
    pin.m_Node->WasLinked(*this, pin);
    m_Flags |= PIN_FLAG_LINKED;
    pin.m_Flags |= PIN_FLAG_LINKED;

    if (std::find(pin.m_LinkFrom.begin(), pin.m_LinkFrom.end(), m_ID) == pin.m_LinkFrom.end())
    {
        pin.m_LinkFrom.push_back(m_ID);
    }
    ed::SetPinChanged(pin.m_ID);

    return true;
}

void Pin::Unlink()
{
    if (!m_Link)
        return;

    auto bp = m_Node->m_Blueprint;
    auto link = bp->GetPinFromID(m_Link);
    if (!link)
        return;

    m_Link = 0;

    m_Node->WasUnlinked(*this, *link);
    link->m_Node->WasUnlinked(*this, *link);
    m_Flags &= ~PIN_FLAG_LINKED;

    for (auto iter = link->m_LinkFrom.begin(); iter !=  link->m_LinkFrom.end();)
    {
        if ((*iter) == m_ID)
        {
            iter = link->m_LinkFrom.erase(iter);
        }
        else
        {
            iter ++;
        }
    }

    if (link->m_LinkFrom.size() == 0)
    {
        link->m_Flags &= ~PIN_FLAG_LINKED;
    }

    ed::SetLinkChanged(link->m_ID);
}

bool Pin::IsLinked() const
{
    return (m_Link || m_Flags & PIN_FLAG_LINKED);
}

Pin* Pin::GetLink(const BP *bp) const
{
    if (!m_Link)
        return nullptr;
    
    auto blueprint = bp;
    if (!bp)
        blueprint = m_Node->m_Blueprint;
    auto link = (Pin *)blueprint->GetPinFromID(m_Link);
    return link;
}

bool Pin::IsInput() const
{
    for (auto pin : m_Node->GetInputPins())
        if (pin->m_ID == m_ID)
            return true;

    return false;
}

bool Pin::IsOutput() const
{
    for (auto pin : m_Node->GetOutputPins())
        if (pin->m_ID == m_ID)
            return true;

    return false;
}

bool Pin::IsProvider() const
{
    auto outputToInput = (GetValueType() != PinType::Flow);

    auto pins = outputToInput ? m_Node->GetOutputPins() : m_Node->GetInputPins();

    for (auto pin : pins)
        if (pin->m_ID == m_ID)
            return true;

    return false;
}

bool Pin::IsReceiver() const
{
    auto outputToInput = (GetValueType() != PinType::Flow);

    auto pins = outputToInput ? m_Node->GetInputPins() : m_Node->GetOutputPins();

    for (auto pin : pins)
        if (pin->m_ID == m_ID)
            return true;

    return false;
}

bool Pin::IsMappedPin() const
{
    return (m_Flags & PIN_FLAG_BRIDGE) || (m_Flags & PIN_FLAG_SHADOW);
}

bool Pin::IsLinkedExportedPin() const
{
    if (!m_Node || !m_Node->m_Blueprint)
        return false;
    bool link_with_export = false;
    auto link = GetLink(m_Node->m_Blueprint);
    if (link && IsMappedPin() && (link->m_Flags & PIN_FLAG_EXPORTED))
    {
        link_with_export = true;
    }
    else if (link && (m_Flags & PIN_FLAG_EXPORTED) && link->IsMappedPin())
    {
        link_with_export = true;
    }
    return link_with_export;
}

bool Pin::Load(const imgui_json::value& value)
{
    string pinType;
    if (!imgui_json::GetTo<imgui_json::string>(value, "type", pinType)) // required
        return false;
    PinTypeFromString(pinType, m_Type);

    if (!imgui_json::GetTo<imgui_json::number>(value, "id", m_ID)) // required
        return false;

    if (value.contains("link"))
        imgui_json::GetTo<imgui_json::number>(value, "link", m_Link); // optional

    if (value.contains("map"))
        imgui_json::GetTo<imgui_json::number>(value, "map", m_MappedPin); // optional
    
    if (value.contains("flags"))
        imgui_json::GetTo<imgui_json::number>(value, "flags", m_Flags); // optional

    if (value.contains("name"))
        imgui_json::GetTo<imgui_json::string>(value, "name", m_Name);

    const imgui_json::array* LinkFromPinsArray = nullptr;
    if (imgui_json::GetPtrTo(value, "link_from", LinkFromPinsArray)) // optional
    {
        m_LinkFrom.clear();
        for (auto& pinValue : *LinkFromPinsArray)
        {
            ID_TYPE id;
            if (imgui_json::GetTo<imgui_json::number>(pinValue, "link_id", id))
                m_LinkFrom.push_back(id);
        }
    }

    return true;
}

void Pin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    value["id"] = imgui_json::number(GetIDFromMap(m_ID, MapID)); // required
    value["type"] = PinTypeToString(m_Type);
    if (m_Link) value["link"] = imgui_json::number(GetIDFromMap(m_Link, MapID));
    value["map"] = imgui_json::number(GetIDFromMap(m_MappedPin, MapID));
    value["flags"] = imgui_json::number(m_Flags);
    if (!m_Name.empty())
        value["name"] = m_Name;  // optional, to make data readable for humans
    auto& LinkFromPinsValue = value["link_from"]; // optional
    for (auto& pinid : m_LinkFrom)
    {
        imgui_json::value pinValue;
        auto ID = GetIDFromMap(pinid, MapID);
        if (ID)
        {
            pinValue["link_id"] = imgui_json::number(ID);
            LinkFromPinsValue.push_back(pinValue);
        }
    }
    if (LinkFromPinsValue.is_null())
        value.erase("link_from");
}

PinType Pin::GetValueType() const
{
    return m_Type;
}

PinValue Pin::GetValue() const
{
    switch (m_Type)
    {
        case PinType::Any:
            return ((AnyPin*)this)->GetValue();
        case PinType::Flow:
            return ((FlowPin*)this)->GetValue();
        case PinType::Bool:
            return ((BoolPin*)this)->GetValue();
        case PinType::Int32:
            return ((Int32Pin*)this)->GetValue();
        case PinType::Int64:
            return ((Int64Pin*)this)->GetValue();
        case PinType::Float: 
            return ((FloatPin*)this)->GetValue();
        case PinType::Double:
            return ((DoublePin*)this)->GetValue();
        case PinType::String:
            return ((StringPin*)this)->GetValue();
        case PinType::Point:
            return ((PointPin*)this)->GetValue();
        case PinType::Vec2:
            return ((Vec2Pin*)this)->GetValue();
        case PinType::Vec4:
            return ((Vec4Pin*)this)->GetValue();
        case PinType::Mat:
            return ((MatPin*)this)->GetValue();
        case PinType::Array:
            return ((ArrayPin*)this)->GetValue();
        case PinType::Custom:
            return ((CustomPin*)this)->GetValue();
        default: break;
    }
    return PinValue{};
}

// ----------------------------
// ---[Internal Pin Define]----
// ----------------------------
// AnyPin
bool AnyPin::SetValueType(PinType type)
{
    if (GetValueType() == type)
        return true;

    if (m_InnerPin)
    {
        m_Node->m_Blueprint->ForgetPin(m_InnerPin.get());
        m_InnerPin.reset();
    }

    if (type == PinType::Any)
        return true;

    m_InnerPin = m_Node->CreatePin(type);

    if (auto link = GetLink())
    {
        if (link->GetValueType() != type)
        {
            Unlink();
            LinkTo(*link);
        }
    }

    auto linkedToSet = m_Node->m_Blueprint->FindPinsLinkedTo(*this);
    for (auto linkedTo : linkedToSet)
    {
        if (linkedTo->GetValueType() == type)
            continue;
        linkedTo->Unlink();
        linkedTo->LinkTo(*this);
    }

    return true;
}

bool AnyPin::SetValue(const PinValue& value)
{
    if (!m_InnerPin)
        return false;
    return m_InnerPin->SetValue(std::move(value));
}

bool AnyPin::Load(const imgui_json::value& value)
{
    if (!Pin::Load(value))
        return false;

    PinType type = PinType::Any;
    string typeName;
    if (!imgui_json::GetTo<imgui_json::string>(value, "vtype", typeName)) // required
        return false;
    PinTypeFromString(typeName, type);
    if (type != PinType::Any)
    {
        m_InnerPin = m_Node->CreatePin(type);
        if (!value.contains("inner"))
            return false;
        if (!m_InnerPin->Load(value["inner"]))
            return false;
    }

    return true;
}

void AnyPin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    Pin::Save(value, MapID);
    value["vtype"] = PinTypeToString(GetValueType());
    if (m_InnerPin)
        m_InnerPin->Save(value["inner"], MapID);
}

// BoolPin
bool BoolPin::Load(const imgui_json::value& value)
{
    if (!Pin::Load(value))
        return false;
    if (!imgui_json::GetTo<bool>(value, "value", m_Value)) // required
        return false;
    return true;
}

void BoolPin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    Pin::Save(value, MapID);
    value["value"] = m_Value; // required
}

// Int32Pin
bool Int32Pin::Load(const imgui_json::value& value)
{
    if (!Pin::Load(value))
        return false;
    if (!imgui_json::GetTo<imgui_json::number>(value, "value", m_Value)) // required
        return false;
    return true;
}

void Int32Pin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    Pin::Save(value, MapID);
    value["value"] = imgui_json::number(m_Value); // required
}

// Int64Pin
bool Int64Pin::Load(const imgui_json::value& value)
{
    if (!Pin::Load(value))
        return false;
    if (!imgui_json::GetTo<imgui_json::number>(value, "value", m_Value)) // required
        return false;
    return true;
}

void Int64Pin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    Pin::Save(value, MapID);
    value["value"] = imgui_json::number(m_Value); // required
}

// FloatPin
bool FloatPin::Load(const imgui_json::value& value)
{
    if (!Pin::Load(value))
        return false;
        
    auto& typeValue = value["value"];
    if (typeValue.is_string())
    {
        m_Value = NAN;
    }
    if (!imgui_json::GetTo<imgui_json::number>(value, "value", m_Value)) // required
        return false;
    return true;
}

void FloatPin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    Pin::Save(value, MapID);
    if (isnan(m_Value))
        value["value"] = imgui_json::string("NAN"); // required
    else
        value["value"] = imgui_json::number(m_Value); // required
}

// DoublePin
bool DoublePin::Load(const imgui_json::value& value)
{
    if (!Pin::Load(value))
        return false;
    
    auto& typeValue = value["value"];
    if (typeValue.is_string())
    {
        m_Value = NAN;
    }
    else if (!imgui_json::GetTo<imgui_json::number>(value, "value", m_Value)) // required
        return false;
    return true;
}

void DoublePin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    Pin::Save(value, MapID);
    if (isnan(m_Value))
        value["value"] = imgui_json::string("NAN");
    else
        value["value"] = imgui_json::number(m_Value); // required
}

// StringPin
bool StringPin::Load(const imgui_json::value& value)
{
    if (!Pin::Load(value))
        return false;
    if (!imgui_json::GetTo<imgui_json::string>(value, "value", m_Value)) // required
        return false;
    return true;
}

void StringPin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    Pin::Save(value, MapID);
    value["value"] = m_Value; // required
}

// PointPin
bool PointPin::Load(const imgui_json::value& value)
{
    if (!Pin::Load(value))
        return false;
    // do we need load/save point value into json?
    //if (!imgui_json::GetTo<imgui_json::point>(value, "value", m_Value)) // required
    //    return false;
    // do we need save point type ?
    return true;
}

void PointPin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    Pin::Save(value, MapID);
    // do we need load/save point value into json?
    //value["value"] = imgui_json::point(m_Value); // required
    // do we need save point type ?
}

// ArrayPin
bool ArrayPin::Load(const imgui_json::value& value)
{
    if (!Pin::Load(value))
        return false;
    // TODO::Dicky ArrayPin load
    // do we need load/save array value into json?
    return true;
}

void ArrayPin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    Pin::Save(value, MapID);
    // TODO::Dicky ArrayPin save
    // do we need load/save array value into json?
}

// Vec2Pin
bool Vec2Pin::Load(const imgui_json::value& value)
{
    if (!Pin::Load(value))
        return false;
    if (!imgui_json::GetTo<imgui_json::number>(value["vec"], "x", m_Value.x))
        return false;
    if (!imgui_json::GetTo<imgui_json::number>(value["vec"], "y", m_Value.y))
        return false;
    return true;
}

void Vec2Pin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    Pin::Save(value, MapID);
    value["vec"] = ed::Detail::Serialization::ToJson(m_Value);
}

// Vec4Pin
bool Vec4Pin::Load(const imgui_json::value& value)
{
    if (!Pin::Load(value))
        return false;
    if (!imgui_json::GetTo<imgui_json::number>(value["vec"], "x", m_Value.x))
        return false;
    if (!imgui_json::GetTo<imgui_json::number>(value["vec"], "y", m_Value.y))
        return false;
    if (!imgui_json::GetTo<imgui_json::number>(value["vec"], "z", m_Value.z))
        return false;
    if (!imgui_json::GetTo<imgui_json::number>(value["vec"], "w", m_Value.w))
        return false;
    return true;
}

void Vec4Pin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    Pin::Save(value, MapID);
    value["vec"] = ed::Detail::Serialization::ToJson(m_Value);
}

// MatPin
bool MatPin::Load(const imgui_json::value& value)
{
    if (!Pin::Load(value))
        return false;
    return true;
}

void MatPin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    Pin::Save(value, MapID);
}

// CustomPin
void CustomPin::InitPinEx()
{
    if (m_pPinEx)
    {
        delete m_pPinEx;
        m_pPinEx = nullptr;
    }
    m_pPinEx = m_Node->m_Blueprint->GetPinExRegistry()->Create(m_ExTypeName);
}

bool CustomPin::SyncValue()
{
    if (m_Link) 
    {
        auto bp = m_Node->m_Blueprint;
        CustomPin* inPin = static_cast<CustomPin*>(GetLink(bp));
        inPin->SyncValue();
        inPin->m_DataAccessLock.lock();
        PinValue pinVal = inPin->m_pPinEx->GetCustomPinValue();
        inPin->m_DataAccessLock.unlock();
        SetValue(pinVal);
        return true;
    }
    return false;
}

void CustomPin::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const
{
    Pin::Save(value, MapID);
    value["extype_name"] = m_ExTypeName;
}

bool CustomPin::Load(const imgui_json::value& value)
{
    if (!Pin::Load(value))
        return false;
    if (!imgui_json::GetTo<imgui_json::string>(value, "extype_name", m_ExTypeName))
        return false;

    InitPinEx();
    return true;
}

LinkQueryResult CustomPin::CanLinkTo(const Pin& pin) const
{
    LinkQueryResult lqres = Pin::CanLinkTo(pin);
    if (!lqres) {
        return lqres;
    }

    if (!m_pPinEx) {
        return {false, "The 'm_pPinEx' of this pin is not initialized!"};
    }
    const CustomPin& cuspin = reinterpret_cast<const CustomPin&>(pin);
    if (!cuspin.m_pPinEx) {
        return {false, "The 'm_pPinEx' of argument pin is not initialized!"};
    }
    const PinTypeEx& this_expin_type = m_pPinEx->GetTypeEx();
    const PinTypeEx& oprd_expin_type = cuspin.m_pPinEx->GetTypeEx();
    if (this_expin_type != oprd_expin_type) {
        std::stringstream reason;
        reason << "PinTypeEx '" << this_expin_type.GetName() << "' does NOT match '" << oprd_expin_type.GetName() << "'!";
        return {false, reason.str()};
    }
    return {true, ""};
}

PinExRegistry::~PinExRegistry()
{
    if (m_dll_handle)
        dlclose(m_dll_handle);
}

const PinTypeEx* PinExRegistry::RegisterPinEx(std::string module_path)
{
    m_dll_handle = dlopen(module_path.c_str(), RTLD_LAZY);
	if (!m_dll_handle) {
		std::cerr << "Failed to open library: " << dlerror() << std::endl;
		return nullptr;
	}

	// Reset errors
	dlerror();

	GET_PINEX_MODULE_INFO_FN* pfnGetPinExModuleInfo = (GET_PINEX_MODULE_INFO_FN*) dlsym(m_dll_handle, "GetPinExModuleInfo");
	const char* err = dlerror();
	if (err) {
		std::cerr << "Failed to load version symbol: " << err << std::endl;
		dlclose(m_dll_handle);
        m_dll_handle = nullptr;
		return nullptr;
	}

    const PinExModuleInfo* pModInfo = pfnGetPinExModuleInfo();
    if (pModInfo == nullptr) {
        std::cerr << "PinExModulueInfo is NULL from '" << module_path << "'!" << std::endl;
        dlclose(m_dll_handle);
        m_dll_handle = nullptr;
        return nullptr;
    }

    for (auto pInfo : m_TypeInfos) {
        if (pInfo->m_TypeEx == pModInfo->m_TypeEx) {
            std::cerr << "Conflict PinTypeEx '" << pModInfo->m_TypeEx.GetName() << "', FAILED to load PinEx from '" << module_path << "'!" << std::endl;
            dlclose(m_dll_handle);
            m_dll_handle = nullptr;
            return nullptr;
        }
    }

    m_TypeInfos.push_back(pModInfo);
    return &pModInfo->m_TypeEx;
}

PinEx* PinExRegistry::Create(std::string typeName)
{
    for (auto typeInfo : m_TypeInfos) {
        if (typeInfo->m_TypeEx.GetName() == typeName) {
            return typeInfo->m_CreatorFn();
        }
    }
    return nullptr;
}
}
