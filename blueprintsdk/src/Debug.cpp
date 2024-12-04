#include <Debug.h>
#include <Icon.h>
#include <Utils.h>
#include <UI.h>
#include <imgui_node_editor.h>
#include <imgui_canvas.h>
#include <sstream>
#include <iomanip>

namespace ed = ax::NodeEditor;
DECLARE_HAS_MEMBER(HasVtxCurrentOffset, _VtxCurrentOffset);

namespace BluePrint
{
struct VtxCurrentOffsetRef
{
    // Overload is present when ImDrawList does have _FringeScale member variable.
    template <typename T>
    static unsigned int& Get(typename std::enable_if<HasVtxCurrentOffset<T>::value, T>::type* drawList)
    {
        return drawList->_VtxCurrentOffset;
    }

    // Overload is present when ImDrawList does not have _FringeScale member variable.
    template <typename T>
    static unsigned int& Get(typename std::enable_if<!HasVtxCurrentOffset<T>::value, T>::type* drawList)
    {
        return drawList->_CmdHeader.VtxOffset;
    }
};

static inline unsigned int& ImVtxOffsetRef(ImDrawList* drawList)
{
    return VtxCurrentOffsetRef::Get<ImDrawList>(drawList);
}

DebugOverlay::DebugOverlay(BP* blueprint)
    : m_Blueprint(blueprint)
{
    if (m_Blueprint) m_Blueprint->SetContextMonitor(this);
}

DebugOverlay::~DebugOverlay()
{
    // m_Blueprint may already released, so we can't using dirty point here,
    // we may need set m_Blueprint to nullptr before we release DebugOverlay
    //if (m_Blueprint) m_Blueprint->SetContextMonitor(nullptr);
}

void DebugOverlay::Init(BP* blueprint)
{
    if (m_Blueprint) m_Blueprint->SetContextMonitor(nullptr);
    m_Blueprint = blueprint;
    if (m_Blueprint) m_Blueprint->SetContextMonitor(this);
}

void DebugOverlay::Begin()
{
    if (!m_Blueprint)
        return;

    m_CurrentNode = m_Blueprint->CurrentNode();
    if (nullptr == m_CurrentNode)
        return;

    m_NextNode = m_Blueprint->NextNode();
    m_CurrentFlowPin = m_Blueprint->CurrentFlowPin();

    // Use splitter to draw pin values on top of nodes
    m_DrawList = ImGui::GetWindowDrawList();
    m_Splitter.Split(m_DrawList, 2);
}

void DebugOverlay::End()
{
    if (!m_Blueprint)
        return;

    if (nullptr == m_CurrentNode)
        return;

    m_Splitter.Merge(m_DrawList);
}

ContextMonitor* DebugOverlay::GetContextMonitor()
{
    return this;
}

void DebugOverlay::DrawNode(BluePrintUI* ui, const Node& node)
{
    if (!m_Enable || nullptr == m_CurrentNode)
        return;

    auto isCurrentNode = (m_CurrentNode->m_ID == node.m_ID);
    auto isNextNode = m_NextNode ? (m_NextNode->m_ID == node.m_ID) : false;
    auto isBreakPoint = m_CurrentNode->m_BreakPoint;
    if (!isCurrentNode && !isNextNode)
        return;

    // Draw to layer over nodes
    m_Splitter.SetCurrentChannel(m_DrawList, 1);

    // Save current layout to avoid conflicts with node measurements
    ImGui::ScopedSuspendLayout suspendLayout;

    // Put cursor on the left side of the Pin
    auto itemRectMin = ImGui::GetItemRectMin();
    auto itemRectMax = ImGui::GetItemRectMax();

    // itemRectMin -= ImVec2(4.0f, 4.0f);
    // itemRectMax += ImVec2(4.0f, 4.0f);
    auto pivot = ImVec2((itemRectMin.x + itemRectMax.x) * 0.5f, itemRectMin.y);
    auto markerMin = pivot - ImVec2(40.0f, 35.0f);
    auto markerMax = pivot + ImVec2(40.0f, 0.0f);

    //auto color = ImGui::GetStyleColorVec4(isCurrentNode ? ImGuiCol_PlotHistogram : ImGuiCol_NavHighlight);
    auto color = isCurrentNode ? (isBreakPoint ? ui->m_StyleColors[BluePrintStyleColor_DebugBreakPointNode] :
                                                ui->m_StyleColors[BluePrintStyleColor_DebugCurrentNode]) : 
                                                ui->m_StyleColors[BluePrintStyleColor_DebugNextNode];

    DrawIcon(m_DrawList, markerMin, markerMax, IconType::FlowDown, true, ImColor(color), 0);

    // Switch back to normal layer
    m_Splitter.SetCurrentChannel(m_DrawList, 0);
}

void DebugOverlay::DrawInputPin(BluePrintUI* ui, const Pin& pin)
{
    if (!m_Enable || !m_Blueprint)
        return;
    if (nullptr == m_CurrentNode)
        return;

    auto flowPinValue = m_Blueprint->GetContext().GetPinValue(m_CurrentFlowPin);
    auto flowPin = flowPinValue.GetType() == PinType::Flow ? flowPinValue.As<FlowPin*>() : nullptr;

    const auto isCurrentFlowPin = flowPin && flowPin->m_ID== pin.m_ID;

    if (!pin.m_Link && !isCurrentFlowPin)
        return;

    // Draw to layer over nodes
    m_Splitter.SetCurrentChannel(m_DrawList, 1);

    // Save current layout to avoid conflicts with node measurements
    ImGui::ScopedSuspendLayout suspendLayout;

    // Put cursor on the left side of the Pin
    auto itemRectMin = ImGui::GetItemRectMin();
    auto itemRectMax = ImGui::GetItemRectMax();
    ImGui::SetCursorScreenPos(ImVec2(itemRectMin.x, itemRectMin.y)
        - ImVec2(1.75f * ImGui::GetStyle().ItemSpacing.x, 0.0f));

    // Remember current vertex, later we will patch generated mesh so drawn value
    // will be aligned to the right side of the pin.
    auto vertexStartIdx = ImVtxOffsetRef(m_DrawList) + m_DrawList->_VtxCurrentIdx;//m_DrawList->_VtxCurrentOffset + m_DrawList->_VtxCurrentIdx;

    auto isCurrentNode = (m_CurrentNode == pin.m_Node);
    //auto color = ImGui::GetStyleColorVec4(isCurrentNode ? ImGuiCol_PlotHistogram : ImGuiCol_NavHighlight);
    auto color = isCurrentNode ? ui->m_StyleColors[BluePrintStyleColor_DebugCurrentNode] : 
                                ui->m_StyleColors[BluePrintStyleColor_DebugNextNode];

    // Actual drawing
    if (!pin.IsMappedPin() && pin.m_Type != PinType::Mat && pin.m_Type != PinType::Array)
    {
        PinValueBackgroundRenderer bg(color, 0.5f);
        if (!DrawPinValue(m_Blueprint->GetContext().GetPinValue(pin)))
        {
            if (isCurrentFlowPin)
            {
                auto iconSize = ImVec2(ImGui::GetFontSize(), ImGui::GetFontSize());
                Icon(iconSize, IconType::Flow, true, false, false, color);
            }
            else
                bg.Discard();
        }
        bg.Commit();
    }

    // Move generated mesh to the left
    auto itemWidth = ImGui::GetItemRectSize().x;
    auto vertexEndIdx = ImVtxOffsetRef(m_DrawList) + m_DrawList->_VtxCurrentIdx;//m_DrawList->_VtxCurrentOffset + m_DrawList->_VtxCurrentIdx;
    for (auto vertexIdx = vertexStartIdx; vertexIdx < vertexEndIdx; ++vertexIdx)
        m_DrawList->VtxBuffer[vertexIdx].pos.x -= itemWidth;

    // Switch back to normal layer
    m_Splitter.SetCurrentChannel(m_DrawList, 0);
}

void DebugOverlay::DrawOutputPin(BluePrintUI* ui, const Pin& pin)
{
    if (!m_Enable || nullptr == m_CurrentNode)
        return;

    auto current_pos = ImGui::GetCursorScreenPos();
    // Draw to layer over nodes
    m_Splitter.SetCurrentChannel(m_DrawList, 1);

    ImGui::ScopedSuspendLayout suspendLayout;

    // Put cursor on the right side of the Pin
    auto itemRectMin = ImGui::GetItemRectMin();
    auto itemRectMax = ImGui::GetItemRectMax();
    ImGui::SetCursorScreenPos(ImVec2(itemRectMax.x, itemRectMin.y)
        + ImVec2(1.75f * ImGui::GetStyle().ItemSpacing.x, 0.0f));

    auto isCurrentNode = m_CurrentNode == pin.m_Node;
    //auto color = ImGui::GetStyleColorVec4(isCurrentNode ? ImGuiCol_PlotHistogram : ImGuiCol_NavHighlight);
    auto color = isCurrentNode ? ui->m_StyleColors[BluePrintStyleColor_DebugCurrentNode] :
                                ui->m_StyleColors[BluePrintStyleColor_DebugNextNode];
    
    // Actual drawing
    if (!pin.IsMappedPin() && pin.m_Type != PinType::Mat && pin.m_Type != PinType::Array)
    {
        PinValueBackgroundRenderer bg(color, 0.5f);
        if (!DrawPinValue(m_Blueprint->GetContext().GetPinValue(pin)))
        {
            if (m_CurrentFlowPin.m_ID == pin.m_ID)
            {
                auto iconSize = ImVec2(ImGui::GetFontSize(), ImGui::GetFontSize());
                Icon(iconSize, IconType::Flow, true, false, false, color);
            }
            else
                bg.Discard();
        }
        bg.Commit();
    }

    // Switch back to normal layer
    m_Splitter.SetCurrentChannel(m_DrawList, 0);

    ImGui::SetCursorScreenPos(current_pos);
}

void DebugOverlay::OnDone(Context& context)
{
    if (!m_Blueprint)
        return;
    m_Blueprint->OnContextRunDone();
}

void DebugOverlay::OnPause(Context& context)
{
    if (!m_Blueprint)
        return;
    m_Blueprint->OnContextPause();
}

void DebugOverlay::OnResume(Context& context)
{
    if (!m_Blueprint)
        return;
    m_Blueprint->OnContextResume();
}

void DebugOverlay::OnStepNext(Context& context)
{
    if (!m_Blueprint)
        return;
    m_Blueprint->OnContextStepNext();
}

void DebugOverlay::OnStepCurrent(Context& context)
{
    if (!m_Blueprint)
        return;
    m_Blueprint->OnContextStepCurrent();
}

} // namespace BluePrint