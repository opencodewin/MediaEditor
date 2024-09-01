#include "imgui.h"
#include "imgui_helper.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cerrno>
#include "application.h"
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#endif
#include <SDL.h>
#if IMGUI_GLEW
#include <GL/glew.h>
#endif
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <functional>
static std::function<void()>            MainLoopForEmscriptenP;
static void MainLoopForEmscripten()     { MainLoopForEmscriptenP(); }
#define EMSCRIPTEN_MAINLOOP_BEGIN       MainLoopForEmscriptenP = [&]()
#define EMSCRIPTEN_MAINLOOP_END         ; emscripten_set_main_loop(MainLoopForEmscripten, 0, true)
#else
#define EMSCRIPTEN_MAINLOOP_BEGIN
#define EMSCRIPTEN_MAINLOOP_END
#endif


void Application_FullScreen(bool on)
{
    ImGui_ImplSDL2_FullScreen(ImGui::GetMainViewport(), on);
}

static bool Show_Splash_Window(ApplicationWindowProperty& property, ImGuiContext* ctx)
{
    std::string title = property.name + " Splash";
    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
#endif

    int window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
    window_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    window_flags |= SDL_WINDOW_BORDERLESS;
    SDL_Window* window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        property.splash_screen_width, property.splash_screen_height, window_flags);
    if (!window)
    {
        fprintf(stderr, "Failed to Create Splash Window: %s\n", SDL_GetError());
        return false;
    }

    // Set window icon
    if (!property.icon_path.empty())
    {
        ImGui_ImplSDL2_SetWindowIcon(window, property.icon_path.c_str());
    }

    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Set window alpha
    SDL_SetWindowOpacity(window, property.splash_screen_alpha);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    if (property.application.Application_SetupContext)
        property.application.Application_SetupContext(ctx, property.handle, true);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 1.f);
    
    static int frame_count = 0;
    bool done = false;
    bool splash_done = false;
    bool show = true;
#ifdef __EMSCRIPTEN__
    io.IniFilename = NULL;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!splash_done)
#endif
    {
        ImGui::ImUpdateTextures();
        ImGui_ImplSDL2_WaitForEvent();
        SDL_Event event;
        std::vector<std::string> paths;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;

            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SHOWN)
            {
                show = true;
            }
            if (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_HIDDEN || event.window.event == SDL_WINDOWEVENT_MINIMIZED))
            {
                show = false;
            }
            if (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_EXPOSED || event.window.event == SDL_WINDOWEVENT_RESTORED))
            {
                show = true;
            }
            if (event.type == SDL_DROPFILE)
            {
                // file path in event.drop.file
                paths.push_back(event.drop.file);
                show = true;
            }
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED || SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN || !(SDL_GetWindowFlags(window) & SDL_WINDOW_SHOWN))
        {
            show = false;
        }
#ifndef __EMSCRIPTEN__
        if (!show && !(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
        {
            ImGui::sleep(10);
            continue;
        }
#endif
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (!paths.empty())
        {
            if (property.application.Application_DropFromSystem)
                property.application.Application_DropFromSystem(paths);
            paths.clear();
        }
        
        auto _splash_done = property.application.Application_SplashScreen(property.handle, done);
        // work around with context assert frame_count
        frame_count ++;
        if (frame_count > 1) splash_done = _splash_done;

        ImGui::EndFrame();
#ifndef __EMSCRIPTEN__
        if (splash_done || done) break;
#endif
        // Rendering
        ImGui::Render();
        SDL_GL_MakeCurrent(window, gl_context);
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        SDL_GL_SwapWindow(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    if (property.application.Application_SplashFinalize)
        property.application.Application_SplashFinalize(&property.handle);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    ImGui::UpdatePlatformWindows();
    return done;
}

int main(int argc, char** argv)
{
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    ApplicationWindowProperty property(argc, argv);
    Application_Setup(property);

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    int window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_INPUT_FOCUS;
    if (property.resizable) window_flags |= SDL_WINDOW_RESIZABLE;
    if (property.full_size)
    {
        SDL_DisplayMode DM;
        SDL_GetDesktopDisplayMode(0, &DM);
        SDL_Rect r;
        SDL_GetDisplayUsableBounds(0, &r);
        property.pos_x = r.x;
        property.pos_y = r.y;
        property.width = r.w > 0 ? r.w : DM.w;
        property.height = r.h > 0 ? r.h : DM.h;
        property.center = false;
        window_flags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_MAXIMIZED;
    }
    else if (property.full_screen)
    {
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    else
    {
        if (property.top_most)
        {
            window_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
        }
        if (!property.window_border)
        {
            window_flags |= SDL_WINDOW_BORDERLESS;
        }
    }

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
            SDL_Quit();
            return 0;
        }
    }
#endif

    std::string title = property.name;
    title += " SDL_GL3";
    SDL_Window* window = SDL_CreateWindow(title.c_str(), property.center ? SDL_WINDOWPOS_CENTERED : property.pos_x,
                                                        property.center ? SDL_WINDOWPOS_CENTERED : property.pos_y,
                                                        property.width, property.height, window_flags);
    if (!window)
    {
        fprintf(stderr, "Failed to Create Window: %s\n", SDL_GetError());
        return -1;
    }

    // Set window icon
    if (!property.icon_path.empty())
    {
        ImGui_ImplSDL2_SetWindowIcon(window, property.icon_path.c_str());
    }
#ifdef __APPLE__
    if (property.full_size) SDL_SetWindowResizable(window, SDL_FALSE);
#endif
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

#if IMGUI_GLEW
    if (glewInit() != GLEW_OK) std::cout << "There is a problem\n ";
    std::cout << glGetString(GL_VERSION) << "\n"; 
#endif

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    if (property.docking) io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    if (property.viewport)io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    if (!property.auto_merge) io.ConfigViewportsNoAutoMerge = true;
    if (!splash_done && property.application.Application_SetupContext)
        property.application.Application_SetupContext(ctx, property.handle, false);
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);
    SDL_GetWindowSize(window, &property.width, &property.height);

#if IMGUI_VULKAN_SHADER
    ImGui::ImVulkanShaderInit();
#endif

    // Main loop
    bool done = false;
    bool app_done = false;
    bool show = true;
#ifdef __EMSCRIPTEN__
    io.IniFilename = NULL;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!app_done)
#endif
    {
        ImGui::ImUpdateTextures();
        ImGui_ImplSDL2_WaitForEvent();
        SDL_Event event;
        std::vector<std::string> paths;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;

            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SHOWN)
            {
                show = true;
            }
            if (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_HIDDEN || event.window.event == SDL_WINDOWEVENT_MINIMIZED))
            {
                show = false;
            }
            if (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_EXPOSED || event.window.event == SDL_WINDOWEVENT_RESTORED))
            {
                show = true;
            }
            if (event.type == SDL_DROPFILE)
            {
                // file path in event.drop.file
                paths.push_back(event.drop.file);
                show = true;
            }
        }
#ifndef __EMSCRIPTEN__
        if (!show && !(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
        {
            ImGui::sleep(10);
            continue;
        }
#endif
        if (!paths.empty())
        {
            if (property.application.Application_DropFromSystem)
                property.application.Application_DropFromSystem(paths);
            paths.clear();
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
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
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        SDL_GL_SwapWindow(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    if (property.application.Application_Finalize)
        property.application.Application_Finalize(&property.handle);

    // Cleanup
#if IMGUI_VULKAN_SHADER
    ImGui::ImVulkanShaderClear();
#endif
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::ImDestroyTextures();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}