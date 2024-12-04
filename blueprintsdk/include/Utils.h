#pragma once
#include <imgui.h>
#include <imgui_helper.h>
#include <imgui_node_editor.h>
#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <Icon.h>
#include <inttypes.h>

namespace ed = ax::NodeEditor;

# define PRI_sv             ".*s"
# define FMT_sv(sv)         static_cast<int>((sv).size()), (sv).data()

# define PRI_pin            "s%s%" PRI_sv "%s(0x%08" PRIX32 ")"
# define FMT_pin(pin)       "Pin", (pin)->m_Name.empty() ? "" : " \"", FMT_sv((pin)->m_Name), (pin)->m_Name.empty() ? "" : "\"", (pin)->m_ID

# define PRI_node           "s%" PRI_sv "%s(0x%08" PRIX32 ")"
# define FMT_node(node)     (node)->GetName().empty() ? "" : " \"", FMT_sv((node)->GetName()), (node)->GetName().empty() ? "" : "\"", (node)->m_ID
namespace BluePrint
{
struct BluePrintUI;
ImFont* HeaderFont();
IconType PinTypeToIconType(PinType pinType); // Returns icon for corresponding pin type.
ImVec4 PinTypeToColor(BluePrintUI* ui, PinType pinType); // Returns color for corresponding pin type.
bool DrawPinValue(const PinValue& value); // Draw widget representing pin value.
bool EditPinValue(Pin& pin); // Show editor for pin. Returns true if edit is complete.
void DrawPinValueWithEditor(Pin& pin); // Draw pin value or editor if value is clicked.
const vector<Node*> GetSelectedNodes(BP* blueprint); // Returns selected nodes as a vector.
const vector<Node*> GetGroupedNodes(Node& node); // Returns grouped nodes as a vector.
const vector<Pin*> GetSelectedLinks(BP* blueprint); // Returns selected links as a vector.
const char * StepResultToString(StepResult stepResult);
std::string IDToHexString(const ID_TYPE i);
ID_TYPE GetIDFromMap(ID_TYPE ID, std::map<ID_TYPE, ID_TYPE> MapID);
// Uses ImDrawListSplitter to draw background under pin value
struct PinValueBackgroundRenderer
{
    PinValueBackgroundRenderer();
    PinValueBackgroundRenderer(const ImVec4 color, float alpha = 0.25f);
    ~PinValueBackgroundRenderer();

    void Commit();
    void Discard();

private:
    ImDrawList* m_DrawList = nullptr;
    ImDrawListSplitter m_Splitter;
    ImVec4 m_Color;
    float m_Alpha = 1.0f;
};

// Wrapper over flat API for item construction
struct ItemBuilder
{
    struct NodeBuilder
    {
        ed::PinId m_PinId = ed::PinId::Invalid;

        bool Accept();
        void Reject();
    };

    struct LinkBuilder
    {
        ed::PinId m_StartPinId = ed::PinId::Invalid;
        ed::PinId m_EndPinId = ed::PinId::Invalid;

        bool Accept();
        void Reject();
    };

    ItemBuilder();
    ~ItemBuilder();

    explicit operator bool() const;

    NodeBuilder* QueryNewNode();
    LinkBuilder* QueryNewLink();

private:
    bool m_InCreate = false;
    NodeBuilder m_NodeBuilder;
    LinkBuilder m_LinkBuilder;
};

// Wrapper over flat API for item deletion
struct ItemDeleter
{
    struct NodeDeleter
    {
        ed::NodeId m_NodeId = ed::NodeId::Invalid;

        bool Accept(bool deleteLinks = true);
        void Reject();
    };

    struct LinkDeleter
    {
        ed::LinkId m_LinkId     = ed::LinkId::Invalid;
        ed::PinId  m_StartPinId = ed::PinId::Invalid;
        ed::PinId  m_EndPinId   = ed::PinId::Invalid;

        bool Accept();
        void Reject();
    };

    ItemDeleter();
    ~ItemDeleter();

    explicit operator bool() const;

    NodeDeleter* QueryDeletedNode();
    LinkDeleter* QueryDeleteLink();

private:
    bool m_InDelete = false;
    NodeDeleter m_NodeDeleter;
    LinkDeleter m_LinkDeleter;
};
} // namespace BluePrint