#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include <application.h>
#include <immat.h>
#include <mesh.h>
#include <GL/glew.h>
#include "Config.h"

static gl::Mesh * mesh = nullptr;

static void show_main_view()
{
    auto& io = ImGui::GetIO();
    const ImVec2 Canvas_size = ImGui::GetWindowSize();
    static float camYAngle = ImDegToRad(90.f);
    static float camXAngle = ImDegToRad(0.f);
    static float ambient[4] = {0.05, 0.05, 0.05, 1};
    static float diffusion[4] = {0.2, 0.2, 0.2, 1};
    static float specular[4] = {0.5, 0.5, 0.5, 1};
    static float light_positions[20] = {10.0f, 10.0f, 10.0f};
    static float light_colors[20] = {1.0f, 1.0f, 1.0f};
    static float shininess = 1; 
    static float scale = 1.0f;
    static bool custom_color = false;
    bool bControlHoverd = false;
    bool bAnyPopup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);

    auto cameraViewUpdate = [&]()
    {
        if (mesh) mesh->update();
    };

    if (mesh)
    {
        mesh->display(*ambient, *diffusion, *specular, shininess, custom_color, *light_positions, *light_colors, scale, camXAngle, camYAngle);
    }

    // draw control panel
    ImGuiWindowFlags control_window_flags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoDecoration;
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        const ImGuiViewport* _viewport = ImGui::GetWindowViewport();
        control_window_flags |= ImGuiWindowFlags_NoDocking;
        io.ConfigViewportsNoDecoration = true;
        ImGui::SetNextWindowViewport(_viewport->ID);
    }
    ImGui::SetNextWindowSize(ImVec2(500, 800), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5, 0.5, 0.5, 1.0));
    if (ImGui::Begin("Model Control", nullptr, control_window_flags))
    {
        bControlHoverd |= ImGui::IsWindowFocused();
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

        if (mesh)
        {
            ImGui::TextColored({0.0f,1.0f,1.0f,1.0f}, "Render Mode"); 
            ImGui::RadioButton("Smooth", &mesh->render_mode, 0); ImGui::SameLine();
            ImGui::RadioButton("Lines", &mesh->render_mode, 1); ImGui::SameLine();
            ImGui::RadioButton("Point Cloud", &mesh->render_mode, 2); ImGui::SameLine();
            ImGui::RadioButton("Mesh", &mesh->render_mode, 3); 
            ImGui::Text(" ");
            ImGui::SliderFloat3("Camera position ", &mesh->eye[0], -10.0f, 10.0f);
            ImGui::Separator(); 
            ImGui::TextColored({0.0f,1.0f,1.0f,1.0f}, "Camera"); ImGui::Separator(); 
            if (ImGui::SliderFloat("Scale", &scale, 0.01f, 2.0f)) cameraViewUpdate();
            if (ImGui::SliderFloat("Field of view", &mesh->fovy, 0.0f, 180.0f)) cameraViewUpdate();
            if (ImGui::SliderFloat("Frustum near", &mesh->zNear, -15.0f, 15.0f)) cameraViewUpdate();
            if (ImGui::SliderFloat("Frustum far", &mesh->zFar, -150.0f, 150.0f)) cameraViewUpdate();
            if (ImGui::Button(ICON_FK_REFRESH " Reset View"))
            {
                camYAngle = ImDegToRad(90.f);
                camXAngle = ImDegToRad(0.f);
                mesh->amount = 5.f;
                mesh->tx = 0.0f;
                mesh->ty = 0.0f;
                scale = 1.f;
                cameraViewUpdate();
            }
            ImGui::Separator(); 
            ImGui::Checkbox("Custom Colors", &custom_color); 
            if (custom_color)
            {
                ImGui::SliderFloat3("Ambient R, G, B", &ambient[0], 0.0f, 1.0f);
                ImGui::SliderFloat3("Diffusion R, G, B", &diffusion[0], 0.0f, 1.0f);
                ImGui::SliderFloat3("Specular R, G, B", &specular[0], 0.0f, 1.0f);
                ImGui::SliderFloat("Shininess", &shininess, 0.0f, 10.0f);
            }
            ImGui::Separator(); 
            ImGui::TextColored({0.0f,1.0f,1.0f,1.0f}, "Lighting"); 
            for (int i = 0; i < MAX_LIGHTS; i++)
            {
                std::string light_name = std::string("Light ") + std::to_string(i + 1); 
                ImGui::Separator(); 
                ImGui::Text("%s", light_name.c_str()); 
                ImGui::Separator(); 
                ImGui::SliderFloat3((light_name + " positions").c_str(), &light_positions[4 * i], -100.0f, 100.0f);
                ImGui::SliderFloat3((light_name + " colors").c_str(), &light_colors[4 * i], 0.0f, 1.0f);
                light_positions[(4 * i) + 3] = 1.0f;
                light_colors[(4 * i) + 3] = 1.0f;
            }
        }

        // handle file dialog
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            const ImGuiViewport* _viewport = ImGui::GetWindowViewport();
            ImGui::SetNextWindowViewport(_viewport->ID);
        }
        if (ImGuiFileDialog::Instance()->Display("##OpenFileDlgKey", ImGuiWindowFlags_NoCollapse, ImVec2(600, 600), ImVec2(FLT_MAX, FLT_MAX)))
        {
            if (ImGuiFileDialog::Instance()->IsOk())
            {
                auto file_path = ImGuiFileDialog::Instance()->GetFilePathName();
                auto file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
                auto userDatas = std::string((const char*)ImGuiFileDialog::Instance()->GetUserDatas());
                if (userDatas.compare("Open Model") == 0)
                {
                    if (mesh) { delete mesh; mesh = nullptr; }
                    mesh = new gl::Mesh(file_path);
                    mesh->set_view_size(Canvas_size.x, Canvas_size.y);
                    cameraViewUpdate();
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(3);

    // handle mouse wheel
    if (mesh && !bControlHoverd && !bAnyPopup)
    {
        if (io.MouseWheel < -FLT_EPSILON)
        {
            mesh->ty -= 0.25;
            cameraViewUpdate();
        }
        else if (io.MouseWheel > FLT_EPSILON)
        {
            mesh->ty += 0.25;
            cameraViewUpdate();
        }
        else if (io.MouseWheelH < -FLT_EPSILON)
        {
            mesh->tx -= 0.25;
            cameraViewUpdate();
        }
        else if (io.MouseWheelH > FLT_EPSILON)
        {
            mesh->tx += 0.25;
            cameraViewUpdate();
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            camYAngle += io.MouseDelta.x / Canvas_size.x / mesh->amount * 20;
            camXAngle += io.MouseDelta.y / Canvas_size.y / mesh->amount * 20;
            if (ImGui::BeginTooltip())
            {
                ImGui::Text("CamX Angle:%0.3f", ImRadToDeg(camYAngle));
                ImGui::Text("CamY Angle:%0.3f", ImRadToDeg(camXAngle));
                ImGui::EndTooltip();
            }
            cameraViewUpdate();
        }
    }
}

bool Frame(void* handle, bool app_will_quit)
{
    auto& io = ImGui::GetIO();
    bool app_done = false;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    ImGuiCond cond = ImGuiCond_Once;
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        io.ConfigViewportsNoDecoration = false;
        flags = ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoScrollbar | 
                ImGuiWindowFlags_NoScrollWithMouse | 
                ImGuiWindowFlags_NoSavedSettings | 
                ImGuiWindowFlags_NoBringToFrontOnFocus | 
                ImGuiWindowFlags_NoDocking;
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
    }
    else
    {
        flags = ImGuiWindowFlags_NoTitleBar | 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoScrollbar | 
                ImGuiWindowFlags_NoScrollWithMouse | 
                ImGuiWindowFlags_NoSavedSettings | 
                ImGuiWindowFlags_NoBringToFrontOnFocus;
        cond = ImGuiCond_None;
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize, cond);
    }

    ImGui::Begin("Content", nullptr, flags);
        show_main_view();
    ImGui::End();
    io.ConfigViewportsNoDecoration = true;
    if (app_will_quit)
        app_done = true;
    return app_done;
}

void Initialize(void** handle)
{
}

void Finalize(void** handle)
{
    if (mesh) { delete mesh; }
}

static void SetupContext(ImGuiContext* ctx, void* handle, bool in_splash)
{
    if (!ctx)
        return;
#ifdef USE_PLACES_FEATURE
    ImGuiSettingsHandler bookmark_ini_handler;
    bookmark_ini_handler.TypeName = "BookMark";
    bookmark_ini_handler.TypeHash = ImHashStr("BookMark");
    bookmark_ini_handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) -> void*
    {
        return ImGuiFileDialog::Instance();
    };
    bookmark_ini_handler.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) -> void
    {
        IGFD::FileDialog * dialog = (IGFD::FileDialog *)entry;
        if (dialog) dialog->DeserializePlaces(line);
    };
    bookmark_ini_handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
    {
        ImGuiContext& g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        auto bookmark = ImGuiFileDialog::Instance()->SerializePlaces();
        out_buf->appendf("[%s][##%s]\n", handler->TypeName, handler->TypeName);
        out_buf->appendf("%s\n", bookmark.c_str());
        out_buf->append("\n");
    };
    ctx->SettingsHandlers.push_back(bookmark_ini_handler);
#endif
}

void Application_Setup(ApplicationWindowProperty& property)
{
    property.name = "ObjViewer";
    property.font_scale = 2.0f;
    property.splash_screen_width = 1920;
    property.splash_screen_height = 1080;
    property.application.Application_SetupContext = SetupContext;
    property.application.Application_Initialize = Initialize;
    property.application.Application_Finalize = Finalize;
    property.application.Application_Frame = Frame;
}
