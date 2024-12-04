#pragma once
#include <imgui.h>
#include <imgui_node_editor.h>
#include <imgui_json.h>
#include <BluePrint.h>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <stdint.h>

namespace ed = ax::NodeEditor;
namespace BluePrint
{
struct IMGUI_API Document
{
    struct DocumentState
    {
        imgui_json::value m_NodesState;
        imgui_json::value m_SelectionState;
        imgui_json::value m_BlueprintState;

        imgui_json::value Serialize() const;

        static int Deserialize(const imgui_json::value& value, DocumentState& result);
    };

    struct NavigationState
    {
        imgui_json::value                   m_ViewState;
    };

    struct UndoState
    {
        string          m_Name;
        DocumentState   m_State;
    };

    struct UndoTransaction
        : std::enable_shared_from_this<UndoTransaction>
    {
        UndoTransaction(Document& document, std::string name);
        UndoTransaction(UndoTransaction&&) = delete;
        UndoTransaction(const UndoTransaction&) = delete;
        ~UndoTransaction();

        UndoTransaction& operator=(UndoTransaction&&) = delete;
        UndoTransaction& operator=(const UndoTransaction&) = delete;

        void Begin(std::string name = "");
        void AddAction(std::string name);
        void AddAction(const char* format, ...) IM_FMTARGS(2);
        void Commit(std::string name = "");
        void Discard();

        const Document* GetDocument() const { return m_Document; }

    private:
        string                      m_Name;
        Document*                   m_Document = nullptr;
        UndoState                   m_State;
        ImGuiTextBuffer             m_Actions;
        bool                        m_HasBegan = false;
        bool                        m_IsDone = false;
        shared_ptr<UndoTransaction> m_MasterTransaction;
    };

    [[nodiscard]]
    shared_ptr<UndoTransaction> BeginUndoTransaction(std::string name, std::string action = "");
    [[nodiscard]]

    shared_ptr<UndoTransaction> GetDeferredUndoTransaction(std::string name);

    void SetPath(std::string path);

    imgui_json::value Serialize() const;

    static int Deserialize(const imgui_json::value& value, Document& result);

    int  Load(std::string path);
    int  Import(std::string path, ImVec2 pos);
    bool Save(std::string path) const;
    bool Save() const;

    bool Undo();
    bool Redo();

    DocumentState BuildDocumentState();
    void ApplyState(const DocumentState& state);
    void ApplyState(const NavigationState& state);

    void OnMakeCurrent();

    void OnSaveBegin();
    bool OnSaveNodeState(ID_TYPE nodeId, const imgui_json::value& value, ed::SaveReasonFlags reason);
    bool OnSaveState(const imgui_json::value& value, ed::SaveReasonFlags reason);
    void OnSaveEnd();

    imgui_json::value OnLoadNodeState(ID_TYPE nodeId) const;
    imgui_json::value OnLoadState() const;

            BP& GetBlueprint()       { return m_Blueprint; }
    const   BP& GetBlueprint() const { return m_Blueprint; }

    string                  m_Path;
    string                  m_Name;
    string                  m_CatalogFilter;
    bool                    m_IsModified = false;
    vector<UndoState>       m_Undo;
    vector<UndoState>       m_Redo;

    DocumentState           m_DocumentState;
    NavigationState         m_NavigationState;

    UndoTransaction*        m_MasterTransaction = nullptr;

    shared_ptr<UndoTransaction> m_SaveTransaction = nullptr;

    BP                      m_Blueprint;
    void *                  m_UserData {nullptr};
};

} // namespace BluePrint