#if defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <windows.h>
#include <tchar.h>
#include <vector>
#include "imgui.h"
#include "imgui_helper.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
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

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif

// Data
static HDC                      g_HDC = NULL;
static HGLRC                    g_HGLRC = NULL;
bool CreateGLContext(HWND hWnd)
{
    // Setup pixelformat descriptor
    constexpr PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32,
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };

    // Setup device context
    g_HDC = GetDC(hWnd);
    int pixelFormat = 0;
    pixelFormat = ChoosePixelFormat(g_HDC, &pfd);
    if (!pixelFormat) return false;
    if (!SetPixelFormat(g_HDC, pixelFormat, &pfd)) return false;
    g_HGLRC = wglCreateContext(g_HDC);
    if (!g_HGLRC) return false;
    if (!wglMakeCurrent(g_HDC, g_HGLRC)) return false;
    return true;
}

void CleanupGLContext(HWND hWnd)
{
    wglMakeCurrent(g_HDC, nullptr);
    wglDeleteContext(g_HGLRC);
    ReleaseDC(hWnd, g_HDC);
}

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED)
            {
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
                return 0;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_DPICHANGED:
            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
            {
                //const int dpi = HIWORD(wParam);
                //printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
                const RECT* suggested_rect = (RECT*)lParam;
                ::SetWindowPos(hWnd, NULL, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            }
            break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

# if defined(_UNICODE)
std::wstring widen(const std::string& str)
{
    int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), (wchar_t*)result.data(), size);
    return result;
}
# endif

void Application_FullScreen(bool on)
{
    ImGui_ImplWin32_FullScreen(ImGui::GetMainViewport(), on);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    const auto c_ClassName  = _T("Imgui Application Class");
    ApplicationWindowProperty property;
    Application_Setup(property);
    if (property.full_size)
    {
        UINT width = GetSystemMetrics(SM_CXSCREEN);
        UINT height = GetSystemMetrics(SM_CYSCREEN);
        property.pos_x = 0;
        property.pos_y = 0;
        property.width = width;
        property.height = height;
        property.center = false;
    }
    if (property.center)
    {
        UINT width = GetSystemMetrics(SM_CXSCREEN);
        UINT height = GetSystemMetrics(SM_CYSCREEN);
        property.pos_x = (width - property.width) / 2;
        property.pos_y = (height - property.height) / 2;
    }

# if defined(_UNICODE)
    const std::wstring c_WindowName = widen(property.name + std::string(" Win32_GL3");
# else
    const std::string c_WindowName = property.name + std::string(" Win32_GL3");
# endif

# if defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
# endif
    // Create application window
    const auto wc = WNDCLASSEX{ sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), LoadIcon(GetModuleHandle(nullptr), IDI_APPLICATION),
        LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, c_ClassName, LoadIcon(GetModuleHandle(nullptr), IDI_APPLICATION) };
    RegisterClassEx(&wc);

    auto hwnd = CreateWindow(c_ClassName, c_WindowName.c_str(), WS_OVERLAPPEDWINDOW,
                            property.pos_x, property.pos_y, property.width, property.height,
                            nullptr, nullptr, wc.hInstance, nullptr);
    if (hwnd == nullptr)
    {
        fprintf(stderr, "Failed to Open window! %s\n", c_WindowName.c_str());
        return 1;
    }
    if (!property.window_border)
    {
        ::SetWindowLong(hwnd, GWL_STYLE, WS_BORDER); 
    }
    // Initialize OpenGL
    const char* glsl_version = "#version 130";
    if (!CreateGLContext(hwnd))
    {
        CleanupGLContext(hwnd);
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

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
    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 1.f);

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init(glsl_version);

#if IMGUI_VULKAN_SHADER
    ImGui::ImVulkanShaderInit();
#endif

    // Show the window
    UINT flags = SWP_SHOWWINDOW;
    if (!property.resizable)
    {
        flags |= SWP_NOSIZE;
    }
    if (property.full_size)
    {
        flags |= SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED;
        ::SetWindowLong(hwnd, GWL_STYLE, (GetWindowLong(hwnd, GWL_STYLE) & ~WS_CAPTION & ~WS_SYSMENU & ~WS_MINIMIZEBOX & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME) | WS_BORDER); 
    }
    else if (property.full_screen)
    {
        ImGui_ImplWin32_FullScreen(ImGui::GetMainViewport(), true);
    }
    ::SetWindowPos(hwnd, property.top_most ? HWND_TOPMOST : NULL, property.pos_x, property.pos_y, property.width, property.height, flags);
    ::UpdateWindow(hwnd);

    if (property.application.Application_Initialize)
        property.application.Application_Initialize(&property.handle);

    // Main loop
    bool done = false;
    bool app_done = false;
    while (!done)
    {
        ImGui::ImUpdateTextures();
        ImGui_ImplWin32_WaitForEvent();
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (app_done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        if (property.application.Application_Frame)
            app_done = property.application.Application_Frame(property.handle, done);
        else
            app_done = done;
        
        if (app_done)
            ::PostQuitMessage(0);

        ImGui::EndFrame();

        if (app_done) break;
        
        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_ClearScreen(ImVec2(0, 0), io.DisplaySize, clear_color);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            auto backup_context = wglGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            wglMakeCurrent(g_HDC, backup_context);
        }

        SwapBuffers(g_HDC);
    }

    if (property.application.Application_Finalize)
        property.application.Application_Finalize(&property.handle);

    // Cleanup
#if IMGUI_VULKAN_SHADER
    ImGui::ImVulkanShaderClear();
#endif
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::ImDestroyTextures();
    ImGui::DestroyContext();

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(wglGetCurrentContext());
    CleanupGLContext(hwnd);
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}
