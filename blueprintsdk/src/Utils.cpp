#include <Utils.h>
#include <Document.h>
#include <imgui_helper.h>
#include <imgui_extra_widget.h>
#include <inttypes.h>
#include <UI.h>
#include <Debug.h>

namespace BluePrint
{
ImFont* HeaderFont()
{
    ImFont * font = nullptr;
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->Fonts.size()>1) font = io.Fonts->Fonts[1];
    return font;
}

const char* StepResultToString(StepResult stepResult)
{
    switch (stepResult)
    {
        case StepResult::Success:   return "Success";
        case StepResult::Done:      return "Done";
        case StepResult::Error:     return "Error";
    }

    return "";
}

std::string IDToHexString(const ID_TYPE i) 
{
    std::stringstream s;
    s << "0x" << std::uppercase << std::hex << i;
    return s.str();
}

ID_TYPE GetIDFromMap(ID_TYPE ID, std::map<ID_TYPE, ID_TYPE> MapID)
{
    if (MapID.size() > 0)
    {
        std::map<ID_TYPE, ID_TYPE>::iterator it;
        it = MapID.find(ID);
        if (it == MapID.end())
            return 0;
        else
            return it->second;
    }
    return ID;
}

IconType PinTypeToIconType(PinType pinType)
{
    switch (pinType)
    {
        default:                return IconType::Circle;
        case PinType::Void:     return IconType::Circle;
        case PinType::Any:      return IconType::Diamond;
        case PinType::Flow:     return IconType::Flow;
        case PinType::Bool:     return IconType::Circle;
        case PinType::Int32:    return IconType::Circle;
        case PinType::Int64:    return IconType::Circle;
        case PinType::Float:    return IconType::Circle;
        case PinType::Double:   return IconType::Circle;
        case PinType::String:   return IconType::Circle;
        case PinType::Point:    return IconType::Circle;
        case PinType::Vec2:     return IconType::Bracket;
        case PinType::Vec4:     return IconType::Bracket;
        case PinType::Mat:      return IconType::Grid;
        case PinType::Array:    return IconType::BracketSquare;
        case PinType::Custom:   return IconType::Square;
    }

    return IconType::Circle;
}

ImVec4 PinTypeToColor(BluePrintUI* ui, PinType pinType)
{
    switch (pinType)
    {
        default:                return ui->m_StyleColors[BluePrintStyleColor_PinVoid];
        case PinType::Void:     return ui->m_StyleColors[BluePrintStyleColor_PinVoid];
        case PinType::Any:      return ui->m_StyleColors[BluePrintStyleColor_PinAny];
        case PinType::Flow:     return ui->m_StyleColors[BluePrintStyleColor_PinFlow];
        case PinType::Bool:     return ui->m_StyleColors[BluePrintStyleColor_PinBool];
        case PinType::Int32:    return ui->m_StyleColors[BluePrintStyleColor_PinInt32];
        case PinType::Int64:    return ui->m_StyleColors[BluePrintStyleColor_PinInt64];
        case PinType::Float:    return ui->m_StyleColors[BluePrintStyleColor_PinFloat];
        case PinType::Double:   return ui->m_StyleColors[BluePrintStyleColor_PinDouble];
        case PinType::String:   return ui->m_StyleColors[BluePrintStyleColor_PinString];
        case PinType::Point:    return ui->m_StyleColors[BluePrintStyleColor_PinPoint];
        case PinType::Vec2:     return ui->m_StyleColors[BluePrintStyleColor_PinVector];
        case PinType::Vec4:     return ui->m_StyleColors[BluePrintStyleColor_PinVector];
        case PinType::Mat:      return ui->m_StyleColors[BluePrintStyleColor_PinMat];
        case PinType::Array:    return ui->m_StyleColors[BluePrintStyleColor_PinPoint];
        case PinType::Custom:   return ui->m_StyleColors[BluePrintStyleColor_PinCustom];
    }

    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

bool DrawPinValue(const PinValue& value)
{
    switch (value.GetType())
    {
        case PinType::Void:
            return false;
        case PinType::Any:
            return false;
        case PinType::Flow:
            return false;
        case PinType::Bool:
            if (value.As<bool>())
                ImGui::TextUnformatted("true");
            else
                ImGui::TextUnformatted("false");
            return true;
        case PinType::Int32:
            ImGui::Text("%d", value.As<int32_t>());
            return true;
        case PinType::Int64:
            ImGui::Text("%" PRId64, value.As<int64_t>());
            return true;
        case PinType::Float:
            ImGui::Text("%g", value.As<float>());
            return true;
        case PinType::Double:
            ImGui::Text("%g", value.As<double>());
            return true;
        case PinType::String:
            ImGui::Text("%s", value.As<string>().c_str());
            return true;
        case PinType::Point:
#if defined(__EMSCRIPTEN__)
            ImGui::Text("0x%0lX", value.As<uintptr_t>());
#else
            ImGui::Text("0x%0" PRIXPTR, value.As<uintptr_t>());
#endif
            return true;
        case PinType::Vec2:
            {
                auto val = value.As<ImVec2>();
                ImGui::Text("%g:%g", val.x, val.y);
            }
            return false;
        case PinType::Vec4:
            {
                auto val = value.As<ImVec4>();
                ImGui::Text("%g:%g:%g:%g", val.x, val.y, val.z, val.w);
            }
            return false;
        case PinType::Mat:
            return false;
        case PinType::Custom:
            return false;
        default:
            return false;
    }

    return false;
}

bool EditPinValue(Pin& pin)
{
    ImGui::ScopedItemWidth scopedItemWidth{120};

    auto pinValue = pin.GetValue();

    switch (pinValue.GetType())
    {
        case PinType::Void:
            return true;
        case PinType::Any:
            return true;
        case PinType::Flow:
            return true;
        case PinType::Bool:
            pin.SetValue(!pinValue.As<bool>());
            return true;
        case PinType::Int32:
            {
                auto value = pinValue.As<int32_t>();
                if (ImGui::InputInt("##editor", &value, 1, 50, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    pin.SetValue(value);
                    return true;
                }
            }
            return false;
        case PinType::Int64:
            {
                auto value = pinValue.As<int64_t>();
                if (ImGui::InputInt64("##editor", &value, 1, 50, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    pin.SetValue(value);
                    return true;
                }
            }
            return false;
        case PinType::Float:
            {
                auto value = pinValue.As<float>();
                if (ImGui::InputFloat("##editor", &value, 1, 50, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    pin.SetValue(value);
                    return true;
                }
            }
            return false;
        case PinType::Double:
            {
                auto value = pinValue.As<double>();
                if (ImGui::InputDouble("##editor", &value, 1, 50, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    pin.SetValue(value);
                    return true;
                }
            }
            return false;
        case PinType::String:
            {
                auto value = pinValue.As<string>();
                if (ImGui::InputText("##editor", (char*)value.data(), value.size() + 1, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackResize, [](ImGuiInputTextCallbackData* data) -> int
                {
                    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                    {
                        auto& stringValue = *static_cast<string*>(data->UserData);
                        ImVector<char>* my_str = (ImVector<char>*)data->UserData;
                        IM_ASSERT(stringValue.data() == data->Buf);
                        stringValue.resize(data->BufSize); // NB: On resizing calls, generally data->BufSize == data->BufTextLen + 1
                        data->Buf = (char*)stringValue.data();
                    }
                    return 0;
                }, &value))
                {
                    value.resize(strlen(value.c_str()));
                    pin.SetValue(value);
                    return true;
                }
            }
            return false;
        case PinType::Vec2:
            return true;
        case PinType::Vec4:
            return true;
        case PinType::Mat:
            return true;
        default:
            return true;
    }

    return true;
}

void DrawPinValueWithEditor(Pin& pin)
{
    auto storage = ImGui::GetStateStorage();
    auto activePinId = storage->GetInt(ImGui::GetID("PinValueEditor_ActivePinId"), false);

    if (activePinId == pin.m_ID)
    {
        if (EditPinValue(pin))
        {
            ed::EnableShortcuts(true);
            activePinId = 0;
        }
    }
    else
    {
        // Draw pin value
        PinValueBackgroundRenderer bg;
        if (!DrawPinValue(pin.GetValue()))
        {
            bg.Discard();
            return;
        }

        // Draw invisible button over pin value which triggers an editor if clicked
        auto itemMin = ImGui::GetItemRectMin();
        auto itemMax = ImGui::GetItemRectMax();
        auto itemSize = itemMax - itemMin;
        itemSize.x = ImMax(itemSize.x, 1.0f);
        itemSize.y = ImMax(itemSize.y, 1.0f);

        ImGui::SetCursorScreenPos(itemMin);

        if (ImGui::InvisibleButton("###pin_value_editor", itemSize))
        {
            activePinId = pin.m_ID;
            ed::EnableShortcuts(false);
        }
    }

    storage->SetInt(ImGui::GetID("PinValueEditor_ActivePinId"), activePinId);
}

PinValueBackgroundRenderer::PinValueBackgroundRenderer(const ImVec4 color, float alpha /*= 0.25f*/)
{
    m_DrawList = ImGui::GetWindowDrawList();
    m_Splitter.Split(m_DrawList, 2);
    m_Splitter.SetCurrentChannel(m_DrawList, 1);
    m_Color = color;
    m_Alpha = alpha;
}

PinValueBackgroundRenderer::PinValueBackgroundRenderer()
    : PinValueBackgroundRenderer(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark))
{
}

PinValueBackgroundRenderer::~PinValueBackgroundRenderer()
{
    Commit();
}

void PinValueBackgroundRenderer::Commit()
{
    if (m_Splitter._Current == 0)
        return;

    m_Splitter.SetCurrentChannel(m_DrawList, 0);

    auto itemMin = ImGui::GetItemRectMin();
    auto itemMax = ImGui::GetItemRectMax();
    auto margin = ImGui::GetStyle().ItemSpacing * 0.25f;
    margin.x = ImCeil(margin.x);
    margin.y = ImCeil(margin.y);
    itemMin -= margin;
    itemMax += margin;

    auto color = m_Color;
    color.w *= m_Alpha;
    m_DrawList->AddRectFilled(itemMin, itemMax,
        ImGui::GetColorU32(color), 4.0f);

    m_DrawList->AddRect(itemMin, itemMax,
        ImGui::GetColorU32(ImGuiCol_Text, m_Alpha), 4.0f);

    m_Splitter.Merge(m_DrawList);
}

void PinValueBackgroundRenderer::Discard()
{
    if (m_Splitter._Current == 1)
        m_Splitter.Merge(m_DrawList);
}

const vector<Node*> GetGroupedNodes(Node& node)
{
    vector<Node*> result;
    vector<ed::NodeId> nodes;
    ed::GetGroupedNodes(nodes, node.m_ID, ImVec2(40, 40));
    for (auto nodeId : nodes)
    {
        IM_ASSERT(node.m_Blueprint != nullptr);
        auto _node = node.m_Blueprint->FindNode(static_cast<ID_TYPE>(nodeId.Get()));
        //IM_ASSERT(_node != nullptr);
        if (_node && _node->GetStyle() != NodeStyle::Group)
        {
            if (_node->m_GroupID == 0)
                result.push_back(_node);
            else if (_node->m_GroupID == node.m_ID)
                result.push_back(_node);
        }
    }
    return result;
}

const vector<Node*> GetSelectedNodes(BP* blueprint)
{
    if (!blueprint) return {};
    auto selectedObjects = ed::GetSelectedNodes(nullptr, 0);

    vector<ed::NodeId> nodeIds;
    nodeIds.resize(selectedObjects);
    nodeIds.resize(ed::GetSelectedNodes(nodeIds.data(), selectedObjects));

    vector<Node*> result;
    result.reserve(nodeIds.size());
    for (auto nodeId : nodeIds)
    {
        auto node = blueprint->FindNode(static_cast<ID_TYPE>(nodeId.Get()));
        //IM_ASSERT(node != nullptr);
        if (node) result.push_back(node);
    }

    return result;
}

const vector<Pin*> GetSelectedLinks(BP* blueprint)
{
    if (!blueprint) return {};
    auto selectedObjects = ed::GetSelectedLinks(nullptr, 0);

    vector<ed::LinkId> linkIds;
    linkIds.resize(selectedObjects);
    linkIds.resize(ed::GetSelectedLinks(linkIds.data(), selectedObjects));

    vector<Pin*> result;
    result.reserve(linkIds.size());
    for (auto linkId : linkIds)
    {
        auto node = blueprint->FindPin(static_cast<ID_TYPE>(linkId.Get()));
        //IM_ASSERT(node != nullptr);
        if (node)
            result.push_back(node);
    }

    return result;
}

// -----------------------
// ----[ ItemBuilder ]----
// -----------------------
ItemBuilder::ItemBuilder()
    : m_InCreate(ed::BeginCreate(ImGui::GetStyleColorVec4(ImGuiCol_NavHighlight)))
{
}

ItemBuilder::~ItemBuilder()
{
    if(m_InCreate) ed::EndCreate();
}

ItemBuilder::operator bool() const
{
    return m_InCreate;
}

ItemBuilder::NodeBuilder* ItemBuilder::QueryNewNode()
{
    if (m_InCreate && ed::QueryNewNode(&m_NodeBuilder.m_PinId))
        return &m_NodeBuilder;
    else
        return nullptr;
}

ItemBuilder::LinkBuilder* ItemBuilder::QueryNewLink()
{
    if (m_InCreate && ed::QueryNewLink(&m_LinkBuilder.m_StartPinId, &m_LinkBuilder.m_EndPinId))
        return &m_LinkBuilder;
    else
        return nullptr;
}

bool ItemBuilder::LinkBuilder::Accept()
{
    return ed::AcceptNewItem(ImVec4(0.34f, 1.0f, 0.34f, 1.0f), 3.0f);
}

void ItemBuilder::LinkBuilder::Reject()
{
    ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
}

bool ItemBuilder::NodeBuilder::Accept()
{
    return ed::AcceptNewItem(ImVec4(0.34f, 1.0f, 0.34f, 1.0f), 3.0f);
}

void ItemBuilder::NodeBuilder::Reject()
{
    ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
}

// -----------------------
// ----[ ItemDeleter ]----
// -----------------------
ItemDeleter::ItemDeleter()
    : m_InDelete(ed::BeginDelete())
{
}

ItemDeleter::~ItemDeleter()
{
    ed::EndDelete();
}

ItemDeleter::operator bool() const
{
    return m_InDelete;
}

ItemDeleter::NodeDeleter* ItemDeleter::QueryDeletedNode()
{
    if (m_InDelete && ed::QueryDeletedNode(&m_NodeDeleter.m_NodeId))
        return &m_NodeDeleter;
    else
        return nullptr;
}

ItemDeleter::LinkDeleter* ItemDeleter::QueryDeleteLink()
{
    if (m_InDelete && ax::NodeEditor::QueryDeletedLink(&m_LinkDeleter.m_LinkId, &m_LinkDeleter.m_StartPinId, &m_LinkDeleter.m_EndPinId))
        return &m_LinkDeleter;
    else
        return nullptr;
}

bool ItemDeleter::LinkDeleter::Accept()
{
    return ed::AcceptDeletedItem();
}

void ItemDeleter::LinkDeleter::Reject()
{
    ed::RejectDeletedItem();
}

bool ItemDeleter::NodeDeleter::Accept(bool deleteLinks /*= true*/)
{
    return ed::AcceptDeletedItem(deleteLinks);
}

void ItemDeleter::NodeDeleter::Reject()
{
    ed::RejectDeletedItem();
}
} // namespace BluePrint