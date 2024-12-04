#include <Document.h>
#include <Utils.h>
#include <Debug.h>

namespace BluePrint
{
static string SaveReasonFlagsToString(ed::SaveReasonFlags flags, std::string separator = ", ")
{
    ImGuiTextBuffer builder;

    auto testFlag = [flags](ed::SaveReasonFlags flag)
    {
        return (flags & flag) == flag;
    };

    if (testFlag(ed::SaveReasonFlags::Navigation))
        builder.appendf("Navigation%" PRI_sv, FMT_sv(separator));
    if (testFlag(ed::SaveReasonFlags::Position))
        builder.appendf("Position%" PRI_sv, FMT_sv(separator));
    if (testFlag(ed::SaveReasonFlags::Size))
        builder.appendf("Size%" PRI_sv, FMT_sv(separator));
    if (testFlag(ed::SaveReasonFlags::Selection))
        builder.appendf("Selection%" PRI_sv, FMT_sv(separator));
    if (testFlag(ed::SaveReasonFlags::Node))
        builder.appendf("Node%" PRI_sv, FMT_sv(separator));
    if (testFlag(ed::SaveReasonFlags::Pin))
        builder.appendf("Pin%" PRI_sv, FMT_sv(separator));
    if (testFlag(ed::SaveReasonFlags::Link))
        builder.appendf("Link%" PRI_sv, FMT_sv(separator));
    if (testFlag(ed::SaveReasonFlags::User))
        builder.appendf("User%" PRI_sv, FMT_sv(separator));

    if (builder.empty())
        return "None";
    else
        return string(builder.c_str(), builder.size() - separator.size());
}

imgui_json::value Document::DocumentState::Serialize() const
{
    imgui_json::value result;
    result["nodes"] = m_NodesState;
    result["selection"] = m_SelectionState;
    result["blueprint"] = m_BlueprintState;
    return result;
}

int Document::DocumentState::Deserialize(const imgui_json::value& value, DocumentState& result)
{
    DocumentState state;

    if (!value.is_object())
        return BP_ERR_DOC_LOAD;

    if (!value.contains("nodes") || !value.contains("selection") || !value.contains("blueprint"))
        return BP_ERR_DOC_LOAD;

    auto& nodesValue = value["nodes"];
    auto& selectionValue = value["selection"];
    auto& dataValue = value["blueprint"];

    if (!nodesValue.is_object())
        return BP_ERR_DOC_LOAD;

    state.m_NodesState     = nodesValue;
    state.m_SelectionState = selectionValue;
    state.m_BlueprintState = dataValue;

    result = std::move(state);

    return BP_ERR_NONE;
}

Document::UndoTransaction::UndoTransaction(Document& document, std::string name)
    : m_Name(name)
    , m_Document(&document)
{
}

Document::UndoTransaction::~UndoTransaction()
{
    Commit();
}

void Document::UndoTransaction::Begin(std::string name /*= ""*/)
{
    if (m_HasBegan)
        return;

    if (m_Document->m_MasterTransaction)
    {
        m_MasterTransaction = m_Document->m_MasterTransaction->shared_from_this();
    }
    else
    {
        // Spawn master transaction which commit on destruction
        m_MasterTransaction = m_Document->GetDeferredUndoTransaction(m_Name);
        m_Document->m_MasterTransaction = m_MasterTransaction.get();
        m_MasterTransaction->Begin();
        m_MasterTransaction->m_MasterTransaction = nullptr;
    }

    m_HasBegan = true;

    m_State.m_State = m_Document->m_DocumentState;

    if (!name.empty())
        AddAction(name);
}

void Document::UndoTransaction::AddAction(std::string name)
{
    if (!m_HasBegan || name.empty())
        return;

    m_Actions.appendf("%" PRI_sv "\n", FMT_sv(name));
}

void Document::UndoTransaction::AddAction(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    ImGuiTextBuffer buffer;
    buffer.appendfv(format, args);
    va_end(args);

    AddAction(std::string(buffer.Buf.Data, buffer.Buf.Size));
}

void Document::UndoTransaction::Commit(std::string name)
{
    if (!m_HasBegan || m_IsDone)
        return;

    if (m_MasterTransaction)
    {
        if (!name.empty())
        {
            m_MasterTransaction->m_Actions.append(name.data(), name.data() + name.size());
            m_MasterTransaction->m_Actions.append("\n");
        }
        else
        {
            m_MasterTransaction->m_Actions.append(m_Actions.c_str());
        }

        IM_ASSERT(m_MasterTransaction->m_IsDone == false);
    }
    else
    {
        IM_ASSERT(m_Document->m_MasterTransaction == this);

        if (!m_Actions.empty())
        {
            bool need_undo = true;
            if (name.empty())
                name = std::string(m_Actions.c_str(), m_Actions.size() - 1);

            if (name.find("ClearSelection") != string::npos ||
                name.find("Select Node") != string::npos ||
                name.find("Selection Changed") != string::npos ||
                name.find("Clear Selection") != string::npos ||
                name.find("Select Link") != string::npos ||
                name.find("Navigate") != string::npos ||
                name.find("Zoom") != string::npos ||
                name.find("Scroll") != string::npos ||
                name.find("User") != string::npos)
            {
                need_undo = false; // ignore undo
                LOGV("[Action] : %" PRI_sv, FMT_sv(name));
            }

            m_State.m_Name = name;

            if (need_undo)
            {
                //LOGV("[UndoTransaction] Commit: %" PRI_sv, FMT_sv(name));
                m_Document->m_Undo.emplace_back(std::move(m_State));
                m_Document->m_Redo.clear();
            }

            m_Document->m_DocumentState = m_Document->BuildDocumentState();
        }

        m_Document->m_MasterTransaction = nullptr;
    }

    m_IsDone = true;
}

void Document::UndoTransaction::Discard()
{
    if (!m_HasBegan || m_IsDone)
        return;

    if (!m_MasterTransaction)
    {
        IM_ASSERT(m_Document->m_MasterTransaction == this);

        m_Document->m_MasterTransaction = nullptr;
    }

    m_IsDone = true;
}

void Document::OnSaveBegin()
{
    m_SaveTransaction = BeginUndoTransaction("Save");
}

bool Document::OnSaveNodeState(ID_TYPE nodeId, const imgui_json::value& value, ed::SaveReasonFlags reason)
{
    if (reason != ed::SaveReasonFlags::Size)
    {
        auto node = m_Blueprint.FindNode(nodeId);
        if (!node)
            return false;
        ImGuiTextBuffer buffer;
        buffer.appendf("%" PRI_node " %s", FMT_node(node), SaveReasonFlagsToString(reason).c_str());
        m_SaveTransaction->AddAction("%s", buffer.c_str());
    }

    return true;
}

bool Document::OnSaveState(const imgui_json::value& value, ed::SaveReasonFlags reason)
{
    if ((reason & ed::SaveReasonFlags::Selection) == ed::SaveReasonFlags::Selection)
    {
        m_DocumentState.m_SelectionState = ed::GetState(ed::StateType::Selection);
        m_SaveTransaction->AddAction("Selection Changed");
    }

    if ((reason & ed::SaveReasonFlags::Navigation) == ed::SaveReasonFlags::Navigation)
    {
        m_NavigationState.m_ViewState = ed::GetState(ed::StateType::View);
    }

    return true;
}

void Document::OnSaveEnd()
{
    m_SaveTransaction = nullptr;

    m_DocumentState = BuildDocumentState();
}

imgui_json::value Document::OnLoadNodeState(ID_TYPE nodeId) const
{
    return {};
}

imgui_json::value Document::OnLoadState() const
{
    return {};
}

shared_ptr<Document::UndoTransaction> Document::BeginUndoTransaction(std::string name, std::string action /*= ""*/)
{
    auto transaction = std::make_shared<UndoTransaction>(*this, name);
    transaction->Begin(action);
    return transaction;
}

shared_ptr<Document::UndoTransaction> Document::GetDeferredUndoTransaction(std::string name)
{
    return std::make_shared<UndoTransaction>(*this, name);
}

void Document::SetPath(std::string path)
{
    m_Path = path;

    auto lastSeparator = m_Path.find_last_of("\\/");
    if (lastSeparator != string::npos)
        m_Name = m_Path.substr(lastSeparator + 1);
    else
        m_Name = path;
}

imgui_json::value Document::Serialize() const
{
    imgui_json::value result;
    result["document"] = m_DocumentState.Serialize();
    result["view"] = m_NavigationState.m_ViewState;
    return result;
}

int Document::Deserialize(const imgui_json::value& value, Document& result)
{
    if (!value.is_object())
        return BP_ERR_DOC_LOAD;

    if (!value.contains("document") || !value.contains("view"))
        return BP_ERR_DOC_LOAD;

    auto& documentValue = value["document"];
    auto& viewValue = value["view"];

    if (DocumentState::Deserialize(documentValue, result.m_DocumentState) != 0)
        return BP_ERR_DOC_LOAD;

    result.m_NavigationState.m_ViewState = viewValue;

    if (result.m_Blueprint.Load(result.m_DocumentState.m_BlueprintState) != 0)
        return BP_ERR_DOC_LOAD;

    return BP_ERR_NONE;
}

int Document::Load(std::string path)
{
    int ret = BP_ERR_NONE;
    auto loadResult = imgui_json::value::load(path);
    if (!loadResult.second)
        return BP_ERR_DOC_LOAD;

    if ((ret = Deserialize(loadResult.first, *this)) != BP_ERR_NONE)
        return ret;

    return ret;
}

int Document::Import(std::string path, ImVec2 pos)
{
    int ret = BP_ERR_NONE;
    auto loadResult = imgui_json::value::load(path);
    if (!loadResult.second)
        return BP_ERR_GROUP_LOAD;

    return m_Blueprint.Import(loadResult.first, pos);
}

bool Document::Save(std::string path) const
{
    auto result = Serialize();
    return result.save(path);
}

bool Document::Save() const
{
    if (m_Path.empty())
        return false;
    return Save(m_Path);
}

bool Document::Undo()
{
    if (m_Undo.empty())
        return false;

    auto state = std::move(m_Undo.back());
    m_Undo.pop_back();

    LOGI("[Document] Undo: %s", state.m_Name.c_str());

    UndoState undoState;
    undoState.m_Name = state.m_Name;
    undoState.m_State = m_DocumentState;

    ApplyState(state.m_State);

    m_Redo.push_back(std::move(undoState));

    return true;
}

bool Document::Redo()
{
    if (m_Redo.empty())
        return true;

    auto state = std::move(m_Redo.back());
    m_Redo.pop_back();

    LOGI("[Document] Redo: %s", state.m_Name.c_str());

    UndoState undoState;
    undoState.m_Name = state.m_Name;
    undoState.m_State = m_DocumentState;

    ApplyState(state.m_State);

    m_Undo.push_back(std::move(undoState));

    return true;
}

Document::DocumentState Document::BuildDocumentState()
{
    DocumentState result;
    m_Blueprint.Save(result.m_BlueprintState);
    result.m_SelectionState = ed::GetState(ed::StateType::Selection);
    result.m_NodesState = ed::GetState(ed::StateType::Nodes);
    return result;
}

void Document::ApplyState(const NavigationState& state)
{
    m_NavigationState = state;
    ed::ApplyState(ed::StateType::View, m_NavigationState.m_ViewState);
}

void Document::ApplyState(const DocumentState& state)
{
    //m_Blueprint.Load(state.m_BlueprintState);
    // TODO::Dicky do we need load bp again since we already load on document

    m_DocumentState = state;
    ed::ApplyState(ed::StateType::Nodes, m_DocumentState.m_NodesState);
    ed::ApplyState(ed::StateType::Selection, m_DocumentState.m_SelectionState);
}

void Document::OnMakeCurrent()
{
    ApplyState(m_DocumentState);
    ApplyState(m_NavigationState);
}

} // namespace BluePrint