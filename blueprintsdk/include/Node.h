#pragma once
#include <BluePrint.h>
#include <Pin.h>
#include <Debug.h>
#include <imgui_node_editor.h>
#include <imgui_extra_widget.h>
#include <imgui_curve.h>
#include <inttypes.h>
#include <DynObjectLoader.h>

#if IMGUI_ICONS
#define ICON_NODE               u8"\uf542"
#else
#define ICON_NODE               "N"
#endif

namespace ed = ax::NodeEditor;

namespace BluePrint
{
enum class NodeType:int32_t 
{
    Internal = 0,
    EntryPoint,
    ExitPoint,
    External,
    Dummy,          // In case load failed extra node
};

enum class NodeStyle:int32_t 
{
    Dummy = 0,
    Default, 
    Simple, 
    Comment,
    Group,
    Custom
};

static inline std::vector<std::string> GetCatalogInfo(std::string filter)
{
    std::vector<std::string> calalogs;
    std::string s;
    std::istringstream f(filter);
    while (std::getline(f, s, '#'))
    {
        calalogs.push_back(s);
    }
    return calalogs;
}

struct LinkQueryResult
{
    LinkQueryResult(bool result, std::string reason = "")
        : m_Result(result)
        , m_Reason(reason)
    {}

    explicit operator bool() const { return m_Result; }

    bool Result() const { return m_Result; }
    const string& Reason() const { return m_Reason; }

private:
    bool    m_Result = false;
    string  m_Reason;
};

struct BP;
struct Node;
struct Pin;
struct Context;
struct NodeTypeInfo
{
    using Factory = Node*(*)(BP* blueprint);

    NodeTypeInfo() = default;
    NodeTypeInfo(ID_TYPE id, std::string type_name, std::string name, VERSION_TYPE version, NodeType type, NodeStyle style, std::string catalog, Factory factory)
    {
        m_ID = id; m_NodeTypeName = type_name; m_Name = name; m_Version = version; m_Type = type; m_Style = style; m_Catalog = catalog; m_Factory = factory;
    }
    NodeTypeInfo(ID_TYPE id, std::string type_name, std::string name, std::string author, VERSION_TYPE version, VERSION_TYPE sdk_version, NodeType type, NodeStyle style, std::string catalog, Factory factory)
    {
        m_ID = id; m_NodeTypeName = type_name; m_Name = name; m_Author = author; m_Version = version; m_SDK_Version = sdk_version; m_Type = type; m_Style = style; m_Catalog = catalog; m_Factory = factory;
    }
    NodeTypeInfo(ID_TYPE id, std::string type_name, std::string name, std::string author, VERSION_TYPE version, VERSION_TYPE sdk_version, VERSION_TYPE api_version, NodeType type, NodeStyle style, std::string catalog, Factory factory)
    {
        m_ID = id; m_NodeTypeName = type_name; m_Name = name; m_Author = author; m_Version = version; m_SDK_Version = sdk_version; m_API_Version = api_version; m_Type = type; m_Style = style; m_Catalog = catalog; m_Factory = factory;
    }

    ID_TYPE         m_ID;
    std::string     m_NodeTypeName;
    std::string     m_Name;
    std::string     m_Author;
    VERSION_TYPE    m_Version {0};
    VERSION_TYPE    m_SDK_Version {0};
    VERSION_TYPE    m_API_Version {0};
    NodeType        m_Type;
    NodeStyle       m_Style;
    std::string     m_Catalog;
    Factory         m_Factory;

    std::string     m_Url;

	// for dynamic loading of the object
	typedef int32_t version_t();
	typedef NodeTypeInfo* create_t();
	typedef void destroy_t(NodeTypeInfo*);
};

struct IMGUI_API Node
{
    Node(BP* blueprint);
    virtual ~Node() = default;

    template <typename T>
    unique_ptr<T> CreatePin(std::string name = "");
    unique_ptr<Pin> CreatePin(PinType pinType, std::string name = "");
    Pin * NewPin(PinType pinType, std::string name = "");
    
    virtual void Reset(Context& context) // Reset state of the node before execution. Allows to set initial state for the specified execution context.
    {
        m_Tick = 0;
        m_Hits = 0;
        m_NodeTimeMs = 0;
    }

    virtual void Update() {}  // Update Node
    virtual void PreLoad() {} // pre-load node resource

    virtual void OnPause(Context& context) {}
    virtual void OnResume(Context& context) {}
    virtual void OnStepNext(Context& context) {}
    virtual void OnStepCurrent(Context& context) {}
    virtual void OnStop(Context& context) {}
    virtual void OnClose(Context& context) {}

    virtual void OnDragStart(const Context& context) {}
    virtual void OnDragging(const Context& context) {}
    virtual void OnDragEnd(const Context& context) {}
    virtual void OnResize(const Context& context) {}
    virtual void OnSelect(const Context& context) {}
    virtual void OnDeselect(const Context& context) {}

    virtual Pin* InsertInputPin(PinType type, const std::string name)
    {
        return nullptr;
    }

    virtual Pin* InsertOutputPin(PinType type, const std::string name)
    {
        return nullptr;
    }

    virtual void DeleteOutputPin(const std::string name)
    {
    }

    virtual FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) // Executes node logic from specified entry point. Returns exit point (flow pin on output side) or nothing.
    {
        return {};
    }

    virtual PinValue EvaluatePin(const Context& context, const Pin& pin, bool threading = false) const // Evaluates pin in node in specified execution context.
    {
        return pin.GetValue();
    }

    virtual Pin* FindPin(std::string name)
    {
        auto inpins = GetInputPins();
        for (auto pin : inpins)
        {
            if (pin->m_Name.compare(name) == 0)
                return pin;
        }
        auto outpins = GetOutputPins();
        for (auto pin : outpins)
        {
            if (pin->m_Name.compare(name) == 0)
                return pin;
        }
        return nullptr;
    }

    virtual bool Link(std::string outpin, Node* node, std::string inpin)
    {
        auto link_pin = FindPin(outpin);
        auto linked_pin = node->FindPin(inpin);
        if (link_pin && linked_pin) 
            return link_pin->LinkTo(*linked_pin);
        else
            return false;
    }

    virtual bool Link(Pin & outpin, Node* node, std::string inpin)
    {
        auto linked_pin = node->FindPin(inpin);
        if (linked_pin) 
            return outpin.LinkTo(*linked_pin);
        else
            return false;
    }

    virtual bool Link(std::string inpin, Pin & outpin)
    {
        auto link_pin = FindPin(inpin);
        if (link_pin) 
            return link_pin->LinkTo(outpin);
        else
            return false;
    }

    virtual bool SetPinValue(std::string pin, PinValue value)
    {
        auto need_pin = FindPin(pin);
        if (need_pin)
            return need_pin->SetValue(value);
        else
            return false;
    }

    virtual NodeTypeInfo    GetTypeInfo() const { return {}; }

    virtual NodeType        GetType() const;
    virtual VERSION_TYPE    GetVersion() const;
    virtual VERSION_TYPE    GetSDKVersion() const;
    virtual VERSION_TYPE    GetAPIVersion() const;
    virtual std::string     GetAuthor() const;
    virtual std::string     GetURL() const;
    virtual ID_TYPE         GetTypeID() const;
    virtual NodeStyle       GetStyle() const;
    virtual std::string     GetCatalog() const;
    virtual std::string     GetName() const;
    virtual void            SetName(std::string name);
    virtual void            SetBreakPoint(bool breaken);
    virtual bool            IsSelected();

    virtual LinkQueryResult AcceptLink(const Pin& receiver, const Pin& provider); // Checks if node accept link between these two pins. There node can filter out unsupported link types.
    virtual void            WasLinked(const Pin& receiver, const Pin& provider); // Notifies node that link involving one of its pins has been made.
    virtual void            WasUnlinked(const Pin& receiver, const Pin& provider); // Notifies node that link involving one of its pins has been broken.

    virtual span<Pin*>      GetInputPins() { return {}; } // Returns list of input pins of the node
    virtual span<Pin*>      GetOutputPins() { return {}; } // Returns list of output pins of the node
    virtual Pin*            GetAutoLinkInputFlowPin() { return nullptr; } // Return auto link flow pin which as input
    virtual Pin*            GetAutoLinkOutputFlowPin() { return nullptr; } // Return auto link flow pin which as output
    virtual vector<Pin*>    GetAutoLinkInputDataPin() { return {}; } // Return auto link data pin which as input
    virtual vector<Pin*>    GetAutoLinkOutputDataPin() { return {}; } // Return auto link data pin which as output
    virtual FlowPin*        GetOutputFlowPin() { return nullptr; } // return Output FlowPin point
    virtual void            OnNodeDelete(Node * node = nullptr) {};

    virtual int  Load(const imgui_json::value& value);
    virtual void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID = {});

    virtual bool DrawSettingLayout(ImGuiContext * ctx);
    virtual void DrawMenuLayout(ImGuiContext * ctx);
    virtual bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key = nullptr, bool embedded = true);
    virtual void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo = std::string(ICON_NODE)) const;
    virtual void DrawNodeLogo(ImTextureID logo, int& index, int cols, int rows, ImVec2 size) const;
    virtual ImTextureID LoadNodeLogo(void * data, int size) const;

    static bool DrawDataTypeSetting(const char * label, ImDataType& type, bool full_type = false);

    ID_TYPE         m_ID                {0};
    string          m_Name              {""};
    BP*             m_Blueprint         {nullptr};
    int             m_IconHovered       {-1};
    bool            m_HasSetting        {true};
    bool            m_SettingAutoResize {true};
    bool            m_HasCustomLayout   {false};
    bool            m_BreakPoint        {false};
    bool            m_NoBackGround      {false};
    bool            m_Skippable         {false};
    bool            m_Enabled           {true};
    bool            m_BGRequired        {false};
    float           m_Transparency      {0.0};
    ID_TYPE         m_GroupID           {0};
    std::mutex      m_mutex;

    // for Node banchmark
    uint64_t        m_Tick {0};
    uint64_t        m_Hits {0};
    double          m_NodeTimeMs    {0.f};
    // for avg banchmark
    int             m_HitCount      {0};
    double          m_CountTimeMs   {0.f};
    double          m_AvgTimeMs     {0.f};
};

struct ClipNode
{
    ClipNode(Node* node)
    {
        m_NodeInfo = node->GetTypeInfo();
        m_Name = node->m_Name;
        m_Pos = ed::GetNodePosition(node->m_ID);
        m_Size = ed::GetNodeSize(node->m_ID);
        m_GroupSize = ed::GetGroupSize(node->m_ID);
        m_HasSetting = node->m_HasSetting;
        m_Skippable = node->m_Skippable;
        m_SettingAutoResize = node->m_SettingAutoResize;
        m_HasCustomLayout = node->m_HasCustomLayout;
        m_BreakPoint = node->m_BreakPoint;
        m_Enabled = node->m_Enabled;
    }

    NodeTypeInfo    m_NodeInfo;
    ImVec2          m_Pos               {ImVec2{0, 0}};
    ImVec2          m_Size              {ImVec2{0, 0}};
    ImVec2          m_GroupSize         {ImVec2{0, 0}};
    string          m_Name              {""};
    bool            m_HasSetting        {true};
    bool            m_SettingAutoResize {true};
    bool            m_HasCustomLayout   {false};
    bool            m_Skippable         {false};
    bool            m_BreakPoint        {false};
    bool            m_Enabled           {true};
};

struct IMGUI_API NodeRegistry
{
    NodeRegistry();
    ~NodeRegistry();
    ID_TYPE RegisterNodeType(shared_ptr<NodeTypeInfo> info);
    ID_TYPE RegisterNodeType(std::string Path);
    void UnregisterNodeType(std::string name);
    Node* Create(ID_TYPE typeId, BP* blueprint);
    Node* Create(std::string typeName, BP* blueprint);
    span<const NodeTypeInfo* const> GetTypes() const;
    span<const std::string> GetCatalogs() const;
    span<const Node * const> GetNodes() const;
    const NodeTypeInfo* GetTypeInfo(ID_TYPE typeId) const;

private:
    void RebuildTypes();
    std::vector<NodeTypeInfo>   m_BuildInNodes;
    std::vector<NodeTypeInfo>   m_CustomNodes;
    std::vector<NodeTypeInfo*>  m_Types;
    std::vector<std::string>    m_Catalogs;
    std::vector<DLClass<NodeTypeInfo>*> m_ExternalObject;
    std::vector<Node *>         m_Nodes;
};

} // namespace BluePrint

namespace BluePrint
{
    ID_TYPE NodeTypeIDFromName(string type, string catalog);
    string NodeTypeToString(NodeType type);
    bool NodeTypeFromString(string str, NodeType& type);
    string NodeStyleToString(NodeStyle style);
    bool NodeStyleFromString(string str, NodeStyle& style);
    string NodeVersionToString(VERSION_TYPE version);
    bool NodeVersionFromString(string str, VERSION_TYPE& version);

} // namespace BluePrint

namespace BluePrint
{
    template <typename T>
    inline unique_ptr<T> Node::CreatePin(std::string name /*= ""*/)
    {
        if (auto pin = CreatePin(T::TypeId, name))
            return unique_ptr<T>(static_cast<T*>(pin.release()));
        else
            return nullptr;
    }
}// namespace BluePrint

#define IS_ENTRY_EXIT_NODE(type) (type == BluePrint::NodeType::EntryPoint || type == BluePrint::NodeType::ExitPoint)