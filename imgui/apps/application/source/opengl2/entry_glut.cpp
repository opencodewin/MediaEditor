#include "imgui.h"
#include "imgui_helper.h"
#include "imgui_impl_glut.h"
#include "imgui_impl_opengl2.h"
#ifdef __APPLE__
    #define GL_SILENCE_DEPRECATION
    #include <GLUT/glut.h>
#else
    #include <GL/freeglut.h>
#endif
#include <string>
#include "application.h"
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#endif

#ifdef _MSC_VER
#pragma warning (disable: 4505) // unreferenced local function has been removed
#endif

static ApplicationWindowProperty property;
static ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 1.f);
static bool done = false;
static bool app_done = false;

void glut_display_func()
{
    ImGui::ImUpdateTextures();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGLUT_NewFrame();
    ImGui::NewFrame();
    
    if (property.application.Application_Frame)
        app_done = property.application.Application_Frame(property.handle, done);
    else
        app_done = done;
    
    ImGui::EndFrame();

    if (app_done)
        exit(0);

    // Rendering
    ImGui::Render();

    glViewport(0, 0, (GLsizei)io.DisplaySize.x, (GLsizei)io.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    //glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context where shaders may be bound, but prefer using the GL3+ code.
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    glutSwapBuffers();
    glutPostRedisplay();
}

void Application_FullScreen(bool on)
{
    if (on)
        glutFullScreen();
    else
    {
        glutReshapeWindow(property.width, property.height);
        glutPositionWindow(property.pos_x, property.pos_y);
    }
}

int main(int argc, char** argv)
{
    // Create GLUT window
    glutInit(&argc, argv);
#ifdef __FREEGLUT_EXT_H__
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
#endif
    
    property.argc = argc;
    property.argv = argv;
    Application_Setup(property);
    
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(property.width, property.height);
    if (!property.center) glutInitWindowPosition(property.pos_x, property.pos_y);
    std::string title = property.name;
    title += " GLUT_GL2";
    glutCreateWindow(title.c_str());

    // Setup GLUT display function
    // We will also call ImGui_ImplGLUT_InstallFuncs() to get all the other functions installed for us,
    // otherwise it is possible to install our own functions and call the imgui_impl_glut.h functions ourselves.
    glutDisplayFunc(glut_display_func);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    auto ctx = ImGui::CreateContext();
    if (property.application.Application_SetupContext)
        property.application.Application_SetupContext(ctx, property.handle, false);
    
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGuiContext& g = *GImGui;
    io.ApplicationName = property.name.c_str();
    io.Fonts->AddFontDefault(property.font_scale);
    io.FontGlobalScale = 1.0f / property.font_scale;
    if (property.power_save) io.ConfigFlags |= ImGuiConfigFlags_EnablePowerSavingMode;
    if (property.low_reflash) io.ConfigFlags |= ImGuiConfigFlags_EnableLowRefreshMode;
    ImGui::SetCustomFrameRate(property.max_fps, property.min_fps);
    if (property.navigator)
    {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    }
    if (property.docking) io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    if (property.viewport)io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    if (!property.auto_merge) io.ConfigViewportsNoAutoMerge = true;
    // Setup App setting file path
    auto setting_path = property.using_setting_path ? ImGuiHelper::settings_path(property.name) : "";
    auto ini_name = property.name;
    std::replace(ini_name.begin(), ini_name.end(), ' ', '_');
    setting_path += ini_name + ".ini";
    io.IniFilename = setting_path.c_str();
    if (property.internationalize && !property.language_path.empty())
    {
        io.LanguagePath = property.language_path.c_str();
        g.Style.TextInternationalize = 1;
        g.LanguageName = "Default";
    }

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGLUT_Init();
    ImGui_ImplGLUT_InstallFuncs();
    ImGui_ImplOpenGL2_Init();

#if IMGUI_VULKAN_SHADER
    ImGui::ImVulkanShaderInit();
#endif

    // init application
    if (property.application.Application_Initialize)
        property.application.Application_Initialize(&property.handle);

    glutMainLoop();

    if (property.application.Application_Finalize)
        property.application.Application_Finalize(&property.handle);

    // Cleanup
#if IMGUI_VULKAN_SHADER
    ImGui::ImVulkanShaderClear();
#endif
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGLUT_Shutdown();
    ImGui::ImDestroyTextures();
    ImGui::DestroyContext();

    return 0;
}
