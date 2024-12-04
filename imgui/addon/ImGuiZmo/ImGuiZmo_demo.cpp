#include "ImGuiZmo.h"
#include <imgui_helper.h>
#include <imgui_extra_widget.h>
#include <ImGuiFileDialog.h>
#include <string>
#include <vector>
#include <algorithm>

namespace IMGUIZMO_NAMESPACE
{

void ShowImGuiZmoDemo()
{
    auto& io = ImGui::GetIO();
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 Canvas_size = ImGui::GetWindowSize();
    // background ImGuizmo
    static float cameraView[16] =
    {   1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };
    static const float identityMatrix[16] =
    {   1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };
    static float cubeMatrix[16] =
    {   1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };
    static bool inited = false;
    static float cameraProjection[16];
    static bool bFace = true;
    static bool bMesh = false;
    static bool bNormal = false;
    static float fov = 30.f;
    static float camYAngle = ImDegToRad(165.f);
    static float camXAngle = ImDegToRad(15.f);
    static float camDistance = 8.f;
    static float camShiftX = 0.0f;
    static float camShiftY = 0.0f;
    static std::vector<Model*> models;

    bool bControlHoverd = false;
    bool bAnyPopup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
    ImGuizmo::Perspective(fov, Canvas_size.x / Canvas_size.y, 0.1f, 100.f, cameraProjection, camShiftX, camShiftY);
    ImGuizmo::SetOrthographic(false);

    auto cameraViewUpdate = [&]()
    {
        ImGuizmo::Perspective(fov, Canvas_size.x / Canvas_size.y, 0.1f, 100.f, cameraProjection, camShiftX, camShiftY);
        float eye[] = { cosf(camYAngle) * cosf(camXAngle) * camDistance, sinf(camXAngle) * camDistance, sinf(camYAngle) * cosf(camXAngle) * camDistance };
        float at[] = { 0.f, 0.f, 0.f };
        float up[] = { 0.f, 1.f, 0.f };
        ImGuizmo::LookAt(eye, at, up, cameraView);
        for (auto model : models)
            ImGuizmo::UpdateModel(cameraView, cameraProjection, model);
    };

    if (!inited)
    {
        cameraViewUpdate();
        inited = true;
    }

    // draw zmo
    ImGui::PushStyleColor(ImGuiCol_WindowBg, (ImVec4)ImColor(0.35f, 0.3f, 0.3f));
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, Canvas_size.x, Canvas_size.y);
    ImGuizmo::DrawGrid(cameraView, cameraProjection, identityMatrix, 100.f);
    
    // draw models
    for (auto model : models)
    {
        // Draw Bodel
        ImGuizmo::DrawModel(cameraView, cameraProjection, model, bFace, bMesh, bNormal);

        if (model->showManipulate)
        {
            bool updated = ImGuizmo::Manipulate(cameraView, cameraProjection, model->currentGizmoOperation, model->currentGizmoMode, (float *)&model->identity_matrix, NULL, 
                                                model->useSnap ? (float *)&model->snap : NULL, 
                                                model->boundSizing ? (float *)&model->bounds : NULL, 
                                                model->boundSizingSnap ? (float *)&model->boundsSnap : NULL);
            if (updated)
            {
                ImGuizmo::UpdateModel(cameraView, cameraProjection, model);
            }
            bControlHoverd |= updated;
        }
    }

    // draw view Manipulate
    float viewManipulateRight = ImGui::GetWindowPos().x + Canvas_size.x;
    float viewManipulateTop = ImGui::GetWindowPos().y;
    if (ImGuizmo::ViewManipulate(cameraView, camDistance, ImVec2(viewManipulateRight - 256, viewManipulateTop + 64), ImVec2(128, 128), 0x10101010, &camXAngle, &camYAngle))
    {
        cameraViewUpdate();
        bControlHoverd = true;
    }
    ImGui::PopStyleColor();

    // draw control panel
    ImGuiWindowFlags control_window_flags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoDecoration;
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        const ImGuiViewport* _viewport = ImGui::GetWindowViewport();
        control_window_flags |= ImGuiWindowFlags_NoDocking;
        io.ConfigViewportsNoDecoration = true;
        ImGui::SetNextWindowViewport(_viewport->ID);
    }
    ImGui::SetNextWindowSize(ImVec2(400, 800), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5, 0.5, 0.5, 1.0));
    if (ImGui::Begin("Model Control", nullptr, control_window_flags))
    {
        bControlHoverd |= ImGui::IsWindowFocused();
        if (ImGui::Button(ICON_FK_REFRESH " Reset View"))
        {
            camYAngle = ImDegToRad(165.f);
            camXAngle = ImDegToRad(15.f);
            camDistance = 8.f;
            camShiftX = 0.0f;
            camShiftY = 0.0f;
            cameraViewUpdate();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Face", &bFace);
        ImGui::SameLine();
        ImGui::Checkbox("Mesh", &bMesh);
        ImGui::SameLine();
        ImGui::Checkbox("Normal", &bNormal);
        if (ImGui::Button(ICON_FK_PLUS " Add model"))
        {
            IGFD::FileDialogConfig config;
            config.path = ".";
            config.countSelectionMax = 1;
            config.userDatas = IGFDUserDatas("Open Model");
            config.flags = ImGuiFileDialogFlags_OpenFile_Default;
            ImGuiFileDialog::Instance()->OpenDialog("##OpenFileDlgKey", ICON_IGFD_FOLDER_OPEN " Choose OBJ File", 
                                                    "3D Object file(*.obj){.obj}",
                                                    config);
        }

        for (auto iter = models.begin(); iter != models.end();)
        {
            Model * model = *iter;
            std::string model_label = "Model #" + ImGuiHelper::MillisecToString(model->model_id, 1);
            auto model_label_id = ImGui::GetID(model_label.c_str());
            ImGui::PushID(model_label_id);
            if (ImGui::TreeNodeEx(model_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowOverlap))
            {
                if (ImGui::RadioButton("Translate", model->currentGizmoOperation == ImGuizmo::TRANSLATE))
                    model->currentGizmoOperation = ImGuizmo::TRANSLATE;
                ImGui::SameLine();
                if (ImGui::RadioButton("Rotate", model->currentGizmoOperation == ImGuizmo::ROTATE))
                    model->currentGizmoOperation = ImGuizmo::ROTATE;
                ImGui::SameLine();
                if (ImGui::RadioButton("Scale", model->currentGizmoOperation == ImGuizmo::SCALE))
                    model->currentGizmoOperation = ImGuizmo::SCALE;
                ImGui::SameLine();
                if (ImGui::RadioButton("Universal", model->currentGizmoOperation == ImGuizmo::UNIVERSAL))
                    model->currentGizmoOperation = ImGuizmo::UNIVERSAL;
                ImGuizmo::DecomposeMatrixToComponents(model->identity_matrix, model->matrixTranslation, model->matrixRotation, model->matrixScale);
                ImGui::PushItemWidth(240);
                ImGui::InputFloat3("Tr", (float*)&model->matrixTranslation);
                ImGui::InputFloat3("Rt", (float*)&model->matrixRotation);
                ImGui::InputFloat3("Sc", (float*)&model->matrixScale);
                ImGuizmo::RecomposeMatrixFromComponents(model->matrixTranslation, model->matrixRotation, model->matrixScale, model->identity_matrix);
                ImGui::PopItemWidth();
                if (model->currentGizmoOperation != ImGuizmo::SCALE)
                {
                    if (ImGui::RadioButton("Local", model->currentGizmoMode == ImGuizmo::LOCAL))
                        model->currentGizmoMode = ImGuizmo::LOCAL;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("World",model->currentGizmoMode == ImGuizmo::WORLD))
                        model->currentGizmoMode = ImGuizmo::WORLD;
                }
                ImGui::Separator();
                ImGui::Checkbox("Manipulate", &model->showManipulate);
                ImGui::SameLine();
                if (ImGui::Button(ICON_FK_HOME " Reset"))
                {
                    ImGuizmo::DecomposeMatrixToComponents(identityMatrix, model->matrixTranslation, model->matrixRotation, model->matrixScale);
                    model->matrixScale *= 0.2f;
                    ImGuizmo::RecomposeMatrixFromComponents(model->matrixTranslation, model->matrixRotation, model->matrixScale, model->identity_matrix);
                    ImGuizmo::UpdateModel(cameraView, cameraProjection, model);
                }
                ImGui::SameLine();
                if (ImGui::Button(ICON_FK_TRASH_O " Delete"))
                {
                    delete model;
                    iter = models.erase(iter);
                }
                else
                    iter++;
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        ImVec2 minSize = ImVec2(600, 600);
        ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            const ImGuiViewport* _viewport = ImGui::GetWindowViewport();
            ImGui::SetNextWindowViewport(_viewport->ID);
        }
        if (ImGuiFileDialog::Instance()->Display("##OpenFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
        {
            if (ImGuiFileDialog::Instance()->IsOk())
            {
                auto file_path = ImGuiFileDialog::Instance()->GetFilePathName();
                auto file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
                auto userDatas = std::string((const char*)ImGuiFileDialog::Instance()->GetUserDatas());
                if (userDatas.compare("Open Model") == 0)
                {
                    Model* model = new Model();
                    model->model_data = LoadObj(file_path);
                    if (model->model_data->model_open)
                    {
                        model->model_id = ImGui::get_current_time_usec();
                        ImGuizmo::DecomposeMatrixToComponents(model->identity_matrix, model->matrixTranslation, model->matrixRotation, model->matrixScale);
                        model->matrixScale *= 0.2f;
                        ImGuizmo::RecomposeMatrixFromComponents(model->matrixTranslation, model->matrixRotation, model->matrixScale, model->identity_matrix);
                        ImGuizmo::UpdateModel(cameraView, cameraProjection, model);
                        models.emplace_back(model);
                    }
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(3);

    // handle mouse wheel
    if (!bControlHoverd && !bAnyPopup)
    {
        if (io.MouseWheelH < -FLT_EPSILON)
        {
            camYAngle += ImDegToRad(2.f);
            cameraViewUpdate();
        }
        else if (io.MouseWheelH > FLT_EPSILON)
        {
            camYAngle -= ImDegToRad(2.f);
            cameraViewUpdate();
        }
        else if (io.MouseWheel < -FLT_EPSILON)
        {
            camDistance *= 0.95;
            if (camDistance < 1.f) camDistance = 1.f;
            cameraViewUpdate();
        }
        else if (io.MouseWheel > FLT_EPSILON)
        {
            camDistance *= 1.05;
            if (camDistance > 10.f) camDistance = 10.f;
            cameraViewUpdate();
        }
        
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            camShiftX -= io.MouseDelta.x / Canvas_size.x / camDistance;
            camShiftY += io.MouseDelta.y / Canvas_size.y / camDistance;
            cameraViewUpdate();
        }
    }
}
} // namespace IMGUIZMO_NAMESPACE