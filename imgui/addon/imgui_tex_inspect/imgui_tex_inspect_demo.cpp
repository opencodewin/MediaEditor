// ImGuiTexInspect, a texture inspector widget for dear imgui

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <imgui_tex_inspect_internal.h>
#include <imgui.h>
#include <imgui_helper.h>
#include <imgui_tex_inspect.h>
#include <ImGuiFileDialog.h>

#include <iostream>

namespace ImGuiTexInspect
{
struct Texture
{
    ImTextureID texture {nullptr};
    ImVec2 size {ImVec2(0, 0)};
};

Texture testTex;
Texture fontTexture;
bool testInitted = false;

//-------------------------------------------------------------------------
// [SECTION] FORWARD DECLARATIONS
//-------------------------------------------------------------------------
void Demo_ColorFilters();
void Demo_ColorMatrix();
void Demo_TextureAnnotations();
void Demo_AlphaMode();
void Demo_WrapAndFilter();
void ShowDemoWindow();
void DemoInit();


//-------------------------------------------------------------------------
// [SECTION] EXAMPLE USAGES
//-------------------------------------------------------------------------

/* Each of the following Demo_* functions is a standalone demo showing an example 
 * usage of ImGuiTexInspect.  You'll notice the structure is essentially the same in 
 * all the examples, i.e. a call to BeginInspectorPanel & a call to 
 * EndInspectorPanel, possibly with some annotation functions in between, 
 * followed by some controls to manipulate the inspector.
 *
 * Each Demo_* function corresponds to one of the large buttons at the top of 
 * the demo window.
 */

/* Demo_ColorFilters
 * An example showing controls to filter red, green and blue channels 
 * independently. These controls are provided by the DrawColorChannelSelector 
 * funtions.  The example also uses the DrawGridEditor function to allow the 
 * user to control grid appearance. 
 *
 * The Draw_* functions are provided for convenience, it is of course possible 
 * to control these aspects programmatically.
 */
void Demo_ColorFilters()
{
    /* BeginInspectorPanel & EndInspectorPanel is all you need to draw an
     * inspector (assuming you are already in between an ImGui::Begin and 
     * ImGui::End pair) 
     * */
    static bool flipX = false;
    static bool flipY = false;

    ImGuiTexInspect::InspectorFlags flags = 0;
    if (flipX) SetFlag(flags, ImGuiTexInspect::InspectorFlags_FlipX);
    if (flipY) SetFlag(flags, ImGuiTexInspect::InspectorFlags_FlipY);

    if (ImGuiTexInspect::BeginInspectorPanel("##ColorFilters", testTex.texture, testTex.size, flags))
    {
        // Draw some text showing color value of each texel (you must be zoomed in to see this)
        ImGuiTexInspect::DrawAnnotations(ImGuiTexInspect::ValueText(ImGuiTexInspect::ValueText::BytesDec));
    }
    ImGuiTexInspect::EndInspectorPanel();

    // Now some ordinary ImGui elements to provide some explanation
    ImGui::BeginChild("Controls", ImVec2(600, 100));
    ImGui::TextWrapped("Basics:");
    ImGui::BulletText("Use mouse wheel to zoom in and out.  Click and drag to pan.");
    ImGui::BulletText("Use the demo select buttons at the top of the window to explore");
    ImGui::BulletText("Use the controls below to change basic color filtering options");
    ImGui::EndChild();


    /* DrawColorChannelSelector & DrawGridEditor are convenience functions that 
     * draw ImGui controls to manipulate config of the most recently drawn 
     * texture inspector
     **/
    ImGuiTexInspect::DrawColorChannelSelector();
    ImGui::SameLine(200);
    ImGuiTexInspect::DrawGridEditor();

    ImGui::Separator();

    ImGui::Checkbox("Flip X", &flipX);
    ImGui::Checkbox("Flip Y", &flipY);
}

//-------------------------------------------------------------------------

/* Demo_ColorMatrix 
 * An example showing usage of the ColorMatrix.  See comments at the 
 * declaration of CurrentInspector_SetColorMatrix for details.  This example 
 * shows how to set the matrix directly, as well as how to use the 
 * DrawColorMatrixEditor convenience function to draw ImGui controls to 
 * manipulate it.
 */
void Demo_ColorMatrix()
{
    if (ImGuiTexInspect::BeginInspectorPanel("##ColorMatrix", testTex.texture, testTex.size))
    {
        // Draw some text showing color value of each texel (you must be zoomed in to see this)
        ImGuiTexInspect::DrawAnnotations(ImGuiTexInspect::ValueText(ImGuiTexInspect::ValueText::BytesDec));
    }
    ImGuiTexInspect::EndInspectorPanel();

    ImGui::BeginGroup();
    ImGui::Text("Colour Matrix Editor:");
    // Draw Matrix editor to allow user to manipulate the ColorMatrix
    ImGuiTexInspect::DrawColorMatrixEditor();
    ImGui::EndGroup();

    ImGui::SameLine();

    // Provide some presets that can be used to set the ColorMatrix for example purposes
    ImGui::BeginGroup();
    ImGui::PushItemWidth(200);
    ImGui::Indent(50);
    const ImVec2 buttonSize = ImVec2(160, 0);
    ImGui::Text("Example Presets:");
    // clang-format off
    if (ImGui::Button("Negative", buttonSize))
    {
        // Matrix which inverts each of the red, green, blue channels and leaves Alpha untouched
        float matrix[] = {-1.000f,  0.000f,  0.000f,  0.000f, 
                           0.000f, -1.000f,  0.000f,  0.000f,
                           0.000f,  0.000f, -1.000f,  0.000f, 
                           0.000f,  0.000f,  0.000f,  1.000f};

        float colorOffset[] = {1, 1, 1, 0};
        ImGuiTexInspect::CurrentInspector_SetColorMatrix(matrix, colorOffset);
    }
    if (ImGui::Button("Swap Red & Blue", buttonSize))
    {
        // Matrix which swaps red and blue channels but leaves green and alpha untouched
        float matrix[] = { 0.000f,  0.000f,  1.000f,  0.000f, 
                           0.000f,  1.000f,  0.000f,  0.000f,
                           1.000f,  0.000f,  0.000f,  0.000f, 
                           0.000f,  0.000f,  0.000f,  1.000f};
        float colorOffset[] = {0, 0, 0, 0};
        ImGuiTexInspect::CurrentInspector_SetColorMatrix(matrix, colorOffset);
    }
    if (ImGui::Button("Alpha", buttonSize))
    {
        // Red, green and blue channels are set based on alpha value so that alpha = 1 shows as white. 
        // output alpha is set to 1
        float highlightTransparencyMatrix[] = {0.000f, 0.000f, 0.000f, 0.000f,
                                               0.000f, 0.000f, 0.000f, 0.000f,
                                               0.000f, 0.000f, 0.000f, 0.000f, 
                                               1.000,  1.000,  1.000,  1.000f};
        float highlightTransparencyOffset[] = {0, 0, 0, 1};
        ImGuiTexInspect::CurrentInspector_SetColorMatrix(highlightTransparencyMatrix, highlightTransparencyOffset);
    }
    if (ImGui::Button("Transparency", buttonSize))
    {
        // Red, green and blue channels are scaled by 0.1f. Low alpha values are shown as magenta
        float highlightTransparencyMatrix[] = {0.100f,  0.100f,  0.100f,  0.000f, 
                                               0.100f,  0.100f,  0.100f,  0.000f,
                                               0.100f,  0.100f,  0.100f,  0.000f, 
                                              -1.000f,  0.000f, -1.000f,  0.000f};
        float highlightTransparencyOffset[] = {1, 0, 1, 1};
        ImGuiTexInspect::CurrentInspector_SetColorMatrix(highlightTransparencyMatrix, highlightTransparencyOffset);
    }
    if (ImGui::Button("Default", buttonSize))
    {
        // Default "identity" matrix that doesn't modify colors at all
        float matrix[] = {1.000f, 0.000f, 0.000f, 0.000f, 
                          0.000f, 1.000f, 0.000f, 0.000f,
                          0.000f, 0.000f, 1.000f, 0.000f, 
                          0.000f, 0.000f, 0.000f, 1.000f};

        float colorOffset[] = {0, 0, 0, 0};
        ImGuiTexInspect::CurrentInspector_SetColorMatrix(matrix, colorOffset);
    }
    // clang-format on
    ImGui::PopItemWidth();
    ImGui::EndGroup();
}

/* Demo_AlphaMode
 * Very simple example that calls DrawAlphaModeSelector to draw controls to 
 * allow user to select alpha mode for the inpsector. See InspectorAlphaMode 
 * enum for details on what the different modes are. */
void Demo_AlphaMode()
{
    if (ImGuiTexInspect::BeginInspectorPanel("##AlphaModeDemo", testTex.texture, testTex.size))
    {
        // Add annotations here
    }
    ImGuiTexInspect::EndInspectorPanel();
    ImGuiTexInspect::DrawAlphaModeSelector();
}

/* Demo_WrapAndFilter
 * Demo showing the effect of the InspectorFlags_ShowWrap & InspectorFlags_NoForceFilterNearest flags. 
 * See InspectorFlags_ enum for details on these flags.
 */
void Demo_WrapAndFilter()
{
    static bool showWrap = false;
    static bool forceNearestTexel = true;

    if (ImGuiTexInspect::BeginInspectorPanel("##WrapAndFilter", testTex.texture, testTex.size))
    {
    }
    ImGuiTexInspect::InspectorFlags flags = 0;

    if (showWrap)
        flags |= ImGuiTexInspect::InspectorFlags_ShowWrap;
    if (!forceNearestTexel)
        flags |= ImGuiTexInspect::InspectorFlags_NoForceFilterNearest;

    ImGuiTexInspect::CurrentInspector_SetFlags(flags, ~flags);
    ImGuiTexInspect::EndInspectorPanel();

    ImGui::BeginChild("Controls", ImVec2(600, 0));
    ImGui::TextWrapped("The following option can be enabled to render texture outside of the [0,1] UV range, what you actually "
                       "see outside of this range will depend on the mode of the texture. For example you may see the texture repeat, or "
                       "it might be clamped to the colour of the edge pixels.\nIn this demo the texture is set to wrap.");
    ImGui::Checkbox("Show Wrapping Mode", &showWrap);

    ImGui::TextWrapped("The following option is enabled by default and forces a nearest texel filter, implemented at the shader level. "
                       "By disabling this you can the currently set mode for this texture.");
    ImGui::Checkbox("Force Nearest Texel", &forceNearestTexel);
    ImGui::EndChild();
}

// This class is used in Demo_TextureAnnotations to show the process of creating a new texture annotation.
class CustomAnnotationExample
{
    public:
        void DrawAnnotation(ImDrawList *drawList, ImVec2 texel, ImGuiTexInspect::Transform2D texelsToPixels, ImVec4 value)
        {
            /* A silly example to show the process of creating a new annotation 
             * We'll see which primary colour is the dominant colour in the texel 
             * then draw a different shape for each primary colour.  The radius 
             * will be based on the overall brightness. 
             */
            int numSegments;

            if (value.x > value.y && value.x > value.z)
            {
                // Red pixel - draw a triangle!
                numSegments = 3;
            }
            else
            {
                if (value.y > value.z)
                {
                    // Green pixel - draw a diamond!
                    numSegments = 4;
                }
                else
                {
                    // Blue pixel - draw a hexagon!
                    numSegments = 6;
                }
            }

            // Don't go larger than whole texel
            const float maxRadius = texelsToPixels.Scale.x * 0.5f;

            // Scale radius based on texel brightness
            const float radius = maxRadius * (value.x + value.y + value.z) / 3;
            drawList->AddNgon(texelsToPixels * texel, radius, 0xFFFFFFFF, numSegments);
        }
};

void Demo_TextureAnnotations()
{
    static bool annotationEnabled_arrow = true;
    static bool annotationEnabled_valueText = false;
    static bool annotationEnabled_customExample = false;

    static ImGuiTexInspect::ValueText::Format textFormat = ImGuiTexInspect::ValueText::BytesHex;

    const int maxAnnotatedTexels = 1000;

    if (ImGuiTexInspect::BeginInspectorPanel("##TextureAnnotations", testTex.texture, testTex.size))
    {
        // Draw the currently enabled annotations...
        if (annotationEnabled_arrow)
        {
            ImGuiTexInspect::DrawAnnotations(ImGuiTexInspect::Arrow().UsePreset(ImGuiTexInspect::Arrow::NormalMap), maxAnnotatedTexels);
        }

        if (annotationEnabled_valueText)
        {
            ImGuiTexInspect::DrawAnnotations(ImGuiTexInspect::ValueText(textFormat), maxAnnotatedTexels);
        }

        if (annotationEnabled_customExample)
        {
            ImGuiTexInspect::DrawAnnotations(CustomAnnotationExample(), maxAnnotatedTexels);
        }
    }
    ImGuiTexInspect::EndInspectorPanel();

    // Checkboxes to toggle each type of annotation on and off
    ImGui::BeginChild("Controls", ImVec2(600, 0));
    ImGui::Checkbox("Arrow (Hint: zoom in on the normal-map part of the texture)", &annotationEnabled_arrow);
    ImGui::Checkbox("Value Text",                                                  &annotationEnabled_valueText);
    ImGui::Checkbox("Custom Annotation Example",                                   &annotationEnabled_customExample);
    ImGui::EndChild();

    if (annotationEnabled_valueText)
    {
        // Show a combo to select the text formatting mode
        ImGui::SameLine();
        ImGui::BeginGroup();
        const char *textOptions[] = {"Hex String", "Bytes in Hex", "Bytes in Decimal", "Floats"};
        ImGui::SetNextItemWidth(200);
        int textFormatInt = (int)(textFormat);
        ImGui::Combo("Text Mode", &textFormatInt, textOptions, IM_ARRAYSIZE(textOptions));
        textFormat = (ImGuiTexInspect::ValueText::Format)textFormatInt;
        ImGui::EndGroup();
    }
}

//-------------------------------------------------------------------------
// [SECTION] MAIN DEMO WINDOW FUNCTION
//-------------------------------------------------------------------------

void ShowImGuiTexInspectDemo(bool* p_open)
{
    if (!testInitted)
    {
        DemoInit();
    }

    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1000, 1000), ImGuiCond_FirstUseEver);

    struct DemoConfig
    {
        const char *buttonName;  // Button text to display to user for a demo
        void (*DrawDemoFn)();    // Function which implements the demo
    };
    const DemoConfig demos[] = {
        {"Basics",        Demo_ColorFilters},
        {"Color Matrix",  Demo_ColorMatrix},
        {"Annotations",   Demo_TextureAnnotations},
        {"Alpha Mode",    Demo_AlphaMode},
        {"Wrap & Filter", Demo_WrapAndFilter},
    };

    if (ImGui::Begin("ImGuiTexInspect Demo", p_open))
    {
        ImGui::Text("Select Demo:");
        ImGui::Spacing();

        //Custom color values to example-select buttons to make them stand out
        ImGui::PushStyleColor(ImGuiCol_Button,        (ImVec4)ImColor::HSV(0.59f, 0.7f, 0.8f)); 
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.59f, 0.8f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  (ImVec4)ImColor::HSV(0.59f, 0.9f, 1.0f));

        // Draw row of buttons, one for each demo scene
        static int selectedDemo = 0;
        for (int i = 0; i < IM_ARRAYSIZE(demos); i++)
        {
            if (i != 0)
            {
                ImGui::SameLine();
            }
            if (ImGui::Button(demos[i].buttonName, ImVec2(140, 60)))
            {
                selectedDemo = i;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Image", ImVec2(140, 60)))
        {
            IGFD::FileDialogConfig config;
            config.path = ".";
            config.countSelectionMax = 1;
            config.userDatas = IGFDUserDatas("TexInspect");
            config.flags = ImGuiFileDialogFlags_OpenFile_Default;
            ImGuiFileDialog::Instance()->OpenDialog("##TexInspectDemoFileDlgKey", "Choose Image File", 
                                                    image_filter.c_str(),
                                                    config);

        }

        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();

        ImGui::Spacing();
        
        // Call function to render currently example scene
        demos[selectedDemo].DrawDemoFn();

        ImVec2 minSize = ImVec2(600, 600);
        ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
        if (ImGuiFileDialog::Instance()->Display("##TexInspectDemoFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
        {
            if (ImGuiFileDialog::Instance()->IsOk())
            {
                ImGui::ImDestroyTexture(&testTex.texture);
                testTex.size = ImVec2(0,0);
                auto file_path = ImGuiFileDialog::Instance()->GetFilePathName();
                auto texture = ImGui::ImLoadTexture(file_path.c_str());
                if (texture)
                {
                    testTex.texture = texture;
                    testTex.size = ImVec2(ImGui::ImGetTextureWidth(texture), ImGui::ImGetTextureHeight(texture));
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }
    }
    ImGui::End();
}

//-------------------------------------------------------------------------
// [SECTION] INIT & TEXTURE LOAD
//-------------------------------------------------------------------------

void DemoInit()
{
    ImGuiTexInspect::Init();
    ImGuiTexInspect::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    fontTexture.texture = io.Fonts->TexID;
    fontTexture.size = ImVec2((float)io.Fonts->TexWidth, (float)io.Fonts->TexHeight);

    testInitted = true;
}
} //namespace 
