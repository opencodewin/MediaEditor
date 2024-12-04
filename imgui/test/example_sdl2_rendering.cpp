// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// **DO NOT USE THIS CODE IF YOUR CODE/ENGINE IS USING MODERN OPENGL (SHADERS, VBO, VAO, etc.)**
// **Prefer using the code in the example_sdl2_opengl3/ folder**
// See imgui_impl_sdl2.cpp for details.

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "ImGuiFileDialog.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include "Config.h"
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#define GL_LOAD_FORMAT_RGB GL_RGB
#define GL_LOAD_FORMAT_RGBA GL_RGBA

#define WINDOW_WIDTH    1280
#define WINDOW_HEIGHT   720

void load_image(std::string & path, GLuint &texture)
{
    if (texture != -1)
    {
        glDeleteTextures(1, &texture);
        texture = -1;
    }
    SDL_Surface *surface = IMG_Load(path.c_str());
	if (surface != nullptr) {
		int mode = GL_LOAD_FORMAT_RGB;
		if (surface->format->BytesPerPixel == 4) {
			mode = GL_LOAD_FORMAT_RGBA;
		}

		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, mode, GL_UNSIGNED_BYTE, surface->pixels);
		SDL_FreeSurface(surface);
	}
}

// Main code
int main(int, char**)
{
    GLuint framebuffer_texture = -1;
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

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

    // Setup window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
#ifdef DEFAULT_CONFIG_PATH
    std::string ini_file = std::string(DEFAULT_CONFIG_PATH) + "sdl_rendering.ini";
#else
    std::string ini_file = "sdl_rendering.ini";
#endif
    io.IniFilename = ini_file.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // init File Dialog
    ImGuiFileDialog filedialog;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init();

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_KEYDOWN)
            {
                int key = event.key.keysym.scancode;
                if (key == SDL_SCANCODE_Q || key == SDL_SCANCODE_ESCAPE)
                {
                    done = true;
                }
            }
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && 
                event.window.event == SDL_WINDOWEVENT_CLOSE && 
                event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("文件")) 
            {
                if (ImGui::Button("关于..."))
                    ImGui::OpenPopup("关于");
                ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                if (ImGui::BeginPopupModal("关于", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text("SDL 2.0 ImGUI OpenGL2\n\n");
                    ImGui::Separator();
                    ImGui::Text("%.3fms %.1ffps", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                    ImGui::Separator();
                    int i = ImGui::GetCurrentWindow()->ContentSize.x;
                    ImGui::Indent((i - 40.0f) * 0.5f);
                    if (ImGui::Button("OK", ImVec2(40, 0))) { ImGui::CloseCurrentPopup(); }
                    ImGui::SetItemDefaultFocus();
                    ImGui::EndPopup();
                }
                ImGui::Separator();
                if (ImGui::Button("打开..."))
                {
                    const char *filters = "Image files (*.png *.gif *.jpg *.jpeg){.png,.gif,.jpg,.jpeg},.md";
                    IGFD::FileDialogConfig config;
                    config.path = ".";
                    config.flags = ImGuiFileDialogFlags_Modal;
                    config.countSelectionMax = 1;
                    filedialog.OpenDialog("ChooseFileDlgKey", ICON_IGFD_FOLDER_OPEN " Choose a File", filters, config);
                }
                ImGui::Separator();
                if (ImGui::Button("退出")) 
                {
                    done = true;
                }

                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        auto viewport = ImGui::GetWindowViewport();
        ImVec2 maxSize = viewport->Size;
		ImVec2 minSize = maxSize * 0.5f;
        const char* filters = ".*,.jpg,.jpeg,.png";
        if (filedialog.Display("ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
		{
            if (filedialog.IsOk())
			{
                std::string filePathName = filedialog.GetFilePathName();
                load_image(filePathName, framebuffer_texture);
            }
            filedialog.Close();
        }

        int window_width = 0;
        int window_height = 0;
        SDL_GetWindowSize(window, &window_width, &window_height);
        ImVec4 tint(1.0, 1.0, 1.0, 1.0);
		ImGuiWindowFlags flags = ImGuiWindowFlags_None;
		flags = ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoBackground |
				ImGuiWindowFlags_NoMove | 
				ImGuiWindowFlags_NoBringToFrontOnFocus;
		ImVec2 pos(0.0f, 0.0f);
		ImVec2 initial_size((float)window_width, (float)window_height);
		ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(initial_size, ImGuiCond_Always);
		if (framebuffer_texture != -1 && ImGui::Begin("screen", nullptr, flags)) 
		{
            ImVec2 content_region = ImGui::GetContentRegionAvail();
            ImGui::Image((void *)(intptr_t)framebuffer_texture,
                        content_region,
                        ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                        tint);
            ImGui::End();
        }
        // Rendering
        ImGui::Render();
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

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
