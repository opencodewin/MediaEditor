#include "imgui.h"
#include "imgui_helper.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cerrno>
#include "application.h"
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#endif
#if IMGUI_GLEW
#include <GL/glew.h>
#endif
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
// Include glfw3.h after our OpenGL definitions
#include <GLFW/glfw3.h>

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

#define GLFW_VERSION_COMBINED           (GLFW_VERSION_MAJOR * 1000 + GLFW_VERSION_MINOR * 100 + GLFW_VERSION_REVISION)
#define GLFW_HAS_MONITOR_WORK_AREA      (GLFW_VERSION_COMBINED >= 3300) // 3.3+ glfwGetMonitorWorkarea

static ApplicationWindowProperty property;

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error %d: %s\n", error, description);
}

static void DropCallback(GLFWwindow*, int count, const char** paths)
{
    std::vector<std::string> file_paths;
    for (int i = 0; i < count; i++)
    {
        file_paths.push_back(paths[i]);
    }
    if (!file_paths.empty() && property.application.Application_DropFromSystem)
    {
        property.application.Application_DropFromSystem(file_paths);
    }
}

void Application_FullScreen(bool on)
{
    ImGui_ImplGlfw_FullScreen(ImGui::GetMainViewport(), on);
}

static bool Show_Splash_Window(ApplicationWindowProperty& property, ImGuiContext* ctx)
{
    std::string title = property.name + " Splash";
    glfwWindowHint(GLFW_RESIZABLE, false);
    glfwWindowHint(GLFW_FLOATING, true);
    glfwWindowHint(GLFW_DECORATED, false);
    GLFWwindow* window = glfwCreateWindow(property.splash_screen_width, property.splash_screen_height, title.c_str(), NULL, NULL);
    if (!window)
    {
        printf("GLFW: Create Splash window Error!!!\n");
        return false;
    }

    // Set window icon
    if (!property.icon_path.empty())
    {
        ImGui_ImplGlfw_SetWindowIcon(window, property.icon_path.c_str());
    }
    
    // Set window alpha
    glfwSetWindowOpacity(window, property.splash_screen_alpha);

    glfwMakeContextCurrent(window);
    glfwSetDropCallback(window, DropCallback);
    glfwSwapInterval(1); // Enable vsync

    GLFWmonitor* pMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode * mode = glfwGetVideoMode(pMonitor);
    glfwSetWindowPos(window, (mode->width - property.splash_screen_width) / 2, (mode->height - property.splash_screen_height) / 2);

    // Setup ImGui binding
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 1.f);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    if (property.application.Application_SetupContext)
        property.application.Application_SetupContext(ctx, property.handle, true);

    // Main loop
    static int frame_count = 0;
    bool done = false;
    bool splash_done = false;
    while (!splash_done)
    {
        ImGui::ImUpdateTextures();
        ImGui_ImplGlfw_WaitForEvent();
        glfwPollEvents();
        if (glfwWindowShouldClose(window))
            done = true;

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        auto _splash_done = property.application.Application_SplashScreen(property.handle, done);
        // work around with context assert frame_count
        frame_count ++;
        if (frame_count > 1) splash_done = _splash_done;

        ImGui::EndFrame();
#ifndef __EMSCRIPTEN__
        if (splash_done | done) break;
#endif
        // Rendering
        ImGui::Render();
        glfwMakeContextCurrent(window);
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
        glfwSwapBuffers(window);
    }

    if (property.application.Application_SplashFinalize)
        property.application.Application_SplashFinalize(&property.handle);

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    glfwDestroyWindow(window);
    ImGui::UpdatePlatformWindows();
    return done;
}

int main(int argc, char** argv)
{
    // Setup window
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        return 1;

    property.argc = argc;
    property.argv = argv;
    Application_Setup(property);

    ImVec2 display_scale = ImVec2(1.0, 1.0);
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    auto ctx = ImGui::CreateContext();
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

    // first call application initialize
    if (property.application.Application_Initialize)
        property.application.Application_Initialize(&property.handle);

    // start splash screen if setting
    bool splash_done = false;
#ifndef __EMSCRIPTEN__
    if (property.application.Application_SplashScreen &&
        property.splash_screen_width > 0 &&
        property.splash_screen_height > 0)
    {
        auto app_will_quit = Show_Splash_Window(property, ctx);
        splash_done = true;
        if (app_will_quit)
        {
            ImGui::ImDestroyTextures();
            ImGui::DestroyContext();
            glfwTerminate();
            return 0;
        }
    }
#endif

    std::string title = property.name;
    title += " GLFW_GL2";

    if (property.resizable) glfwWindowHint(GLFW_RESIZABLE, true);
    else glfwWindowHint(GLFW_RESIZABLE, false);
    if (property.full_size || property.full_screen)
    {
        GLFWmonitor* pMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode * mode = glfwGetVideoMode(pMonitor);
        int x = 0, y = 0, w = 0, h = 0;
#if GLFW_HAS_MONITOR_WORK_AREA
        glfwGetMonitorWorkarea(pMonitor, &x, &y, &w, &h);
#endif
        property.pos_x = x;
        property.pos_y = y;
        property.width = w > 0 ? w : mode->width;
        property.height = h > 0 ? h : mode->height;
        property.center = false;
        glfwWindowHint(GLFW_DECORATED, false);
        glfwWindowHint(GLFW_MAXIMIZED, true);
    }
    else
    {
        if (property.top_most)
        {
            glfwWindowHint(GLFW_FLOATING, true);
        }
        else
        {
            glfwWindowHint(GLFW_FLOATING, false);
        }
        if (!property.window_border)
        {
            glfwWindowHint(GLFW_DECORATED, false);
        }
        else
        {
            glfwWindowHint(GLFW_DECORATED, true);
        }
    }

    GLFWwindow* window = glfwCreateWindow(property.width, property.height, title.c_str(), NULL, NULL);
    if (!window)
    {
        printf("GLFW: Create Main window Error!!!\n");
        return 1;
    }

    // Set window icon
    if (!property.icon_path.empty())
    {
        ImGui_ImplGlfw_SetWindowIcon(window, property.icon_path.c_str());
    }
    
    glfwMakeContextCurrent(window);
    glfwSetDropCallback(window, DropCallback);
    glfwSwapInterval(1); // Enable vsync

#if IMGUI_GLEW
    if (glewInit() != GLEW_OK) std::cout << "There is a problem\n ";
    std::cout << glGetString(GL_VERSION) << "\n"; 
#endif

    // Get/Set frame buffer scale
#if !defined(__APPLE__) && GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >=3
    float x_scale, y_scale;
    glfwGetWindowContentScale(window, &x_scale, &y_scale);
    if (x_scale != 1.0 || y_scale != 1.0)
    {
        display_scale = ImVec2(x_scale, y_scale);
    }
#endif
    io.DisplayFramebufferScale = display_scale;

    if (!property.center)
    {
        glfwSetWindowPos(window, property.pos_x, property.pos_y);
    }

    if (property.docking) io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    if (property.viewport)io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    if (!property.auto_merge) io.ConfigViewportsNoAutoMerge = true;
    if (!splash_done && property.application.Application_SetupContext)
        property.application.Application_SetupContext(ctx, property.handle, false);
    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup ImGui binding
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

#if IMGUI_VULKAN_SHADER
    ImGui::ImVulkanShaderInit();
#endif

    // Main loop
    bool done = false;
    bool app_done = false;
    while (!app_done)
    {
        ImGui::ImUpdateTextures();
        ImGui_ImplGlfw_WaitForEvent();
        glfwPollEvents();
        if (glfwWindowShouldClose(window))
            done = true;

        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0 || glfwGetWindowAttrib(window, GLFW_VISIBLE) == 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            app_done = done;
            continue;
        }
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);    
        
        if (property.application.Application_Frame)
            app_done = property.application.Application_Frame(property.handle, done);
        else
            app_done = done;

        ImGui::EndFrame();
#ifndef __EMSCRIPTEN__
        if (app_done) break;
#endif
        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
        glfwSwapBuffers(window);
    }

    if (property.application.Application_Finalize)
        property.application.Application_Finalize(&property.handle);

    // Cleanup
#if IMGUI_VULKAN_SHADER
    ImGui::ImVulkanShaderClear();
#endif
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::ImDestroyTextures();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
