#include "ImNewCurve.h"

void ImGui::ImNewCurve::ShowDemo()
{
    ImGuiIO& io = ImGui::GetIO();
    static bool bUseTimebase = false;
    static int aTimebaseNumDen[] = {1, 25};
    static bool bCanZoomV = true;
    static bool move_curve = true;
    static bool keep_begin_end = false;
    static bool dock_begin_end = false;
    static ImGui::ImNewCurve::Editor::Holder s_hCurveEditor;

    float table_width = 300;
    auto size_x = ImGui::GetWindowSize().x - table_width - 60;

    if (!s_hCurveEditor)
    {
        s_hCurveEditor = ImGui::ImNewCurve::Editor::CreateInstance();
        s_hCurveEditor->SetBackgroundColor(IM_COL32(0, 0, 0, 255));
        s_hCurveEditor->SetBackgroundColor(IM_COL32(32, 32, 32, 128));
        auto hCurve = ImGui::ImNewCurve::Curve::CreateInstance("Curve1", ImGui::ImNewCurve::Smooth, {0,0,0,0}, {1,1,1,1}, {0,0,0,0});
        // auto hCurve = s_hCurveEditor->AddCurveByDim("key1", ImGui::ImNewCurve::Smooth, ImGui::ImNewCurve::DIM_X, -1, 1, 0, IM_COL32(255, 0, 0, 255), true);
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_X, ImVec2(0.00f, 0.00f));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_X, ImVec2(0.25f, 0.61f));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_X, ImVec2(0.50f, 1.00f));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_X, ImVec2(0.75f, 0.61f));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_X, ImVec2(1.00f, 0.00f));
        s_hCurveEditor->AddCurve(hCurve, ImGui::ImNewCurve::DIM_X, IM_COL32(255, 0, 0, 255));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_Y, ImVec2(0.00f, 1.00f));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_Y, ImVec2(0.25f, 0.39f));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_Y, ImVec2(0.50f, 0.00f));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_Y, ImVec2(0.75f, 0.39f));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_Y, ImVec2(1.00f, 1.00f));
        s_hCurveEditor->AddCurve(hCurve, ImGui::ImNewCurve::DIM_Y, IM_COL32(0, 255, 0, 255));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_Z, ImVec2(0.00f, 0.00f));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_Z, ImVec2(0.25f, 0.05f));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_Z, ImVec2(0.50f, 0.25f));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_Z, ImVec2(0.75f, 0.75f));
        hCurve->AddPointByDim(ImGui::ImNewCurve::DIM_Z, ImVec2(1.00f, 1.00f));
        s_hCurveEditor->AddCurve(hCurve, ImGui::ImNewCurve::DIM_Z, IM_COL32(0, 0, 255, 255));
        s_hCurveEditor->SetShowValueToolTip(true);
    }
    // const auto& aCurveTypeNames = ImGui::ImNewCurve::GetCurveTypeNames();
    bool reset = false;
    if (ImGui::Button("Reset##curve_reset"))
        reset = true;
    if (ImGui::Checkbox("Use Timebase", &bUseTimebase))
        s_hCurveEditor = nullptr;
    ImGui::SameLine(); ImGui::PushItemWidth(100);
    ImGui::BeginDisabled(!bUseTimebase);
    if (ImGui::InputInt2("##Timebase", aTimebaseNumDen))
    {
        if (aTimebaseNumDen[0] <= 0) aTimebaseNumDen[0] = 1;
        if (aTimebaseNumDen[1] <= 0) aTimebaseNumDen[1] = 1;
    }
    ImGui::EndDisabled(); ImGui::PopItemWidth();
    ImGui::SameLine(); ImGui::Checkbox("Zoom V", &bCanZoomV);
    ImGui::SameLine(); ImGui::Checkbox("Move Curve", &move_curve);
    // ImGui::SameLine(); ImGui::Checkbox("Keep Begin End", &keep_begin_end);
    // ImGui::SameLine(); ImGui::Checkbox("Dock Begin End", &dock_begin_end);
    uint32_t u32Flags = 0;
    if (bCanZoomV) u32Flags |= IMNEWCURVE_EDITOR_FLAG_ZOOM_V | IMNEWCURVE_EDITOR_FLAG_SCROLL_V;
    if (move_curve) u32Flags |= IMNEWCURVE_EDITOR_FLAG_MOVE_CURVE_V;
    if (s_hCurveEditor) s_hCurveEditor->DrawContent("##curve_editor_view", ImVec2(size_x, 300), 1.f, 0.f, u32Flags);
}
