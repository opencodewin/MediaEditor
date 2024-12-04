#pragma once
#include <iostream>
#include <BluePrint.h>
#include <immat.h>

#define PIN_FLAG_NONE       (0)
#define PIN_FLAG_IN         (1<<0)
#define PIN_FLAG_OUT        (1<<1)
#define PIN_FLAG_BRIDGE     (1<<2)
#define PIN_FLAG_SHADOW     (1<<3)
#define PIN_FLAG_LINKED     (1<<4)
#define PIN_FLAG_EXPORTED   (1<<5)
#define PIN_FLAG_PUBLICIZED (1<<6)
#define PIN_FLAG_FORCESHOW  (1<<7)

struct PinExModuleInfo;

namespace BluePrint
{
enum class PinType: int32_t 
{
    Void = -1, 
    Any, 
    Flow, 
    Bool, 
    Int32,
    Int64,
    Float, 
    Double,
    String, 
    Point,
    Vec2,
    Vec4,
    Mat,
    Array,
    Custom
};

class IMGUI_API PinTypeEx
{
public:
    PinTypeEx(const std::string& name) :
        m_Name(name) {}
    PinTypeEx(const PinTypeEx& oprd) :
        m_Name(oprd.m_Name) {}
    ~PinTypeEx() {}

    const std::string& GetName() const { return m_Name; }

    friend bool operator==(const PinTypeEx& lhs, const PinTypeEx& rhs) { return lhs.m_Name == rhs.m_Name; }
    friend bool operator!=(const PinTypeEx& lhs, const PinTypeEx& rhs) { return !(lhs == rhs); }

private:
    const std::string   m_Name;
};

struct PinValueEx
{
    PinValueEx() {}
    virtual ~PinValueEx() {}

    virtual const std::type_info& GetTypeInfo() const = 0;
    virtual PinValueEx* CreateCopy() const = 0;
    virtual bool CheckIdentical(const PinValueEx& r) const = 0;
    virtual void* GetVoidPtr() const = 0;
};

struct LinkQueryResult;
struct FlowPin;
struct PinValue
{
    using ValueType = variant<monostate, FlowPin*, bool, int32_t, int64_t, float, double, std::string, uintptr_t, ImVec2, ImVec4, ImGui::ImMat, imgui_json::array, PinValueEx*>;

    PinValue() = default;
    PinValue(const PinValue&) = default;
    PinValue(PinValue&&) = default;
    PinValue& operator=(const PinValue&) = default;
    PinValue& operator=(PinValue&&) = default;

    PinValue(FlowPin* pin): m_Value(pin) {}
    PinValue(bool value): m_Value(value) {}
    PinValue(int32_t value): m_Value(value) {}
    PinValue(int64_t value): m_Value(value) {}
    PinValue(float value): m_Value(value) {}
    PinValue(double value): m_Value(value) {}
    PinValue(uintptr_t value): m_Value(value) {}
    PinValue(std::string&& value): m_Value(std::move(value)) {}
    PinValue(const std::string& value): m_Value(value) {}
    PinValue(const char* value): m_Value(std::string(value)) {}
    PinValue(const ImVec2 value): m_Value(value) {}
    PinValue(const ImVec4 value): m_Value(value) {}
    PinValue(ImGui::ImMat value): m_Value(value) {}
    PinValue(imgui_json::array value): m_Value(value) {}
    PinValue(PinValueEx* valex)
    {
        if (valex)
            m_Value = valex->CreateCopy();
        else
            m_Value = valex;
    }
    PinValue(PinValueEx*&& valex): m_Value(std::move(valex)) {}

    ~PinValue()
    {
        if (GetType() == PinType::Custom)
        {
            PinValueEx* pPinValEx = As<PinValueEx*>();
            delete pPinValEx;
        }
    }

    PinType GetType() const { return static_cast<PinType>(m_Value.index()); }

    template <typename T>
    T& As()
    {
        return get<T>(m_Value);
    }

    template <typename T>
    const T& As() const
    {
        return get<T>(m_Value);
    }

private:
    ValueType m_Value;
};

struct Node;
struct BP;
struct IMGUI_API Pin
{
    Pin(Node* node, PinType type, std::string name = "");
    virtual ~Pin();

    virtual bool     SetValueType(PinType type) { return m_Type == type; }  // By default, type of held value cannot be changed
    virtual PinType  GetValueType() const;                                  // Returns type of held value (may be different from GetType() for Any pin)
    virtual bool     SetValue(const PinValue& value) { return false; }      // Sets new value to be held by the pin (not all allow data to be modified)
    virtual PinValue GetValue() const;                                      // Returns value held by this pin
    //virtual PinValue GetValue();
    PinType          GetType() const;                                       // Returns type of this pin (which may differ from the type of held value for AnyPin)

    virtual LinkQueryResult CanLinkTo(const Pin& pin) const;                // Check if link between this and other pin can be created
    bool LinkTo(Pin& pin);                              // Tries to create link to specified pin as a provider
    void Unlink();                                      // Breaks link from this receiver to provider
    bool IsLinked() const;                              // Return true if this pin is linked to provider
    Pin* GetLink(const BP *bp = nullptr) const;         // Return provider pin

    bool IsInput() const;                               // Pin is on input side
    bool IsOutput() const;                              // Pin is on output side

    bool IsProvider() const;                            // Pin can provide data
    bool IsReceiver() const;                            // Pin can receive data

    bool IsMappedPin() const;                           // Pin is Bridge/Shadow pin
    bool IsLinkedExportedPin() const;                   // Pin is linked with group export pin

    virtual bool Load(const imgui_json::value& value);
    virtual void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const;

    ID_TYPE         m_ID        {static_cast<ID_TYPE>(-1)};
    Node*           m_Node      {nullptr};
    PinType         m_Type      {PinType::Void};
    string          m_Name;
    ID_TYPE         m_Link      {static_cast<ID_TYPE>(0)};
    ID_TYPE         m_Flags     {PIN_FLAG_NONE};

    std::vector<ID_TYPE> m_LinkFrom;

    // For Bridge/Shadow Pin
    ID_TYPE         m_MappedPin {static_cast<ID_TYPE>(0)};
};

template<class T>
class PinValueExImpl : public PinValueEx
{
public:
    template<class Y>
    PinValueExImpl(Y* ValuePtr) :
        m_Shptr(ValuePtr)
    {}
    template<class Y, class Deleter>
    PinValueExImpl(Y* ValuePtr, Deleter d)
    {
        m_Shptr = std::shared_ptr<T>(ValuePtr, d);
    }
    template<class Y>
    PinValueExImpl(const std::shared_ptr<Y>& ValueShptr)
    {
        m_Shptr = std::shared_ptr<T>(ValueShptr);
    }
    template<class Y>
    PinValueExImpl(const PinValueExImpl<Y>& r)
    {
        m_Shptr = std::shared_ptr<T>(r.m_Shptr);
    }

    virtual ~PinValueExImpl()
    {
        // std::cout << "Delete <PinValueExImpl*>(" << this << "), holding ptr (" << m_Shptr.get() << ")." << std::endl;
    }

    T* GetValuePtr() const
    { return m_Shptr.get(); }

    const std::type_info& GetTypeInfo() const override
    { return typeid(T); }

    PinValueEx* CreateCopy() const override
    {
        // PinValueExImpl<T>* res = new PinValueExImpl<T>(*this);
        // std::cout << "Create <PinValueExImpl*>(" << res << "), holding ptr (" << GetVoidPtr() << ")." << std::endl;
        // return res;
        return new PinValueExImpl<T>(*this);
    }

    bool CheckIdentical(const PinValueEx& r) const override
    {
        if (GetTypeInfo() != r.GetTypeInfo())
            return false;
        if (m_Shptr != static_cast<const PinValueExImpl<T>&>(r).m_Shptr)
            return false;
        return true;
    }

    void* GetVoidPtr() const override
    {
        return m_Shptr.get();
    }

private:
    std::shared_ptr<T>  m_Shptr;
};

class IMGUI_API PinEx
{
public:
    PinEx() {}
    virtual ~PinEx()
    {
        if (m_pPinValueEx)
        {
            delete m_pPinValueEx;
            m_pPinValueEx = nullptr;
        }
    }

    virtual const PinTypeEx& GetTypeEx() const = 0;

    PinValue GetPointPinValue() const
    {
        return PinValue(reinterpret_cast<uintptr_t>(m_pPinValueEx));
    }

    PinValue GetCustomPinValue() const
    {
        return PinValue(m_pPinValueEx);
    }

    void SetPinValueEx(const PinValueEx* pPinValueEx)
    {
        if (m_pPinValueEx && pPinValueEx && m_pPinValueEx->CheckIdentical(*pPinValueEx)) 
        {
            return;
        }
        if (m_pPinValueEx) 
        {
            delete m_pPinValueEx;
            m_pPinValueEx = nullptr;
        }
        if (pPinValueEx)
        {
            m_pPinValueEx = pPinValueEx->CreateCopy();
        }
    }

    virtual void SetValuePtr(void* valuePtr, const std::type_info& typeInfo) = 0;

    template<class T>
    T* GetValuePtr() const
    {
        if (!m_pPinValueEx)
            return nullptr;
        const std::type_info& typeinfo1 = typeid(T);
        const std::type_info& typeinfo2 = m_pPinValueEx->GetTypeInfo();
        if (typeinfo1 != typeinfo2)
        {
            std::stringstream ss;
            ss << "PinValueExImpl value type MISMATCH! '" << typeinfo1.name() << "' != '" << typeinfo2.name() << "'.";
            throw std::runtime_error(ss.str());
        }
        return (static_cast<PinValueExImpl<T>*>(m_pPinValueEx))->GetValuePtr();
    }

protected:
    PinValueEx*     m_pPinValueEx   {nullptr};
};

// ----------------------------
// ---[Internal Pin Define]----
// ----------------------------
// FlowPin represent execution flow
struct IMGUI_API FlowPin final : Pin
{
    static constexpr auto TypeId = PinType::Flow;

    FlowPin(): FlowPin(nullptr) {}
    FlowPin(Node* node): Pin(node, PinType::Flow) {}
    FlowPin(Node* node, std::string name): Pin(node, PinType::Flow, name) {}

    PinValue GetValue() const override { return const_cast<FlowPin*>(this); }
};

// AnyPin can morph into any other data pin while creating a link
struct IMGUI_API AnyPin final : Pin
{
    static constexpr auto TypeId = PinType::Any;

    AnyPin(Node* node, std::string name = ""): Pin(node, PinType::Any, name) {}

    bool     SetValueType(PinType type) override;
    PinType  GetValueType() const override { return m_InnerPin ? m_InnerPin->GetValueType() : Pin::GetValueType(); }
    bool     SetValue(const PinValue& value) override;
    PinValue GetValue() const override { return m_InnerPin ? m_InnerPin->GetValue() : PinValue{}; }

    bool Load(const imgui_json::value& value) override;
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const override;

    std::unique_ptr<Pin> m_InnerPin;
};

// Boolean type pin
struct IMGUI_API BoolPin final : Pin
{
    static constexpr auto TypeId = PinType::Bool;

    BoolPin(Node* node, bool value = false)
        : Pin(node, PinType::Bool)
        , m_Value(value)
    {}

    // C++ implicitly convert literals to bool, this will intercept
    // such calls an do the right thing.
    template <size_t N>
    BoolPin(Node* node, const char (&name)[N], bool value = false)
        : Pin(node, PinType::Bool, name)
        , m_Value(value)
    {}

    BoolPin(Node* node, std::string name, bool value = false)
        : Pin(node, PinType::Bool, name)
        , m_Value(value)
    {}

    bool SetValue(const PinValue& value) override
    {
        if (value.GetType() != TypeId)
            return false;
        m_Value = value.As<bool>();
        return true;
    }

    PinValue GetValue() const override { return m_Value; }

    bool Load(const imgui_json::value& value) override;
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const override;

    bool m_Value = false;
};

// Integer 32bit type pin
struct IMGUI_API Int32Pin final : Pin
{
    static constexpr auto TypeId = PinType::Int32;

    Int32Pin(Node* node, int32_t value = 0): Pin(node, PinType::Int32), m_Value(value) {}
    Int32Pin(Node* node, std::string name, int32_t value = 0): Pin(node, PinType::Int32, name), m_Value(value) {}

    bool SetValue(const PinValue& value) override
    {
        if (value.GetType() != TypeId)
            return false;
        m_Value = value.As<int32_t>();
        return true;
    }

    PinValue GetValue() const override { return m_Value; }

    bool Load(const imgui_json::value& value) override;
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const override;

    int32_t m_Value = 0;
};

// Integer 64bit type pin
struct IMGUI_API Int64Pin final : Pin
{
    static constexpr auto TypeId = PinType::Int64;

    Int64Pin(Node* node, int64_t value = 0): Pin(node, PinType::Int64), m_Value(value) {}
    Int64Pin(Node* node, std::string name, int64_t value = 0): Pin(node, PinType::Int64, name), m_Value(value) {}

    bool SetValue(const PinValue& value) override
    {
        if (value.GetType() != TypeId)
            return false;
        m_Value = value.As<int64_t>();
        return true;
    }

    PinValue GetValue() const override { return m_Value; }

    bool Load(const imgui_json::value& value) override;
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const override;

    int64_t m_Value = 0;
};

// Floating point 32bit type pin
struct IMGUI_API FloatPin final : Pin
{
    static constexpr auto TypeId = PinType::Float;
    FloatPin(Node* node, float value = 0.0f): Pin(node, PinType::Float), m_Value(value) {}
    FloatPin(Node* node, std::string name, float value = 0.0f): Pin(node, PinType::Float, name), m_Value(value) {}

    bool SetValue(const PinValue& value) override
    {
        if (value.GetType() != TypeId)
            return false;
        m_Value = value.As<float>();
        return true;
    }

    PinValue GetValue() const override { return m_Value; }

    bool Load(const imgui_json::value& value) override;
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const override;

    float m_Value = 0.0f;
};

// Floating point 64bit type pin
struct IMGUI_API DoublePin final : Pin
{
    static constexpr auto TypeId = PinType::Double;
    DoublePin(Node* node, double value = 0.0f): Pin(node, PinType::Double), m_Value(value) {}
    DoublePin(Node* node, std::string name, double value = 0.0f): Pin(node, PinType::Double, name), m_Value(value) {}

    bool SetValue(const PinValue& value) override
    {
        if (value.GetType() != TypeId)
            return false;
        m_Value = value.As<double>();
        return true;
    }

    PinValue GetValue() const override { return m_Value; }

    bool Load(const imgui_json::value& value) override;
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const override;

    double m_Value = 0.0f;
};

// String type pin
struct IMGUI_API StringPin final : Pin
{
    static constexpr auto TypeId = PinType::String;

    StringPin(Node* node, std::string value = ""): Pin(node, PinType::String), m_Value(value) {}
    StringPin(Node* node, std::string name, std::string value = ""): Pin(node, PinType::String, name), m_Value(value) {}

    bool SetValue(const PinValue& value) override
    {
        if (value.GetType() != TypeId)
            return false;
        m_Value = value.As<std::string>();
        return true;
    }

    PinValue GetValue() const override { return m_Value; }

    bool Load(const imgui_json::value& value) override;
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const override;

    std::string m_Value;
};

// Point type pin
struct IMGUI_API PointPin final : Pin
{
    static constexpr auto TypeId = PinType::Point;

    PointPin(Node* node, uintptr_t value = 0): Pin(node, PinType::Point), m_Value(value) {}
    PointPin(Node* node, std::string name, uintptr_t value = 0): Pin(node, PinType::Point, name), m_Value(value) {}

    bool SetValue(const PinValue& value) override
    {
        if (value.GetType() != TypeId)
            return false;
        m_Value = value.As<uintptr_t>();
        return true;
    }

    PinValue GetValue() const override { return m_Value; }

    bool Load(const imgui_json::value& value) override;
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const override;

    uintptr_t m_Value;
};

// Vec2 ImVec2 type pin
struct IMGUI_API Vec2Pin final : Pin
{
    static constexpr auto TypeId = PinType::Vec2;
    Vec2Pin(Node* node, ImVec2 value = {}): Pin(node, PinType::Vec2), m_Value(value) {}
    Vec2Pin(Node* node, std::string name, ImVec2 value = {}): Pin(node, PinType::Vec2, name), m_Value(value) {}

    bool SetValue(const PinValue& value) override
    {
        if (value.GetType() != TypeId)
            return false;
        m_Value = value.As<ImVec2>();
        return true;
    }

    PinValue GetValue() const override { return m_Value; }

    bool Load(const imgui_json::value& value) override;
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const override;

    ImVec2 m_Value {0.f, 0.f};
};

// Vec4 ImVec4 type pin
struct IMGUI_API Vec4Pin final : Pin
{
    static constexpr auto TypeId = PinType::Vec4;
    Vec4Pin(Node* node, ImVec4 value = {}): Pin(node, PinType::Vec4), m_Value(value) {}
    Vec4Pin(Node* node, std::string name, ImVec4 value = {}): Pin(node, PinType::Vec4, name), m_Value(value) {}

    bool SetValue(const PinValue& value) override
    {
        if (value.GetType() != TypeId)
            return false;
        m_Value = value.As<ImVec4>();
        return true;
    }

    PinValue GetValue() const override { return m_Value; }

    bool Load(const imgui_json::value& value) override;
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const override;

    ImVec4 m_Value {0.f, 0.f, 0.f, 0.f};
};

// Array type pin
struct IMGUI_API ArrayPin final : Pin
{
    static constexpr auto TypeId = PinType::Array;
    ArrayPin(Node* node, imgui_json::array value = {}): Pin(node, PinType::Array), m_Value(value) {}
    ArrayPin(Node* node, std::string name, imgui_json::array value = {}): Pin(node, PinType::Array, name), m_Value(value) {}

    bool SetValue(const PinValue& value) override
    {
        if (value.GetType() != TypeId)
            return false;
        m_Value = value.As<imgui_json::array>();
        return true;
    }

    PinValue GetValue() const override { return m_Value; }

    bool Load(const imgui_json::value& value) override;
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const override;

    imgui_json::array m_Value;
};

// Mat ImMat type pin
struct IMGUI_API MatPin final : Pin
{
    static constexpr auto TypeId = PinType::Mat;
    MatPin(Node* node, ImGui::ImMat value = {}): Pin(node, PinType::Mat), m_Value(value) {}
    MatPin(Node* node, std::string name, ImGui::ImMat value = {}): Pin(node, PinType::Mat, name), m_Value(value) {}

    bool SetValue(const PinValue& value) override
    {
        if (value.GetType() != TypeId)
            return false;
        m_Value = value.As<ImGui::ImMat>();
        return true;
    }

    PinValue GetValue() const override { return m_Value; }

    bool Load(const imgui_json::value& value) override;
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const override;

    ImGui::ImMat m_Value = {};
};
struct IMGUI_API CustomPin final : Pin
{
    static constexpr auto TypeId = PinType::Custom;

    CustomPin(Node* node, const std::string& extype_name):
        Pin(node, PinType::Custom), m_ExTypeName(extype_name)
    { InitPinEx(); }
    CustomPin(Node* node, const std::string& extype_name, std::string name, uintptr_t value = 0) :
        Pin(node, PinType::Custom, name), m_ExTypeName(extype_name)
    { InitPinEx(); }
    
    ~CustomPin() { if (m_pPinEx) delete m_pPinEx; }

    void InitPinEx();

    bool SetValue(const PinValue& value) override
    {
        if (value.GetType() != TypeId)
            return false;
        m_DataAccessLock.lock();
        m_pPinEx->SetPinValueEx(value.As<PinValueEx*>());
        m_DataAccessLock.unlock();
        return true;
    }

    PinValue GetValue() const override
    {
        return m_pPinEx->GetPointPinValue();
    }

    bool SyncValue();

    template<class T>
    void SetValuePtr(T* ptr)
    {
        m_pPinEx->SetValuePtr(ptr, typeid(T));
    }

    template<class T>
    T* GetValuePtr()
    {
        SyncValue();
        return m_pPinEx->GetValuePtr<T>();
    }

    bool Load(const imgui_json::value& value) override;
    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {}) const override;

    LinkQueryResult CanLinkTo(const Pin& pin) const override;
    PinEx& GetPinEx() const { return *m_pPinEx; }

private:
    std::string     m_ExTypeName        {""};
    PinEx*          m_pPinEx            {nullptr};
    std::mutex      m_DataAccessLock;
};

class PinExRegistry
{
public:
    PinExRegistry() {}
    ~PinExRegistry();

    const PinTypeEx* RegisterPinEx(std::string module_path);
    PinEx* Create(std::string typeName);

private:
    std::vector<const PinExModuleInfo*>  m_TypeInfos;
    void* m_dll_handle {nullptr};
};
} // namespace BluePrint

namespace BluePrint
{
    IMGUI_API string PinTypeToString(PinType type);
    IMGUI_API bool PinTypeFromString(string str, PinType& type);
} // namespace BluePrint

struct PinExModuleInfo
{
    using CREATOR_FN = BluePrint::PinEx*(*)(void);

    BluePrint::PinTypeEx    m_TypeEx;
    VERSION_TYPE            m_Version;
    CREATOR_FN              m_CreatorFn;
};

typedef const PinExModuleInfo* GET_PINEX_MODULE_INFO_FN();

# define BP_PINEX_DYNAMIC(clazz, type, version) \
    static const PinExModuleInfo PINEX_MODULE_INFO__##clazz = { \
        type, \
        version, \
        []() -> ::BluePrint::PinEx* { return new ::BluePrint::clazz(); } \
    }; \
    extern "C" EXPORT const PinExModuleInfo* GetPinExModuleInfo() { \
        return &PINEX_MODULE_INFO__##clazz; \
    }
