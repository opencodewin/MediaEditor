#include "imgui.h"
#include "imgui_helper.h"
#include "imgui_impl_glfw.h"
#include <stdio.h>          // printf, fprintf
#include <stdlib.h>         // abort
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cerrno>
#include "application.h"
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#endif
#include "entry_vulkan.h"

#define GLFW_VERSION_COMBINED           (GLFW_VERSION_MAJOR * 1000 + GLFW_VERSION_MINOR * 100 + GLFW_VERSION_REVISION)
#define GLFW_HAS_MONITOR_WORK_AREA      (GLFW_VERSION_COMBINED >= 3300) // 3.3+ glfwGetMonitorWorkarea

static ApplicationWindowProperty property;

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
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
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
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

    GLFWmonitor* pMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode * mode = glfwGetVideoMode(pMonitor);
    glfwSetWindowPos(window, (mode->width - property.splash_screen_width) / 2, (mode->height - property.splash_screen_height) / 2);
    glfwSetDropCallback(window, DropCallback);
    // Setup Vulkan
    if (!glfwVulkanSupported())
    {
        printf("GLFW: Vulkan Not Supported\n");
        return false;
    }

    ImVector<const char*> extensions;
    uint32_t extensions_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    for (uint32_t i = 0; i < extensions_count; i++)
        extensions.push_back(glfw_extensions[i]);
    SetupVulkan(extensions);

    // Create Window Surface
    VkSurfaceKHR surface;
    VkResult err = glfwCreateWindowSurface(g_Instance, window, g_Allocator, &surface);
    check_vk_result(err);

    // Create Framebuffers
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    SetupVulkanWindow(wd, surface, w, h);

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.RenderPass = wd->RenderPass;
    init_info.Subpass = 0;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = check_vk_result;
    init_info.CheckVkResultFn = check_vk_result;
    // Setup ImGui binding
    ImGui_ImplVulkan_Init(&init_info);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    if (property.application.Application_SetupContext)
        property.application.Application_SetupContext(ctx, property.handle, true);
    
    static int frame_count = 0;
    bool done = false;
    bool splash_done = false;
    bool show = true;
    while (!splash_done)
    {
        ImGui::ImUpdateTextures();
        ImGui_ImplGlfw_WaitForEvent();
        glfwPollEvents();
        if (glfwWindowShouldClose(window))
            done = true;
        // Resize swap chain?
        int fb_width, fb_height;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 && (g_SwapChainRebuild || g_MainWindowData.Width != fb_width || g_MainWindowData.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily, g_Allocator, fb_width, fb_height, g_MinImageCount);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
        }
        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
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
        FrameRendering(wd);
    }

    if (property.application.Application_SplashFinalize)
        property.application.Application_SplashFinalize(&property.handle);

    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    CleanupVulkanWindow();
    CleanupVulkan();
    glfwDestroyWindow(window);
    ImGui::UpdatePlatformWindows();
    return done;
}

int main(int argc, char** argv)
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    property.argc = argc;
    property.argv = argv;
    Application_Setup(property);
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    auto ctx = ImGui::CreateContext();
    ImGuiContext& g = *GImGui;
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImVec2 display_scale = ImVec2(1.0, 1.0);
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
    title += " Vulkan GLFW";
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
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

    glfwSetDropCallback(window, DropCallback);

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
    
    // Setup Vulkan
    if (!glfwVulkanSupported())
    {
        printf("GLFW: Vulkan Not Supported\n");
        return 1;
    }
    if (!property.center)
    {
        glfwSetWindowPos(window, property.pos_x, property.pos_y);
    }

    ImVector<const char*> extensions;
    uint32_t extensions_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    for (uint32_t i = 0; i < extensions_count; i++)
        extensions.push_back(glfw_extensions[i]);
    SetupVulkan(extensions);

    // Create Window Surface
    VkSurfaceKHR surface;
    VkResult err = glfwCreateWindowSurface(g_Instance, window, g_Allocator, &surface);
    check_vk_result(err);

    // Create Framebuffers
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    SetupVulkanWindow(wd, surface, w, h);

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

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.RenderPass = wd->RenderPass;
    init_info.Subpass = 0;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = check_vk_result;
    // Setup ImGui binding
    ImGui_ImplVulkan_Init(&init_info);

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

        // Resize swap chain?
        int fb_width, fb_height;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 && (g_SwapChainRebuild || g_MainWindowData.Width != fb_width || g_MainWindowData.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily, g_Allocator, fb_width, fb_height, g_MinImageCount);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
            property.width = fb_width;
            property.height = fb_height;
        }

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
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
        FrameRendering(wd);
    }

    if (property.application.Application_Finalize)
        property.application.Application_Finalize(&property.handle);

    // Cleanup
#if IMGUI_VULKAN_SHADER
    ImGui::ImVulkanShaderClear();
#endif
    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::ImDestroyTextures();
    ImGui::DestroyContext();

    CleanupVulkanWindow();
    CleanupVulkan();

    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
