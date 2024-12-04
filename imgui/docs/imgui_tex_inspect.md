ImGuiTexInspect
=====

ImGuiTexInspect is a texture inspector library for Dear ImGui.  It's a debug tool that allows you to easily inspect the data in any texture.  It provides the following features:
- Zoom and Pan
- Hover to see exact RGBA values
- Annotation system at high zoom which annotates texel cells with:
    - Numerical RGBA values
    - Arrow representation of vector data encoded in color channels (e.g. normal maps)
    - Custom annotations, which can be added easily
- Customizable alpha handling
- Filters to select a combination of red, green, blue & alpha channels
- Per texel matrix-transformation to allow versatile channel swizzling, blending, etc

Screenshot
=====
[![screenshot1](https://andyborrell.github.io/imgui_tex_inspect/Screenshot_1.png)](https://andyborrell.github.io/imgui_tex_inspect/Screenshot_1.png)

Demo
=====

The ```examples``` directory contains example projects for the supported platforms.  Note that the example projects expect the `imgui` directory to be in the same parent directory as `imgui_tex_inspect`.

[You can also see a WebGL build of the demo project here](https://andyborrell.github.io/imgui_tex_inspect)


[![screenshot2](https://andyborrell.github.io/imgui_tex_inspect/Screenshot_2.png)](https://andyborrell.github.io/imgui_tex_inspect/Screenshot_2.png)


Usage
=====
Add an inspector instance to a Dear ImGui window like so:

```cpp
ImGui::Begin("Simple Texture Inspector");
ImGuiTexInspect::BeginInspectorPanel("Inspector", textureHandle, textureSize);
ImGuiTexInspect::EndInspectorPanel();
ImGui::End();
```

Adding annotations to a texture is done by inserting a call to `ImGuiTexInspect::DrawAnnotations` between the calls to `ImGuiTexInspect::BeginInspectorPanel` and `ImGuiTexInspect::EndInspectorPanel`.  The following shows how to add text to the inspector showing the RGBA components for each texel.  Of course, this per-texel annotation is only visible when sufficiently zoomed in.

```cpp
ImGui::Begin("Example Texture##screenshot2");
ImGuiTexInspect::BeginInspectorPanel("Inspector", textureHandle, textureSize);
ImGuiTexInspect::DrawAnnotations(ImGuiTexInspect::ValueText(ImGuiTexInspect::ValueText::Floats));
ImGuiTexInspect::EndInspectorPanel();
ImGui::End();
```

For more advanced usage take a look at `imgui_tex_inspect_demo.cpp`.


Integration
=====
To integrate ImGuiTexInspect to your project you must: 
- Add the `imgui_tex_inspect.cpp`source file
- Add the appropriate renderer source file from `imgui_tex_inspect_test\backends`
- Add `imgui_tex_inspect` and `imgui_tex_inspect\backends` as include directories

The following calls are required to initialize ImGuiTexInspect:
```cpp
ImGuiTexInspect::ImplOpenGL3_Init(); // Or DirectX 11 equivalent (check your chosen backend header file)
ImGuiTexInspect::Init();
ImGuiTexInspect::CreateContext();
```

The main API is in `imgui_tex_inspect.h`

In order to initialize the backend you will also need to include the appropriate backend header file, e.g. `tex_inspect_opengl.h` 


Supported Renderers
===== 
ImGuiTexInspect relies on a renderer specific backend, much like Dear ImGui does.  In fact the backend code for ImGuiTexInspect is very closely based on Dear ImGui's own backend code.  Currently the following backend source files are available:

- `tex_inspect_opengl.cpp`
    - Desktop GL: 2.x 3.x 4.x
    - Embedded GL: ES 2.0 (WebGL 1.0), ES 3.0 (WebGL 2.0)
- `tex_inspect_directx11.cpp`
    - DirectX 11

To avoid linker errors be sure to only include one backend source file.

Dependencies
=====
The only external dependency is Dear ImGui itself.  Dear ImGui version 1.80 onwards is supported.  Older versions of Dear ImGui have not been tested, but could probably be made to work without too much effort.


