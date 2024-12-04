#include <imgui.h>
#include <imgui_helper.h>
#include <imgui_fft.h>
#include <imgui_extra_widget.h>
#include <implot.h>
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#endif
#if !IMGUI_ICONS
#define ICON_MD_WARNING "!"
#define ICON_TRUE "T"
#define ICON_FALSE "F"
#else
#define ICON_TRUE u8"\ue5ca"
#define ICON_FALSE u8"\ue5cd"
#endif

namespace ImGui
{
static void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void ShowKnobDemoWindow()
{
    static float val = 0.5, val_default = 0.5;
    float t = (float)ImGui::GetTime();
    float h = abs(sin(t * 0.2));
    float s = abs(sin(t * 0.1)) * 0.5 + 0.4;
    ImVec4 base_color = ImVec4(0.f, 0.f, 0.f, 1.f), active_color = ImVec4(0.f, 0.f, 0.f, 1.f), hovered_color = ImVec4(0.f, 0.f, 0.f, 1.f);
    ImGui::ColorConvertHSVtoRGB(h, s, 0.5f, base_color.x, base_color.y, base_color.z);
    ImGui::ColorConvertHSVtoRGB(h, s, 0.6f, active_color.x, active_color.y, active_color.z);
    ImGui::ColorConvertHSVtoRGB(h, s, 0.7f, hovered_color.x, hovered_color.y, hovered_color.z);
    ImVec4 highlight_base_color = ImVec4(0.f, 0.f, 0.f, 1.f), highlight_active_color = ImVec4(0.f, 0.f, 0.f, 1.f), highlight_hovered_color = ImVec4(0.f, 0.f, 0.f, 1.f);
    ImGui::ColorConvertHSVtoRGB(h, s, 0.75f, highlight_base_color.x, highlight_base_color.y, highlight_base_color.z);
    ImGui::ColorConvertHSVtoRGB(h, s, 0.95f, highlight_active_color.x, highlight_active_color.y, highlight_active_color.z);
    ImGui::ColorConvertHSVtoRGB(h, s, 1.0f, highlight_hovered_color.x, highlight_hovered_color.y, highlight_hovered_color.z);
    ImVec4 lowlight_base_color = ImVec4(0.f, 0.f, 0.f, 1.f), lowlight_active_color = ImVec4(0.f, 0.f, 0.f, 1.f), lowlight_hovered_color = ImVec4(0.f, 0.f, 0.f, 1.f);
    ImGui::ColorConvertHSVtoRGB(h, s, 0.2f, lowlight_base_color.x, lowlight_base_color.y, lowlight_base_color.z);
    ImGui::ColorConvertHSVtoRGB(h, s, 0.3f, lowlight_active_color.x, lowlight_active_color.y, lowlight_active_color.z);
    ImGui::ColorConvertHSVtoRGB(h, s, 0.4f, lowlight_hovered_color.x, lowlight_hovered_color.y, lowlight_hovered_color.z);
    ImVec4 tick_base_color = ImVec4(0.8f, 0.8f, 0.8f, 1.f), tick_active_color = ImVec4(1.f, 1.f, 1.f, 1.f), tick_hovered_color = ImVec4(1.f, 1.f, 1.f, 1.f);
    ColorSet circle_color = {base_color, active_color, hovered_color};
    ColorSet wiper_color = {highlight_base_color, highlight_active_color, highlight_hovered_color};
    ColorSet track_color = {lowlight_base_color, lowlight_active_color, lowlight_hovered_color};
    ColorSet tick_color = {tick_base_color, tick_active_color, tick_hovered_color};

    float knob_size = 80.f;
    float knob_step = NAN; // (max - min) / 200.f
    ImGui::Knob("##Tick", &val, 0.0f, 1.0f, knob_step, val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_TICK, "%.03fdB");
    ImGui::SameLine();
    ImGui::Knob("TickDot", &val, 0.0f, 1.0f, knob_step, val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_TICK_DOT, "%.03fdB");
    ImGui::SameLine();
    ImGui::Knob("TickWiper", &val, 0.0f, 1.0f, knob_step, val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_TICK_WIPER, "%.03fdB");
    ImGui::SameLine();
    ImGui::Knob("Wiper", &val, 0.0f, 1.0f, knob_step, val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_WIPER, "%.03fdB");
    ImGui::SameLine();
    ImGui::Knob("WiperTick", &val, 0.0f, 1.0f, knob_step, val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_WIPER_TICK, "%.03fdB");
    ImGui::SameLine();
    ImGui::Knob("WiperDot", &val, 0.0f, 1.0f, knob_step, val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_WIPER_DOT, "%.03fdB");
    ImGui::SameLine();
    ImGui::Knob("WiperOnly", &val, 0.0f, 1.0f, knob_step, val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_WIPER_ONLY, "%.03fdB");
    ImGui::SameLine();
    ImGui::Knob("SteppedTick", &val, 0.0f, 1.0f, knob_step, val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_TICK, "%.03fdB", 10);
    ImGui::SameLine();
    ImGui::Knob("SteppedDot", &val, 0.0f, 1.0f, knob_step, val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.03fdB", 10);
    ImGui::SameLine();
    ImGui::Knob("Space", &val, 0.0f, 1.0f, knob_step, val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_SPACE, "%.03fdB");

    // no limit knob
    static float no_limit_val = 0.5, no_limit_val_default = 0.5;
    float no_limit_knob_step = 0.01; // (max - min) / 200.f
    ImGui::Knob("##TickNL", &no_limit_val, NAN, NAN, no_limit_knob_step, no_limit_val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_TICK, "%.03f");
    ImGui::SameLine();
    ImGui::Knob("TickDotNL", &no_limit_val, NAN, NAN, no_limit_knob_step, no_limit_val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_TICK_DOT, "%.03f");
    ImGui::SameLine();
    ImGui::Knob("TickWiperNL", &no_limit_val, NAN, NAN, no_limit_knob_step, no_limit_val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_TICK_WIPER, "%.03f");
    ImGui::SameLine();
    ImGui::Knob("WiperNL", &no_limit_val, NAN, NAN, no_limit_knob_step, no_limit_val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_WIPER, "%.03f");
    ImGui::SameLine();
    ImGui::Knob("WiperTickNL", &no_limit_val, NAN, NAN, no_limit_knob_step, no_limit_val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_WIPER_TICK, "%.03f");
    ImGui::SameLine();
    ImGui::Knob("WiperDotNL", &no_limit_val, NAN, NAN, no_limit_knob_step, no_limit_val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_WIPER_DOT, "%.03f");
    ImGui::SameLine();
    ImGui::Knob("WiperOnlyNL", &no_limit_val, NAN, NAN, no_limit_knob_step, no_limit_val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_WIPER_ONLY, "%.03f");
    ImGui::SameLine();
    ImGui::Knob("SteppedTickNL", &no_limit_val, NAN, NAN, no_limit_knob_step, no_limit_val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_TICK, "%.03f", 10);
    ImGui::SameLine();
    ImGui::Knob("SteppedDotNL", &no_limit_val, NAN, NAN, no_limit_knob_step, no_limit_val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.03f", 10);
    ImGui::SameLine();
    ImGui::Knob("SpaceNL", &no_limit_val, NAN, NAN, no_limit_knob_step, no_limit_val_default, knob_size, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_SPACE, "%.03f");

    int idb = val * 80;
    ImGui::Fader("##mastervol", ImVec2(20, 80), &idb, 0, 80, "%d", 1.0f); ImGui::ShowTooltipOnHover("Slide.");
    ImGui::SameLine();
    static int stack = 0;
    static int count = 0;
    ImGui::UvMeter("##vuvr", ImVec2(10, 80), &idb, 0, 80, 20); ImGui::ShowTooltipOnHover("Vertical Uv meters.");
    ImGui::UvMeter("##huvr", ImVec2(80, 10), &idb, 0, 80, 20, &stack, &count); ImGui::ShowTooltipOnHover("Horizon Uv meters width stack effect.");

    //ProgressBar
    static float progress = 0.f;
    progress += 0.1; if (progress > 100.f) progress = 0.f;
    ImGui::RoundProgressBar(knob_size, &progress, 0.f, 100.f, circle_color, wiper_color, tick_color);
}

void ShowExtraWidgetDemoWindow()
{
    if (ImGui::TreeNode("Knob Widget"))
    {
        ShowKnobDemoWindow();
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Extended Widgets"))
    {
        // Check Widgets
        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();ImGui::Text("Check Buttons:");
        ImGui::SameLine();
        static bool checkButtonState1=false;
        if (ImGui::CheckButton("CheckButton",&checkButtonState1, ImVec4(0.5, 0.0, 0.0, 1.0))) {}
        ImGui::SameLine();
        static bool checkButtonState2=false;
        if (ImGui::CheckButton("SmallCheckButton",&checkButtonState2, ImVec4(0.0, 0.5, 0.0, 1.0), true)) {}
        
        ImGui::Spacing();
        ImGui::TextUnformatted("ToggleButton:");ImGui::SameLine();
        ImGui::ToggleButton("ToggleButtonDemo",&checkButtonState1);

        ImGui::Spacing();
        ImGui::Text("ColorButton (by @ocornut)");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s","Code posted by @ocornut here:\nhttps://github.com/ocornut/imgui/issues/4722");
        // [Button rounding depends on the FrameRounding Style property (but can be overridden with the last argument)]
        const float cbv1width = ImGui::GetContentRegionAvail().x*0.45f;
        ImGui::ColoredButton("Hello##ColoredButtonV1Hello", ImVec2(cbv1width, 0.0f), IM_COL32(255, 255, 255, 255), IM_COL32(200, 60, 60, 255), IM_COL32(180, 40, 90, 255));
        ImGui::SameLine();
        ImGui::ColoredButton("You##ColoredButtonV1You", ImVec2(cbv1width, 0.0f), IM_COL32(255, 255, 255, 255), IM_COL32(50, 220, 60, 255), IM_COL32(69, 150, 70, 255),10.0f); // FrameRounding in [0.0,12.0]

        // ColorComboTest
        ImGui::Spacing();
        static ImVec4 chosenColor(1,1,1,1);
        if (ImGui::ColorCombo("ColorCombo",&chosenColor))
        {
            // choice OK here
        }

        ImGui::SameLine();
        static bool color_choose = false;
        ImGui::CheckButton("+",&color_choose, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::ColorChooser(&color_choose, &chosenColor))
        {
            // choice OK here
        }

        // rolling text
        ImGui::Spacing();
        static int speed = 20;
        static std::string long_string = "The quick brown fox jumps over the lazy dog, 那只敏捷的棕毛狐狸跃过那只懒狗";
        ImGui::AddTextRolling(long_string.c_str(), ImVec2(100, 16), speed);
        static std::string ascii_string = "The quick brown fox jumps over the lazy dog";
        ImGui::AddTextRolling(ascii_string.c_str(), ImVec2(100, 16), speed);
        static std::string utf8_string = "那只敏捷的棕毛狐狸跃过那只懒狗";
        ImGui::AddTextRolling(utf8_string.c_str(), ImVec2(100, 16), speed);
        ImGui::PushItemWidth(100);
        ImGui::SliderInt("Speed##rolling text", &speed, 1, 100);
        ImGui::PopItemWidth();

        //Rainbow Text
        ImGui::RainbowText("The quick brown fox jumps over the lazy dog, 那只敏捷的棕毛狐狸跃过那只懒狗");

        // CheckboxFlags Overload
        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();ImGui::Text("CheckBoxFlags Overload:");
        static int numFlags=16;
        static int numRows=2;
        static int numColumns=3;
        ImGui::PushItemWidth(ImGui::GetWindowWidth()*0.2f);
        ImGui::SliderInt("Flags##CBF_Flags",&numFlags,1,20);ImGui::SameLine();
        ImGui::SliderInt("Rows##CBF_Rows",&numRows,1,4);ImGui::SameLine();
        ImGui::SliderInt("Columns##CBF_Columns",&numColumns,1,5);
        ImGui::PopItemWidth();

        static unsigned int cbFlags = (unsigned int)  128+32+8+1;
        static const unsigned int cbAnnotationFlags = 0;//132;   // Optional (default is zero = no annotations)
        int flagIndexHovered = -1;  // Optional
        ImGui::CheckboxFlags("Flags###CBF_Overload",&cbFlags,numFlags,numRows,numColumns,cbAnnotationFlags,&flagIndexHovered);
        if (flagIndexHovered!=-1) {
            // Test: Manual positional tooltip
            ImVec2 m = ImGui::GetIO().MousePos;
            ImGui::SetNextWindowPos(ImVec2(m.x, m.y+ImGui::GetTextLineHeightWithSpacing()));
            ImGui::Begin("CBF_Overload_Tooltip", NULL, ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);
            ImGui::Text("flag %d. Hold shift\nwhile clicking to toggle it",flagIndexHovered);
            ImGui::End();
        }
        // BUG: This is completely wrong (both x and y position):
        //ImGui::SameLine(0,0);ImGui::Text("%s","Test");    // (I don't know how to get this fixed)

        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Line leader"))
    {
        ImGui::Circle(false); ImGui::TextUnformatted("Circle");
        ImGui::Circle(true); ImGui::TextUnformatted("Circle filled");
        ImGui::Circle(false, true); ImGui::TextUnformatted("Circle arrow");
        ImGui::Circle(true, true); ImGui::TextUnformatted("Circle filled arrow");
        ImGui::Separator();
        ImGui::Square(false); ImGui::TextUnformatted("Square");
        ImGui::Square(true); ImGui::TextUnformatted("Square filled");
        ImGui::Square(false, true); ImGui::TextUnformatted("Square arrow");
        ImGui::Square(true, true); ImGui::TextUnformatted("Square filled arrow");
        ImGui::Separator();
        ImGui::BracketSquare(false); ImGui::TextUnformatted("Bracket Square");
        ImGui::BracketSquare(true); ImGui::TextUnformatted("Bracket Square filled");
        ImGui::BracketSquare(false, true); ImGui::TextUnformatted("Bracket Square arrow");
        ImGui::BracketSquare(true, true); ImGui::TextUnformatted("Bracket Square filled arrow");
        ImGui::Separator();
        ImGui::RoundSquare(false); ImGui::TextUnformatted("Round Square");
        ImGui::RoundSquare(true); ImGui::TextUnformatted("Round Square filled");
        ImGui::RoundSquare(false, true); ImGui::TextUnformatted("Round Square arrow");
        ImGui::RoundSquare(true, true); ImGui::TextUnformatted("Round Square filled arrow");
        ImGui::Separator();
        ImGui::GridSquare(false); ImGui::TextUnformatted("Grid Square");
        ImGui::GridSquare(true); ImGui::TextUnformatted("Grid Square filled");
        ImGui::GridSquare(false, true); ImGui::TextUnformatted("Grid Square arrow");
        ImGui::GridSquare(true, true); ImGui::TextUnformatted("Grid Square filled arrow");
        ImGui::Separator();
        ImGui::Diamond(false); ImGui::TextUnformatted("Diamond");
        ImGui::Diamond(true); ImGui::TextUnformatted("Diamond filled");
        ImGui::Diamond(false, true); ImGui::TextUnformatted("Diamond arrow");
        ImGui::Diamond(true, true); ImGui::TextUnformatted("Diamond filled arrow");
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Custom badge button"))
    {
        ImGui::CircleButton("Circle", false); ImGui::SameLine();
        ImGui::CircleButton("Circle filled", true); ImGui::SameLine();
        ImGui::CircleButton("Circle arrow", false, true); ImGui::SameLine();
        ImGui::CircleButton("Circle filled arrow", true, true);

        ImGui::SquareButton("Square", false); ImGui::SameLine();
        ImGui::SquareButton("Square filled", true); ImGui::SameLine();
        ImGui::SquareButton("Square arrow", false, true); ImGui::SameLine();
        ImGui::SquareButton("Square filled arrow", true, true);

        ImGui::BracketSquareButton("Bracket Square", false); ImGui::SameLine();
        ImGui::BracketSquareButton("Bracket Square filled", true); ImGui::SameLine();
        ImGui::BracketSquareButton("Bracket Square arrow", false, true); ImGui::SameLine();
        ImGui::BracketSquareButton("Bracket Square filled arrow", true, true);

        ImGui::RoundSquareButton("Round Square", false); ImGui::SameLine();
        ImGui::RoundSquareButton("Round Square filled", true); ImGui::SameLine();
        ImGui::RoundSquareButton("Round Square arrow", false, true); ImGui::SameLine();
        ImGui::RoundSquareButton("Round Square filled arrow", true, true);

        ImGui::GridSquareButton("Grid Square", false); ImGui::SameLine();
        ImGui::GridSquareButton("Grid Square filled", true); ImGui::SameLine();
        ImGui::GridSquareButton("Grid Square arrow", false, true); ImGui::SameLine();
        ImGui::GridSquareButton("Grid Square filled arrow", true, true);

        ImGui::DiamondButton("Diamond", false); ImGui::SameLine();
        ImGui::DiamondButton("Diamond filled", true); ImGui::SameLine();
        ImGui::DiamondButton("Diamond arrow", false, true); ImGui::SameLine();
        ImGui::DiamondButton("Diamond filled arrow", true, true);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Date Chooser"))
    {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Choose a date(New):");
        ImGui::SameLine();
        static tm myDate1 = {};       // IMPORTANT: must be static! (plenty of compiler warnings here if we write: static tm myDate3={0}; Is there any difference?)
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.2f);
        if (ImGui::InputDate("Date ##MyDate1", myDate1, "%d/%m/%Y")) {
            // A new date has been chosen
            //fprintf(stderr,"A new date has been chosen exacty now: \"%.2d-%.2d-%.4d\"\n",myDate3.tm_mday,myDate3.tm_mon+1,myDate3.tm_year+1900);
        }

        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Choose another date with time(New):");
        ImGui::SameLine();
        static tm myDate2 = {};       // IMPORTANT: must be static! (plenty of compiler warnings here if we write: static tm myDate4={0}; Is there any difference?)
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.2f);
        if (ImGui::InputDateTime("##MyDate2", myDate2, "%d/%m/%Y %H:%M")) {
            // A new date has been chosen
            //fprintf(stderr,"A new date has been chosen exacty now: \"%.2d-%.2d-%.4d\"\n",myDate4.tm_mday,myDate4.tm_mon+1,myDate4.tm_year+1900);
        }

        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Extended ProgressBar and Indicator"))
    {
        const float time = ((float)(((unsigned int) (ImGui::GetTime() * 1000.f)) % 50000) - 25000.f) / 25000.f;
        float progress=(time > 0 ? time : -time);
        // No IDs needed for ProgressBars:
        ImGui::ProgressBar("ProgressBar", progress);
        ImGui::ProgressBar("ProgressBar##demo", 1.f - progress);
        ImGui::ProgressBar("", 500 + progress * 1000, 500, 1500, "%4.0f (absolute value in [500,1500] and fixed bar size)", ImVec2(150, -1));
        ImGui::ProgressBar("", 500 + progress * 1000, 500, 1500, "%3.0f%% (same as above, but with percentage and new colors)", ImVec2(150, -1), ImVec4(0.7, 0.7, 1, 1),ImVec4(0.05, 0.15, 0.5, 0.8),ImVec4(0.8, 0.8, 0,1));
        
        // LoadingIndicatorCircle
        ImGui::Separator();
        ImGui::Text("LoadingIndicatorCircle(...) from https://github.com/ocornut/imgui/issues/1901");
        ImGui::Separator();
        ImGui::TextUnformatted("Test 1:");ImGui::SameLine();
        ImGui::LoadingIndicatorCircle("MyLIC1");ImGui::SameLine();
        ImGui::TextUnformatted("Test 2:");ImGui::SameLine();
        ImGui::LoadingIndicatorCircle("MyLIC2",1.f,&ImGui::GetStyle().Colors[ImGuiCol_Header],&ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered]);
        ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Test 3:");ImGui::SameLine();ImGui::LoadingIndicatorCircle("MyLIC3",2.0f);
        ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Test 4:");ImGui::SameLine();ImGui::LoadingIndicatorCircle("MyLIC4",4.0f,&ImGui::GetStyle().Colors[ImGuiCol_Header],&ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered],12,2.f);
        ImGui::Separator();

        // LoadingIndicatorCircle2
        ImGui::Separator();
        ImGui::Text("LoadingIndicatorCircle2(...) from https://github.com/ocornut/imgui/issues/1901");
        ImGui::Separator();
        ImGui::TextUnformatted("Test 1:");ImGui::SameLine();
        ImGui::LoadingIndicatorCircle2("MyLIC21");ImGui::SameLine();
        ImGui::TextUnformatted("Test 2:");ImGui::SameLine();
        ImGui::LoadingIndicatorCircle2("MyLIC22",1.f,1.5f,&ImGui::GetStyle().Colors[ImGuiCol_Header]);
        ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Test 3:");ImGui::SameLine();ImGui::LoadingIndicatorCircle2("MyLIC23",2.0f);
        ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Test 4:");ImGui::SameLine();ImGui::LoadingIndicatorCircle2("MyLIC24",4.0f,1.f,&ImGui::GetStyle().Colors[ImGuiCol_Header]);

        ImGui::Separator();
        static float buffer_val = 0.0f;
        buffer_val += 0.01;
        if (buffer_val > 1.0) buffer_val = 0;
        ImGui::BufferingBar("##buffer_bar", buffer_val, ImVec2(400, 6), 0.75, ImGui::GetColorU32(ImGuiCol_Button), ImGui::GetColorU32(ImGuiCol_ButtonHovered));
        ImGui::SameLine(); HelpMarker("BufferingBar widget with float value.");

        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Spin"))
    {
        // Spin Test:
        static int int_v = 10;
        ImGui::SpinInt("##spin_int", &int_v, 1, 10);
        ImGui::SameLine(); HelpMarker("Hold key Ctrl to spin fast.");
        static float float_v = 10.0f;
        ImGui::SpinFloat("##spin_float", &float_v, 1.f, 10.f);
        ImGui::SameLine(); HelpMarker("Hold key Ctrl to spin fast.");
        static double double_v = 10.0;
        ImGui::SpinDouble("##spin_double", &double_v, 1., 10.);
        ImGui::SameLine(); HelpMarker("Hold key Ctrl to spin fast.");
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Extra Spinners"))
    {

        static int hue = 0;
        static float nextdot = 0, nextdot2;
        nextdot -= 0.07f;
        static float velocity = 1.f;
        ImGui::SliderFloat("Speed", &velocity, 0.0f, 10.0f, "velocity = %.3f");
        
        static ImVec2 selected{0, 0};
        constexpr int sidex = 10, sidey = 24;

        float widget_size = 50.f;
        static ImColor spinner_filling_meb_bg;
        ImGuiStyle &style = GImGui->Style;
        ImVec2 lastSpacing = style.ItemSpacing, lastPadding = style.WindowPadding;
        style.ItemSpacing = style.WindowPadding = {0.f, 0.f};
        for (int y = 0; y < sidey; y++) {
            for (int x = 0; x < sidex; x++) {
                if (x > 0)
                    ImGui::SameLine();
                ImGui::BeginChild(100 + y * sidex + x, ImVec2(widget_size, widget_size), false, ImGuiWindowFlags_NoScrollbar);
                ImVec2 curpos_begin = ImGui::GetCursorPos();
                ImGui::PushID(y * sidex + x);
                if (ImGui::Selectable("", selected.x == x && selected.y == y, 0, ImVec2(widget_size, widget_size))) {
                    selected = ImVec2((float)x, (float)y);
                }
                float sp_offset = (widget_size - 32.f) / 2.f;
                ImGui::SetCursorPos({curpos_begin.x + sp_offset, curpos_begin.y + sp_offset});
                switch (y * sidex + x) {
                    case   0: ImGui::Spinner<e_st_rainbow>("Spinner", Radius{16.f}, Thickness{2.f}, Color{ImColor::HSV(++hue * 0.005f, 0.8f, 0.8f)}, Speed{8 * velocity}, AngleMin{0.f}, AngleMax{PI_2}, Dots{1}, Mode{0}); ImGui::ShowTooltipOnHover("Spinner"); break;
                    case   1: ImGui::Spinner<e_st_rainbow>("Spinner", Radius{16}, Thickness{2}, Color{ImColor::HSV(++hue * 0.005f, 0.8f, 0.8f)}, Speed{8 * velocity}, AngleMin{0.f}, AngleMax{PI_2}, Dots{4}, Mode{1}); break;
                    case   2: ImGui::Spinner<e_st_dots>("SpinnerDots", Radius{16}, Thickness{4}, Color{ImColor(255, 255, 255)}, FloatPtr{&nextdot}, Speed{1 * velocity}, Dots{12}, MinThickness{-1.f}, Mode{0}); ImGui::ShowTooltipOnHover("SpinnerDots"); break;
                    case   3: ImGui::Spinner<e_st_dots>("SpinnerDots", Radius{16}, Thickness{4}, Color{ImColor(255, 255, 255)}, FloatPtr{&nextdot}, Speed{1 * velocity}, Dots{12}, MinThickness{-1.f}, Mode{1}); ImGui::ShowTooltipOnHover("SpinnerDotsM1"); break;
                    case   4: ImGui::Spinner<e_st_ang>("SpinnerAng270", Radius{16.f}, Thickness{2.f}, Color{ImColor(255, 255, 255)}, BgColor{ImColor(255, 255, 255, 128)}, Speed{6 * velocity}, Angle{270.f / 360.f * 2 * IM_PI}, Mode{0}); ImGui::ShowTooltipOnHover("SpinnerAng270"); break;
                    case   5: ImGui::Spinner<e_st_ang>("SpinnerAng270NoBg", Radius{16.f}, Thickness{2.f}, Color{ImColor(255, 255, 255)}, BgColor{ImColor(255, 255, 255, 0)}, Speed{6 * velocity}, Angle{270.f / 360.f * 2 * IM_PI}); ImGui::ShowTooltipOnHover("SpinnerAng270NoBg"); break;
                    case   6: ImGui::Spinner<e_st_vdots>("SpinnerVDots", Radius{16.f}, Thickness{4.f}, Color{ImColor::HSV(hue * 0.001f, 0.8f, 0.8f)}, BgColor{ImColor::HSV(hue * 0.0011f, 0.8f, 0.8f)}, Speed{2.7f * velocity}, Dots{12}, MiddleDots{6}, Mode{0}); ImGui::ShowTooltipOnHover("SpinnerVDots"); break;
                    case   7: ImGui::Spinner<e_st_bounce_ball>("SpinnerBounceBall", Radius{16.f}, Thickness{6.f}, Color{ImColor(255, 255, 255)}, Speed{4 * velocity}, Dots{1}, Mode{0}); ImGui::ShowTooltipOnHover("SpinnerBounceBall"); break;
                    case   8: ImGui::Spinner<e_st_eclipse>("SpinnerAngEclipse", Radius{16.f}, Thickness{5.f}, Color{ImColor(255, 255, 255)}, Speed{6 * velocity}); ImGui::ShowTooltipOnHover("SpinnerAngEclipse"); break;
                    case   9: ImGui::Spinner<e_st_ingyang>("SpinnerIngYang", Radius{16.f}, Thickness{5.f}, Reverse{false}, Delta{0.f}, Color{ImColor(255, 255, 255)}, AltColor{ImColor(255, 0, 0)}, Speed{4 * velocity}, Angle{IM_PI * 0.8f}); ImGui::ShowTooltipOnHover("SpinnerIngYang"); break;
                    case  10: ImGui::Spinner<e_st_barchartsine>("SpinnerBarChartSine", Radius{16}, Thickness{4}, Color{ImColor(255, 255, 255)}, Speed{6.8f * velocity}, Dots{4}, Mode{0}); ImGui::ShowTooltipOnHover("SpinnerBarChartSine"); break;
                    case  11: ImGui::SpinnerBounceDots("SpinnerBounceDots", 16, 6, ImColor(255, 255, 255), 6 * velocity, 3, 0); ImGui::ShowTooltipOnHover("SpinnerBounceDots"); break;
                    case  12: ImGui::SpinnerFadeDots("SpinnerFadeDots", 16, 6, ImColor(255, 255, 255), 8 * velocity, 8); ImGui::ShowTooltipOnHover("SpinnerFadeDots"); break;
                    case  13: ImGui::SpinnerScaleDots("SpinnerScaleDots", 16, 6, ImColor(255, 255, 255), 7 * velocity, 8); ImGui::ShowTooltipOnHover("SpinnerScaleDots"); break;
                    case  14: ImGui::SpinnerMovingDots("SpinnerMovingDots", 16, 6, ImColor(255, 255, 255), 30 * velocity, 3); ImGui::ShowTooltipOnHover("SpinnerMovingDots"); break;
                    case  15: ImGui::SpinnerRotateDots("SpinnerRotateDots", 16, 6, ImColor(255, 255, 255), 4 * velocity, 2, 0); ImGui::ShowTooltipOnHover("SpinnerRotateDots"); break;
                    case  16: ImGui::SpinnerTwinAng("SpinnerTwinAng", 16, 16, 6, ImColor(255, 255, 255), ImColor(255, 0, 0), 4 * velocity, IM_PI, 0); ImGui::ShowTooltipOnHover("SpinnerTwinAng"); break;
                    case  17: ImGui::SpinnerClock("SpinnerClock", 16, 2, ImColor(255, 0, 0), ImColor(255, 255, 255), 4 * velocity); ImGui::ShowTooltipOnHover("SpinnerClock"); break;
                    case  18: ImGui::SpinnerIngYang("SpinnerIngYangR", 16, 5, true, 0.1f, ImColor(255, 255, 255), ImColor(255, 0, 0), 4 * velocity, IM_PI * 0.8f); ImGui::ShowTooltipOnHover("SpinnerIngYangR"); break;
                    case  19: ImGui::SpinnerBarChartSine("SpinnerBarChartSine2", 16, 4, ImColor::HSV(hue * 0.005f, 0.8f, 0.8f), 4.8f * velocity, 4, 1); ImGui::ShowTooltipOnHover("SpinnerBarChartSine2"); break;
                    case  20: ImGui::SpinnerTwinAng180("SpinnerTwinAng", 16, 12, 4, ImColor(255, 255, 255), ImColor(255, 0, 0), 4 * velocity, PI_DIV_4, 0); ImGui::ShowTooltipOnHover("SpinnerTwinAng"); break;
                    case  21: ImGui::SpinnerTwinAng180("SpinnerTwinAng2", 16, 11, 4, ImColor(255, 255, 255), ImColor(255, 0, 0), 4 * velocity, IM_PI, 1); ImGui::ShowTooltipOnHover("SpinnerTwinAng2"); break;
                    case  22: ImGui::SpinnerIncDots("SpinnerIncDots", 16, 4, ImColor(255, 255, 255), 5.6f * velocity, 6); ImGui::ShowTooltipOnHover("SpinnerIncDots"); break;
                    case  23: nextdot2 -= 0.2f * velocity; ImGui::SpinnerDots("SpinnerDotsWoBg", &nextdot2, 16, 4, ImColor(255, 255, 255), 0.3f * velocity, 12, 0.f, 0); ImGui::ShowTooltipOnHover("SpinnerDotsWoBg"); break;
                    case  24: ImGui::SpinnerIncScaleDots("SpinnerIncScaleDots", 16, 4, ImColor(255, 255, 255), 6.6f * velocity, 6, 0, 0); ImGui::ShowTooltipOnHover("SpinnerIncScaleDots"); break;
                    case  25: ImGui::SpinnerAng("SpinnerAng90", 16, 6, ImColor(255, 255, 255), ImColor(255, 255, 255, 128), 8.f * velocity, IM_PI / 2.f, 0); ImGui::ShowTooltipOnHover("SpinnerAng90"); break;
                    case  26: ImGui::SpinnerAng("SpinnerAng90NoBg", 16, 6, ImColor(255, 255, 255), ImColor(255, 255, 255, 0), 8.5f * velocity, IM_PI / 2.f, 0); ImGui::ShowTooltipOnHover("SpinnerAng90NoBg"); break;
                    case  27: ImGui::SpinnerFadeBars("SpinnerFadeBars", 10, ImColor(255, 255, 255), 4.8f * velocity, 3); ImGui::ShowTooltipOnHover("SpinnerFadeBars"); break;
                    case  28: ImGui::SpinnerPulsar("SpinnerPulsar", 16, 2, ImColor(255, 255, 255), 1 * velocity, true, 0, 0); ImGui::ShowTooltipOnHover("SpinnerPulsar"); break;
                    case  29: ImGui::SpinnerIngYang("SpinnerIngYangR2", 16, 5, true, 3.f, ImColor(255, 255, 255), ImColor(255, 0, 0), 4 * velocity, IM_PI * 0.8f); ImGui::ShowTooltipOnHover("SpinnerIngYangR2"); break;
                    case  30: ImGui::SpinnerBarChartRainbow("SpinnerBarChartRainbow", 16, 4, ImColor::HSV(hue * 0.005f, 0.8f, 0.8f), 6.8f * velocity, 4, 0); ImGui::ShowTooltipOnHover("SpinnerBarChartRainbow"); break;
                    case  31: ImGui::SpinnerBarsRotateFade("SpinnerBarsRotateFade", 8, 18, 4, ImColor(255, 255, 255), 7.6f, 6); ImGui::ShowTooltipOnHover("SpinnerBarsRotateFade"); break;
                    case  32: ImGui::SpinnerFadeBars("SpinnerFadeScaleBars", 10, ImColor(255, 255, 255), 6.8f, 3, true); ImGui::ShowTooltipOnHover("SpinnerFadeScaleBars"); break;
                    case  33: ImGui::SpinnerBarsScaleMiddle("SpinnerBarsScaleMiddle", 6, ImColor(255, 255, 255), 8.8f, 3); ImGui::ShowTooltipOnHover("SpinnerBarsScaleMiddle"); break;
                    case  34: ImGui::SpinnerAngTwin("SpinnerAngTwin1", 16, 13, 2, ImColor(255, 0, 0), ImColor(255, 255, 255), 6 * velocity, PI_DIV_2, 1, 0); ImGui::ShowTooltipOnHover("SpinnerAngTwin1"); break;
                    case  35: ImGui::SpinnerAngTwin("SpinnerAngTwin2", 13, 16, 2, ImColor(255, 0, 0), ImColor(255, 255, 255), 6 * velocity, PI_DIV_2, 1, 0); ImGui::ShowTooltipOnHover("SpinnerAngTwin2"); break;
                    case  36: ImGui::SpinnerAngTwin("SpinnerAngTwin3", 13, 16, 2, ImColor(255, 0, 0), ImColor(255, 255, 255), 6 * velocity, PI_DIV_2, 2, 0); ImGui::ShowTooltipOnHover("SpinnerAngTwin3"); break;
                    case  37: ImGui::SpinnerAngTwin("SpinnerAngTwin4", 16, 13, 2, ImColor(255, 0, 0), ImColor(255, 255, 255), 6 * velocity, PI_DIV_2, 2, 0); ImGui::ShowTooltipOnHover("SpinnerAngTwin4"); break;
                    case  38: ImGui::SpinnerTwinPulsar("SpinnerTwinPulsar", 16, 2, ImColor(255, 255, 255), 0.5f * velocity, 2, 0); ImGui::ShowTooltipOnHover("SpinnerTwinPulsar"); break;
                    case  39: ImGui::SpinnerAngTwin("SpinnerAngTwin4", 14, 13, 3, ImColor(255, 0, 0), ImColor(0, 0, 0, 0), 5 * velocity, IM_PI / 1.5f, 2); ImGui::ShowTooltipOnHover("SpinnerAngTwin4"); break;
                    case  40: ImGui::SpinnerBlocks("SpinnerBlocks", 16, 7, ImColor(255, 255, 255, 30), ImColor::HSV(hue * 0.005f, 0.8f, 0.8f), 5 * velocity); ImGui::ShowTooltipOnHover("SpinnerBlocks"); break;
                    case  41: ImGui::SpinnerTwinBall("SpinnerTwinBall", 16, 11, 2, 2.5f, ImColor(255, 0, 0), ImColor(255, 255, 255), 6 * velocity, 2); ImGui::ShowTooltipOnHover("SpinnerTwinBall"); break;
                    case  42: ImGui::SpinnerTwinBall("SpinnerTwinBall2", 15, 19, 2, 2.f, ImColor(255, 0, 0), ImColor(255, 255, 255), 6 * velocity, 3); ImGui::ShowTooltipOnHover("SpinnerTwinBall2"); break;
                    case  43: ImGui::SpinnerTwinBall("SpinnerTwinBall2", 16, 16, 2, 5.f, ImColor(255, 0, 0), ImColor(255, 255, 255), 5 * velocity, 1); ImGui::ShowTooltipOnHover("SpinnerTwinBall2"); break;
                    case  44: ImGui::SpinnerAngTriple("SpinnerAngTriple", 16, 13, 10, 1.3f, ImColor(255, 255, 255), ImColor(255, 0, 0), ImColor(255, 255, 255), 5 * velocity, 1.5f * IM_PI); ImGui::ShowTooltipOnHover("SpinnerAngTriple"); break;
                    case  45: ImGui::SpinnerIncFullDots("SpinnerIncFullDots", 16, 4, ImColor(255, 255, 255), 5.6f, 4); ImGui::ShowTooltipOnHover("SpinnerIncFullDots"); break; 
                    case  46: ImGui::SpinnerGooeyBalls("SpinnerGooeyBalls", 16, ImColor(255, 255, 255), 2.f); ImGui::ShowTooltipOnHover("SpinnerGooeyBalls"); break;
                    case  47: ImGui::SpinnerRotateGooeyBalls("SpinnerRotateGooeyBalls2", 16, 5, ImColor(255, 255, 255), 6.f, 2, 0); ImGui::ShowTooltipOnHover("SpinnerRotateGooeyBalls2"); break;
                    case  48: ImGui::SpinnerRotateGooeyBalls("SpinnerRotateGooeyBalls3", 16, 5, ImColor(255, 255, 255), 6.f, 3, 0); ImGui::ShowTooltipOnHover("SpinnerRotateGooeyBalls3"); break;
                    case  49: ImGui::SpinnerMoonLine("SpinnerMoonLine", 16, 3, ImColor(200, 80, 0), ImColor(80, 80, 80), 5 * velocity); ImGui::ShowTooltipOnHover("SpinnerMoonLine"); break;
                    case  50: ImGui::SpinnerArcRotation("SpinnerArcRotation", 13, 5, ImColor(255, 255, 255), 3 * velocity, 4, 0); ImGui::ShowTooltipOnHover("SpinnerArcRotation"); break;
                    case  51: ImGui::SpinnerFluid("SpinnerFluid", 16, ImColor(0, 0, 255), 3.8f * velocity, 4); ImGui::ShowTooltipOnHover("SpinnerFluid"); break;
                    case  52: ImGui::SpinnerArcFade("SpinnerArcFade", 13, 5, ImColor(255, 255, 255), 3 * velocity, 4, 0); ImGui::ShowTooltipOnHover("SpinnerArcFade"); break;
                    case  53: ImGui::SpinnerFilling("SpinnerFilling", 16, 6, ImColor(255, 255, 255), ImColor(255, 0, 0), 4 * velocity); ImGui::ShowTooltipOnHover("SpinnerFilling"); break;
                    case  54: ImGui::SpinnerTopup("SpinnerTopup", 16, 12, ImColor(255, 0, 0), ImColor(80, 80, 80), ImColor(255, 255, 255), 1 * velocity); ImGui::ShowTooltipOnHover("SpinnerTopup");  break;
                    case  55: ImGui::SpinnerFadePulsar("SpinnerFadePulsar", 16, ImColor(255, 255, 255), 1.5f * velocity, 1, 0); ImGui::ShowTooltipOnHover("SpinnerFadePulsar");  break;
                    case  56: ImGui::SpinnerFadePulsar("SpinnerFadePulsar2", 16, ImColor(255, 255, 255), 0.9f * velocity, 2, 0); ImGui::ShowTooltipOnHover("SpinnerFadePulsar2"); break;
                    case  57: ImGui::SpinnerPulsar("SpinnerPulsar", 16, 2, ImColor(255, 255, 255), 1 * velocity, false); ImGui::ShowTooltipOnHover("SpinnerPulsar"); break;
                    case  58: ImGui::SpinnerDoubleFadePulsar("SpinnerDoubleFadePulsar", 16, 2, ImColor(255, 255, 255), 2 * velocity); ImGui::ShowTooltipOnHover("SpinnerDoubleFadePulsar"); break;
                    case  59: ImGui::SpinnerFilledArcFade("SpinnerFilledArcFade", 16, ImColor(255, 255, 255), 4 * velocity, 4); ImGui::ShowTooltipOnHover("SpinnerFilledArcFade"); break;
                    case  60: ImGui::SpinnerFilledArcFade("SpinnerFilledArcFade6", 16, ImColor(255, 255, 255), 6 * velocity, 6); ImGui::ShowTooltipOnHover("SpinnerFilledArcFade6"); break;
                    case  61: ImGui::SpinnerFilledArcFade("SpinnerFilledArcFade6", 16, ImColor(255, 255, 255), 8 * velocity, 12); ImGui::ShowTooltipOnHover("SpinnerFilledArcFade6"); break;
                    case  62: ImGui::SpinnerFilledArcColor("SpinnerFilledArcColor", 16, ImColor(255, 0, 0), ImColor(255, 255, 255), 2.8f * velocity, 4); ImGui::ShowTooltipOnHover("SpinnerFilledArcColor"); break;
                    case  63: ImGui::SpinnerCircleDrop("SpinnerCircleDrop", 16, 1.5f, 4.f, ImColor(255, 0, 0), ImColor(255, 255, 255), 2.8f * velocity, IM_PI); ImGui::ShowTooltipOnHover("SpinnerCircleDrop"); break;
                    case  64: ImGui::SpinnerSurroundedIndicator("SpinnerSurroundedIndicator", 16, 5, ImColor(0, 0, 0), ImColor(255, 255, 255), 7.8f * velocity); ImGui::ShowTooltipOnHover("SpinnerSurroundedIndicator"); break;
                    case  65: ImGui::SpinnerTrianglesSelector("SpinnerTrianglesSelector", 16, 8, ImColor(0, 0, 0), ImColor(255, 255, 255), 4.8f * velocity, 8); ImGui::ShowTooltipOnHover("SpinnerTrianglesSelector"); break;
                    case  66: ImGui::SpinnerFlowingGradient("SpinnerFlowingFradient", 16, 6, ImColor(200, 80, 0), ImColor(80, 80, 80), 5 * velocity, IM_PI * 2.f); ImGui::ShowTooltipOnHover("SpinnerFlowingFradient"); break;
                    case  67: ImGui::SpinnerRotateSegments("SpinnerRotateSegments", 16, 4, ImColor(255, 255, 255), 3 * velocity, 4, 1, 0); ImGui::ShowTooltipOnHover("SpinnerRotateSegments"); break;
                    case  68: ImGui::SpinnerRotateSegments("SpinnerRotateSegments2", 16, 3, ImColor(255, 255, 255), 2.4f * velocity, 4, 2, 0); ImGui::ShowTooltipOnHover("SpinnerRotateSegments2"); break;
                    case  69: ImGui::SpinnerRotateSegments("SpinnerRotateSegments3", 16, 2, ImColor(255, 255, 255), 2.1f * velocity, 4, 3, 0); ImGui::ShowTooltipOnHover("SpinnerRotateSegments3"); break;
                    case  70: ImGui::SpinnerLemniscate("SpinnerLemniscate", 20, 3, ImColor(255, 255, 255), 2.1f * velocity, 3); ImGui::ShowTooltipOnHover("SpinnerLemniscate"); break;
                    case  71: ImGui::SpinnerRotateGear("SpinnerRotateGear", 16, 6, ImColor(255, 255, 255), 2.1f * velocity, 8); ImGui::ShowTooltipOnHover("SpinnerRotateGear"); break;
                    case  72: ImGui::SpinnerRotatedAtom("SpinnerRotatedAtom", 16, 2, ImColor(255, 255, 255), 2.1f * velocity, 3, 0); ImGui::ShowTooltipOnHover("SpinnerRotatedAtom"); break;
                    case  73: ImGui::SpinnerAtom("SpinnerAtom", 16, 2, ImColor(255, 255, 255), 4.1f * velocity, 3); ImGui::ShowTooltipOnHover("SpinnerAtom"); break;
                    case  74: ImGui::SpinnerRainbowBalls("SpinnerRainbowBalls", 16, 4, ImColor::HSV(0.25f, 0.8f, 0.8f, 0.f), 1.5f * velocity, 5, 0); ImGui::ShowTooltipOnHover("SpinnerRainbowBalls"); break;
                    case  75: ImGui::SpinnerCamera("SpinnerCamera", 16, 8, [] (int i) { return ImColor::HSV(i * 0.25f, 0.8f, 0.8f); }, 4.8f * velocity, 8, 0); ImGui::ShowTooltipOnHover("SpinnerCamera"); break;
                    case  76: ImGui::SpinnerArcPolarFade("SpinnerArcPolarFade", 16, ImColor(255, 255, 255), 6 * velocity, 6, 0); ImGui::ShowTooltipOnHover("SpinnerArcPolarFade"); break;
                    case  77: ImGui::SpinnerArcPolarRadius("SpinnerArcPolarRadius", 16, ImColor::HSV(0.25f, 0.8f, 0.8f), 6.f * velocity, 6); ImGui::ShowTooltipOnHover("SpinnerArcPolarRadius"); break;
                    case  78: ImGui::SpinnerCaleidoscope("SpinnerArcPolarPies", 16, 4, ImColor::HSV(0.25f, 0.8f, 0.8f), 2.6f * velocity, 10, 0); ImGui::ShowTooltipOnHover("SpinnerArcPolarPies"); break;
                    case  79: ImGui::SpinnerCaleidoscope("SpinnerArcPolarPies2", 16, 4, ImColor::HSV(0.35f, 0.8f, 0.8f), 3.2f * velocity, 10, 1); ImGui::ShowTooltipOnHover("SpinnerArcPolarPies2"); break;
                    case  80: ImGui::SpinnerScaleBlocks("SpinnerScaleBlocks", 16, 8, ImColor::HSV(hue * 0.005f, 0.8f, 0.8f), 5 * velocity); ImGui::ShowTooltipOnHover("SpinnerScaleBlocks"); break;
                    case  81: ImGui::SpinnerRotateTriangles("SpinnerRotateTriangles", 16, 2, ImColor(255, 255, 255), 6.f * velocity, 3); ImGui::ShowTooltipOnHover("SpinnerRotateTriangles"); break;
                    case  82: ImGui::SpinnerArcWedges("SpinnerArcWedges", 16, ImColor::HSV(0.3f, 0.8f, 0.8f), 2.8f * velocity, 4); ImGui::ShowTooltipOnHover("SpinnerArcWedges"); break;
                    case  83: ImGui::SpinnerScaleSquares("SpinnerScaleSquares", 16, 8, ImColor::HSV(hue * 0.005f, 0.8f, 0.8f), 5 * velocity); ImGui::ShowTooltipOnHover("SpinnerScaleSquares"); break;
                    case  84: ImGui::SpinnerHboDots("SpinnerHboDots", 16, 4, ImColor(255, 255, 255), 0.f, 0.f, 1.1f * velocity, 6); ImGui::ShowTooltipOnHover("SpinnerHboDots"); break;
                    case  85: ImGui::SpinnerHboDots("SpinnerHboDots2", 16, 4, ImColor(255, 255, 255), 0.1f, 0.5f, 1.1f * velocity, 6); ImGui::ShowTooltipOnHover("SpinnerHboDots2"); break;
                    case  86: ImGui::Spinner<e_st_bounce_ball>("SpinnerBounceBall3", Radius{16}, Thickness{4}, Color{ImColor(255, 255, 255)}, Speed{3.2f * velocity}, Dots{5}); ImGui::ShowTooltipOnHover("SpinnerBounceBall3"); break;
                    case  87: ImGui::SpinnerBounceBall("SpinnerBounceBallShadow", 16, 4, ImColor(255, 255, 255), 2.2f * velocity, 1, true); ImGui::ShowTooltipOnHover("SpinnerBounceBallShadow"); break;
                    case  88: ImGui::SpinnerBounceBall("SpinnerBounceBall5Shadow", 16, 4, ImColor(255, 255, 255), 3.6f * velocity, 5, true); ImGui::ShowTooltipOnHover("SpinnerBounceBall5Shadow"); break;
                    case  89: ImGui::SpinnerSquareStrokeFade("SpinnerSquareStrokeFade", 13, 5, ImColor(255, 255, 255), 3 * velocity); ImGui::ShowTooltipOnHover("SpinnerSquareStrokeFade"); break;
                    case  90: ImGui::SpinnerSquareStrokeFill("SpinnerSquareStrokeFill", 13, 5, ImColor(255, 255, 255), 3 * velocity); ImGui::ShowTooltipOnHover("SpinnerSquareStrokeFill"); break;
                    case  91: ImGui::SpinnerSwingDots("SpinnerSwingDots", 16, 6, ImColor(255, 0, 0), 4.1f * velocity); ImGui::ShowTooltipOnHover("SpinnerSwingDots"); break;
                    case  92: ImGui::SpinnerRotateWheel("SpinnerRotateWheel", 16, 10, ImColor(255, 255, 0), ImColor(255, 255, 255), 2.1f * velocity, 8); ImGui::ShowTooltipOnHover("SpinnerRotateWheel"); break;
                    case  93: ImGui::SpinnerWaveDots("SpinnerWaveDots", 16, 3, ImColor(255, 255, 255), 6 * velocity, 8); ImGui::ShowTooltipOnHover("SpinnerWaveDots"); break;
                    case  94: ImGui::SpinnerRotateShapes("SpinnerRotateShapes", 16, 2, ImColor(255, 255, 255), 6.f * velocity, 4, 4); ImGui::ShowTooltipOnHover("SpinnerRotateShapes"); break;
                    case  95: ImGui::SpinnerSquareStrokeLoading("SpinnerSquareStrokeLoanding", 13, 5, ImColor(255, 255, 255), 3 * velocity); ImGui::ShowTooltipOnHover("SpinnerSquareStrokeLoanding"); break;
                    case  96: ImGui::SpinnerSinSquares("SpinnerSinSquares", 16, 2, ImColor(255, 255, 255), 1.f * velocity, 0); ImGui::ShowTooltipOnHover("SpinnerSinSquares"); break;
                    case  97: ImGui::SpinnerZipDots("SpinnerZipDots", 16, 3, ImColor(255, 255, 255), 6* velocity, 5); ImGui::ShowTooltipOnHover("SpinnerZipDots"); break;
                    case  98: ImGui::SpinnerDotsToBar("SpinnerDotsToBar", 16, 3, 0.5f, ImColor::HSV(0.31f, 0.8f, 0.8f), 5 * velocity, 5); ImGui::ShowTooltipOnHover("SpinnerDotsToBar"); break;
                    case  99: ImGui::SpinnerSineArcs("SpinnerSineArcs", 16, 1, ImColor(255, 255, 255), 3 * velocity); ImGui::ShowTooltipOnHover("SpinnerSineArcs"); break;
                    case 100: ImGui::SpinnerTrianglesShift("SpinnerTrianglesShift", 16, 8, ImColor(0, 0, 0), ImColor(255, 255, 255), 1.8f * velocity, 8); ImGui::ShowTooltipOnHover("SpinnerTrianglesShift"); break;
                    case 101: ImGui::SpinnerCircularLines("SpinnerCircularLines", 16, ImColor(255, 255, 255), 1.5f * velocity, 8, 0); ImGui::ShowTooltipOnHover("SpinnerCircularLines"); break;
                    case 102: ImGui::SpinnerLoadingRing("SpinnerLoadingRing", 16, 6, ImColor(255, 0, 0), ImColor(255, 255, 255, 128), 1.f * velocity, 5); ImGui::ShowTooltipOnHover("SpinnerLoadingRing"); break;
                    case 103: ImGui::SpinnerPatternRings("SpinnerPatternRings", 16, 2, ImColor(255, 255, 255), 4.1f * velocity, 3); ImGui::ShowTooltipOnHover("SpinnerPatternRings"); break;
                    case 104: ImGui::SpinnerPatternSphere("SpinnerPatternSphere", 16, 2, ImColor(255, 255, 255), 2.1f * velocity, 6); ImGui::ShowTooltipOnHover("SpinnerPatternSphere"); break;
                    case 105: ImGui::SpinnerRingSynchronous("SpinnerRingSnchronous", 16, 2, ImColor(255, 255, 255), 2.1f * velocity, 3); ImGui::ShowTooltipOnHover("SpinnerRingSnchronous"); break;
                    case 106: ImGui::SpinnerRingWatermarks("SpinnerRingWatermarks", 16, 2, ImColor(255, 255, 255), 2.1f * velocity, 3); ImGui::ShowTooltipOnHover("SpinnerRingWatermarks"); break;
                    case 107: ImGui::SpinnerFilledArcRing("SpinnerFilledArcRing", 16, 6, ImColor(255, 0, 0), ImColor(255, 255, 255), 2.8f * velocity, 8); ImGui::ShowTooltipOnHover("SpinnerFilledArcRing"); break;
                    case 108: ImGui::SpinnerPointsShift("SpinnerPointsShift", 16, 3, ImColor(0, 0, 0), ImColor(255, 255, 255), 1.8f * velocity, 10); ImGui::ShowTooltipOnHover("SpinnerPointsShift"); break;
                    case 109: ImGui::SpinnerCircularPoints("SpinnerCircularPoints", 16, 1.2, ImColor(255, 255, 255), 10.f * velocity, 7); ImGui::ShowTooltipOnHover("SpinnerCircularPoints"); break;
                    case 110: ImGui::SpinnerCurvedCircle("SpinnerCurvedCircle", 16, 1.2f, ImColor(255, 255, 255), 1.f * velocity, 3); ImGui::ShowTooltipOnHover("SpinnerCurvedCircle"); break;
                    case 111: ImGui::SpinnerModCircle("SpinnerModCirclre", 16, 1.2f, ImColor(255, 255, 255), 1.f, 2.f, 3.f * velocity); ImGui::ShowTooltipOnHover("SpinnerModCirclre"); break;
                    case 112: ImGui::SpinnerModCircle("SpinnerModCirclre2", 16, 1.2f, ImColor(255, 255, 255), 1.11f, 3.33f, 3.f * velocity); ImGui::ShowTooltipOnHover("SpinnerModCirclre2"); break;
                    case 113: ImGui::SpinnerPatternEclipse("SpinnerPatternEclipse", 16, 2, ImColor(255, 255, 255), 4.1f * velocity, 5, 2.f, 0.f); ImGui::ShowTooltipOnHover("SpinnerPatternEclipse"); break;
                    case 114: ImGui::SpinnerPatternEclipse("SpinnerPatternEclipse2", 16, 2, ImColor(255, 255, 255), 4.1f * velocity, 9, 4.f, 1.f); ImGui::ShowTooltipOnHover("SpinnerPatternEclipse2"); break;
                    case 115: ImGui::SpinnerMultiFadeDots("SpinnerMultiFadeDots", 16, 2, ImColor(255, 255, 255), 8 * velocity, 8); ImGui::ShowTooltipOnHover("SpinnerMultiFadeDots"); break;
                    case 116: ImGui::SpinnerRainbowShot("SpinnerRainbowShot", 16, 4, ImColor::HSV(0.25f, 0.8f, 0.8f, 0.f), 1.5f * velocity, 5); ImGui::ShowTooltipOnHover("SpinnerRainbowShot"); break;
                    case 117: ImGui::SpinnerSpiral("SpinnerSpiral", 16, 2, ImColor(255, 255, 255), 6 * velocity, 5); ImGui::ShowTooltipOnHover("SpinnerSpira"); break;
                    case 118: ImGui::SpinnerSpiralEye("SpinnerSpiralEye", 16, 1, ImColor(255, 255, 255), 3 * velocity); ImGui::ShowTooltipOnHover("SpinnerSpiralEye"); break;
                    case 119: ImGui::SpinnerWifiIndicator("SpinnerWifiIndicator", 16, 1.5f, ImColor(0, 0, 0), ImColor(255, 255, 255), 7.8f * velocity, 5.52f, 3); ImGui::ShowTooltipOnHover("SpinnerWifiIndicator"); break;
                    case 120: ImGui::SpinnerHboDots("SpinnerHboDots", 16, 2, ImColor(255, 255, 255), 0.f, 0.f, 1.1f * velocity, 10); ImGui::ShowTooltipOnHover("SpinnerHboDots"); break;
                    case 121: ImGui::SpinnerHboDots("SpinnerHboDots2", 16, 4, ImColor(255, 255, 255), 0.1f, 0.5f, 1.1f * velocity, 2); ImGui::ShowTooltipOnHover("SpinnerHboDots2"); break;
                    case 122: ImGui::SpinnerHboDots("SpinnerHboDots4", 16, 4, ImColor(255, 255, 255), 0.1f, 0.5f, 1.1f * velocity, 3); ImGui::ShowTooltipOnHover("SpinnerHboDots4"); break;
                    case 123: ImGui::SpinnerDnaDots("SpinnerDnaDotsH", 16, 3, ImColor(255, 255, 255), 2 * velocity, 8, 0.25f); ImGui::ShowTooltipOnHover("SpinnerDnaDotsH"); break;
                    case 124: ImGui::SpinnerDnaDots("SpinnerDnaDotsV", 16, 3, ImColor(255, 255, 255), 2 * velocity, 8, 0.25f, true); ImGui::ShowTooltipOnHover("SpinnerDnaDotsV"); break;
                    case 125: ImGui::SpinnerRotateDots("SpinnerRotateDots2", 16, 6, ImColor(255, 255, 255), 4 * velocity, ImMax<int>(int(ImSin((float)ImGui::GetTime() * 0.5f) * 8), 3)); ImGui::ShowTooltipOnHover("SpinnerRotateDots2"); break;
                    case 126: ImGui::SpinnerSevenSegments("SpinnerSevenSegments", "012345679ABCDEF", 16, 2, ImColor(255, 255, 255), 4 * velocity); ImGui::ShowTooltipOnHover("SpinnerSevenSegments"); break;
                    case 127: ImGui::SpinnerSolarBalls("SpinnerSolarBalls", 16, 4, ImColor(255, 0, 0), ImColor(255, 255, 255), 5 * velocity, 4); ImGui::ShowTooltipOnHover("SpinnerSolarBalls"); break;
                    case 128: ImGui::SpinnerSolarArcs("SpinnerSolarArcs", 16, 4, ImColor(255, 0, 0), ImColor(255, 255, 255), 5 * velocity, 4); ImGui::ShowTooltipOnHover("SpinnerSolarArcs"); break;
                    case 129: ImGui::SpinnerRainbow("Spinner", 16, 2, ImColor::HSV(++hue * 0.005f, 0.8f, 0.8f), 8 * velocity, 0.f, PI_2, 3); ImGui::ShowTooltipOnHover("Spinner"); break;
                    case 130: ImGui::SpinnerRotatingHeart("SpinnerRotatedHeart", 16, 2, ImColor(255, 0, 0), 8 * velocity, 0.f); ImGui::ShowTooltipOnHover("SpinnerRotatedHeart"); break;
                    case 131: ImGui::SpinnerSolarScaleBalls("SpinnerSolarScaleBalls", 16, 1.3f, ImColor(255, 0, 0), 1 * velocity, 36); ImGui::ShowTooltipOnHover("SpinnerSolarScaleBalls"); break;
                    case 132: ImGui::SpinnerOrionDots("SpinnerOrionDots", 16, 1.3f, ImColor(255, 255, 255), 4 * velocity, 12); ImGui::ShowTooltipOnHover("SpinnerOrionDots"); break;
                    case 133: ImGui::SpinnerGalaxyDots("SpinnerGalaxyDots", 16, 1.3f, ImColor(255, 255, 255), 0.2 * velocity, 6); ImGui::ShowTooltipOnHover("SpinnerGalaxyDots"); break;
                    case 134: ImGui::SpinnerAsciiSymbolPoints("SpinnerAsciiSymbolPoints", "012345679ABCDEF", 16, 2, ImColor(255, 255, 255), 4 * velocity); ImGui::ShowTooltipOnHover("SpinnerAsciiSymbolPoints"); break;
                    case 135: ImGui::SpinnerRainbowCircle("SpinnerRainbowCircle", 16, 4, ImColor::HSV(0.25f, 0.8f, 0.8f), 1 * velocity, 4); ImGui::ShowTooltipOnHover("SpinnerRainbowCircle"); break;
                    case 136: ImGui::SpinnerRainbowCircle("SpinnerRainbowCircle2", 16, 2, ImColor::HSV(hue * 0.001f, 0.8f, 0.8f), 2 * velocity, 8, 0); ImGui::ShowTooltipOnHover("SpinnerRainbowCircle2"); break;
                    case 137: ImGui::Spinner<e_st_vdots>("SpinnerVDots2", Radius{16}, Thickness{4}, Color{ImColor(255, 255, 255)}, BgColor{ImColor::HSV(hue * 0.0011f, 0.8f, 0.8f)}, Speed{2.1f * velocity}, Dots{2}, MiddleDots{6}); ImGui::ShowTooltipOnHover("SpinnerVDots2"); break;
                    case 138: ImGui::Spinner<e_st_vdots>("SpinnerVDots3", Radius{16}, Thickness{4}, Color{ImColor(255, 255, 255)}, BgColor{ImColor::HSV(hue * 0.0011f, 0.8f, 0.8f)}, Speed{2.9f * velocity}, Dots{3}, MiddleDots{6}); ImGui::ShowTooltipOnHover("SpinnerVDots3"); break;
                    case 139: ImGui::SpinnerSquareRandomDots("SpinnerSquareRandomDots", 16, 2.8f, ImColor(255, 255, 255, 30), ImColor::HSV(hue * 0.005f, 0.8f, 0.8f), 5 * velocity); ImGui::ShowTooltipOnHover("SpinnerSquareRandomDots"); break;
                    case 140: ImGui::SpinnerFluidPoints("SpinnerFluidPoints", 16, 2.8f, ImColor(0, 0, 255), 3.8f * velocity, Dots{4}, 0.45f); ImGui::ShowTooltipOnHover("SpinnerFluidPoints"); break;
                    case 141: ImGui::SpinnerDotsLoading("SpinnerDotsLoading", 16, 4.f, ImColor(255, 255, 255), ImColor(255, 255, 255, 124), 2.f * velocity); ImGui::ShowTooltipOnHover("SpinnerDotsLoading"); break;
                    case 142: ImGui::SpinnerDotsToPoints("SpinnerDotsToPoints", 16, 3, 0.5f, ImColor::HSV(0.31f, 0.8f, 0.8f), 1.8 * velocity, 5); ImGui::ShowTooltipOnHover("SpinnerDotsToPoints"); break;
                    case 143: ImGui::SpinnerThreeDots("SpinnerThreeDots", 16, 6, ImColor(255, 255, 255), 4 * velocity, 8); ImGui::ShowTooltipOnHover("SpinnerThreeDots"); break;
                    case 144: ImGui::Spinner4Caleidospcope("Spinner4Caleidospcope", 16, 6, ImColor::HSV(hue * 0.0031f, 0.8f, 0.8f), 4 * velocity, 8); ImGui::ShowTooltipOnHover("Spinner4Caleidospcope"); break;
                    case 145: ImGui::SpinnerFiveDots("SpinnerSixDots", 16, 6, ImColor(255, 255, 255), 4 * velocity, 8); ImGui::ShowTooltipOnHover("SpinnerSixDots"); break;
                    case 146: ImGui::SpinnerFillingMem("SpinnerFillingMem", 16, 6, ImColor::HSV(hue * 0.001f, 0.8f, 0.8f), spinner_filling_meb_bg, 4 * velocity); ImGui::ShowTooltipOnHover("SpinnerFillingMem"); break;
                    case 147: ImGui::SpinnerHerbertBalls("SpinnerHerbertBalls", 16, 2.3f, Color{ImColor(255, 255, 255)}, 2.f * velocity, 4); ImGui::ShowTooltipOnHover("SpinnerHerbertBalls"); break;
                    case 148: ImGui::SpinnerHerbertBalls3D("SpinnerHerbertBalls3D", 16, 3.f, Color{ImColor(255, 255, 255)}, 1.4f * velocity); ImGui::ShowTooltipOnHover("SpinnerHerbertBalls3D"); break;
                    case 149: ImGui::SpinnerSquareLoading("SpinnerSquareLoanding", 16, 2, Color{ImColor(255, 255, 255)}, 3 * velocity); ImGui::ShowTooltipOnHover("SpinnerSquareLoanding"); break;
                    case 150: ImGui::SpinnerTextFading("SpinnerTextFading", "Loading", 16, 12, ImColor::HSV(hue * 0.0011f, 0.8f, 0.8f), 4 * velocity); ImGui::ShowTooltipOnHover("SpinnerTextFading"); break;
                    case 151: ImGui::SpinnerBarChartAdvSine ("SpinnerBarChartAdvSine", 16, 5, ImColor(255, 255, 255), 4.8f * velocity, 0); ImGui::ShowTooltipOnHover("SpinnerBarChartAdvSine"); break;
                    case 152: ImGui::SpinnerBarChartAdvSineFade("SpinnerBarChartAdvSineFade", 16, 5, ImColor(255, 255, 255), 4.8f * velocity, 0); ImGui::ShowTooltipOnHover("SpinnerBarChartAdvSineFade"); break;
                    case 153: ImGui::SpinnerMovingArcs("SpinnerMovingArcs", 16, 4, ImColor(255, 255, 255), 2 * velocity, 4); ImGui::ShowTooltipOnHover("SpinnerMovingArcs"); break;
                    case 154: ImGui::SpinnerFadeTris("SpinnerFadeTris", 20, ImColor(255, 255, 255), 5.f * velocity, 2); ImGui::ShowTooltipOnHover("SpinnerFadeTris"); break;
                    case 155: ImGui::SpinnerBounceDots("SpinnerBounceDots", 16, 2.5, ImColor(255, 255, 255), 3 * velocity, 6, 1); ImGui::ShowTooltipOnHover("SpinnerBounceDots"); break;
                    case 156: ImGui::SpinnerRotateDots("SpinnerRotateDots", 16, 2, ImColor(255, 255, 255), 4 * velocity, 16, 1); ImGui::ShowTooltipOnHover("SpinnerRotateDots"); break;
                    case 157: ImGui::SpinnerTwinAng360("SpinnerTwinAng360", 16, 11, 2, ImColor(255, 255, 255), ImColor(255, 0, 0), 2.4f, 2.1f, 1); ImGui::ShowTooltipOnHover("SpinnerTwinAng360"); break;
                    case 158: ImGui::SpinnerAngTwin("SpinnerAngTwin1", 18, 13, 2, ImColor(255, 0, 0), ImColor(255, 255, 255), 3 * velocity, 1.3, 3, 1); ImGui::ShowTooltipOnHover("SpinnerAngTwin1"); break;
                    case 159: ImGui::SpinnerGooeyBalls("SpinnerGooeyBalls", 16, ImColor(255, 255, 255), 2.f * velocity, 1); ImGui::ShowTooltipOnHover("SpinnerGooeyBalls"); break;
                    case 160: ImGui::SpinnerArcRotation("SpinnerArcRotation", 13, 2.5, ImColor(255, 255, 255), 3 * velocity, 15, 1); ImGui::ShowTooltipOnHover("SpinnerArcRotation"); break;
                    case 161: ImGui::SpinnerAng("SpinnerAng90Gravity", 16, 1, ImColor(255, 255, 255), ImColor(255, 255, 255, 128), 8.f * velocity, PI_DIV_2, 1); ImGui::ShowTooltipOnHover("SpinnerAng90Gravity"); break;
                    case 162: ImGui::SpinnerAng("SpinnerAng90SinRad", 16, 1, ImColor(255, 255, 255), ImColor(255, 255, 255, 0), 8.f * velocity, 0.75f * PI_2, 2); ImGui::ShowTooltipOnHover("SpinnerAng90SinRad"); break;
                    case 163: ImGui::SpinnerSquishSquare("SpinnerSquishSquare", 16, ImColor(255, 255, 255), 8.f * velocity); ImGui::ShowTooltipOnHover("SpinnerSquishSquare"); break;
                    case 164: ImGui::SpinnerPulsarBall("SpinnerBounceBall", 16, 2, ImColor(255, 255, 255), 4 * velocity, 1); ImGui::ShowTooltipOnHover("SpinnerBounceBall"); break;
                    case 165: ImGui::SpinnerRainbowMix("SpinnerRainbowMix", 16, 2, ImColor::HSV(0.005f, 0.8f, 0.8f), 8 * velocity, 0.f, PI_2, 5, 1); ImGui::ShowTooltipOnHover("SpinnerRainbowMix"); break;
                    case 166: ImGui::SpinnerAngMix("SpinnerAngMix", 16, 1, ImColor(255, 255, 255), 8.f * velocity, IM_PI, 4, 0); ImGui::ShowTooltipOnHover("SpinnerAngMix"); break;
                    case 167: ImGui::SpinnerAngMix("SpinnerAngMixGravity", 16, 1, ImColor(255, 255, 255), 8.f * velocity, PI_DIV_2, 6, 1); ImGui::ShowTooltipOnHover("SpinnerAngMixGravity"); break;
                    case 168: ImGui::SpinnerScaleBlocks("SpinnerScaleBlocks", 16, 8, ImColor::HSV(hue * 0.005f, 0.8f, 0.8f), 5 * velocity, 1); ImGui::ShowTooltipOnHover("SpinnerScaleBlocks"); break;
                    case 169: ImGui::SpinnerFadeDots("SpinnerFadeDots3", 16, 6, ImColor(255, 255, 255), 8 * velocity, 4, 1); ImGui::ShowTooltipOnHover("SpinnerFadeDots3"); break;
                    case 170: ImGui::SpinnerFadeDots("SpinnerFadeDots6", 16, 3, ImColor(255, 255, 255), 8 * velocity, 4, 1); ImGui::ShowTooltipOnHover("SpinnerFadeDots6"); break;
                    case 171: ImGui::SpinnerFadeDots("SpinnerFadeDots2", 16, 2, ImColor(255, 255, 255), 5 * velocity, 8); ImGui::ShowTooltipOnHover("SpinnerFadeDots2"); break;
                    case 172: ImGui::SpinnerScaleDots("SpinnerScaleDots2", 16, 2, ImColor(255, 255, 255), 4 * velocity, 8); ImGui::ShowTooltipOnHover("SpinnerScaleDots2"); break;
                    case 173: ImGui::Spinner3SmuggleDots("Spinner3SmuggleDots", 16, 3, ImColor(255, 255, 255), 4 * velocity, 8,0.25f, true); ImGui::ShowTooltipOnHover("Spinner3SmuggleDots"); break;
                    case 174: ImGui::SpinnerSimpleArcFade("SpinnerSimpleArcFade", 13, 2, ImColor(255, 255, 255), 4 * velocity); ImGui::ShowTooltipOnHover("SpinnerSimpleArcFade"); break;
                    case 175: ImGui::SpinnerTwinHboDots("SpinnerTwinHboDots", 16, 4, ImColor(255, 255, 255), 0.1f, 0.5f, 1.1f * velocity, 6, 0.f); ImGui::ShowTooltipOnHover("SpinnerTwinHboDots"); break;
                    case 176: ImGui::SpinnerTwinHboDots("SpinnerTwinHboDots2", 16, 4, ImColor(255, 255, 255), 0.1f, 0.5f, 3.1f * velocity, 3, -0.5f); ImGui::ShowTooltipOnHover("SpinnerTwinHboDots2"); break;
                    case 177: ImGui::SpinnerThreeDotsStar("SpinnerThreeDotsStar", 16, 4, ImColor(255, 255, 255), 0.1f, 0.5f, 5.1f * velocity, -0.2f); ImGui::ShowTooltipOnHover("SpinnerThreeDotsStar"); break;
                    case 178: ImGui::SpinnerSquareSpins("SpinnerSquareSpins", 16, 6, ImColor(255, 255, 255), 2 * velocity); ImGui::ShowTooltipOnHover("SpinnerSquareSpins"); break;
                    case 179: ImGui::SpinnerMoonDots("SpinnerMoonDots", 16, 8, ImColor(255, 255, 255), ImColor(0, 0, 0), 1.1f * velocity); ImGui::ShowTooltipOnHover("SpinnerMoonDots"); break;
                    case 180: ImGui::SpinnerFilledArcFade("SpinnerFilledArcFade7", 16, ImColor(255, 255, 255), 6 * velocity, 6, 1); ImGui::ShowTooltipOnHover("SpinnerFilledArcFade7"); break;
                    case 181: ImGui::SpinnerRotateSegmentsPulsar("SpinnerRotateSegmentsPulsar", 16, 2, ImColor(255, 255, 255), 1.1f * velocity, 4, 2); ImGui::ShowTooltipOnHover("SpinnerRotateSegmentsPulsar"); break;
                    case 182: ImGui::SpinnerRotateSegmentsPulsar("SpinnerRotateSegmentsPulsar2", 16, 2, ImColor(255, 255, 255), 1.1f * velocity, 1, 3); ImGui::ShowTooltipOnHover("SpinnerRotateSegmentsPulsar2"); break;
                    case 183: ImGui::SpinnerRotateSegmentsPulsar("SpinnerRotateSegmentsPulsar3", 16, 2, ImColor(255, 255, 255), 1.1f * velocity, 3, 3); ImGui::ShowTooltipOnHover("SpinnerRotateSegmentsPulsar3"); break;
                    case 184: ImGui::SpinnerPointsArcBounce("SpinnerPointsArcBounce", 16, 2, ImColor(255, 255, 255), 3 * velocity, 12, 1, 0.f); ImGui::ShowTooltipOnHover("SpinnerPointsArcBounce"); break;
                    case 185: ImGui::SpinnerSomeScaleDots("SpinnerSomeScaleDots0", 16, 4, ImColor(255, 255, 255), 5.6f * velocity, 6, 0); ImGui::ShowTooltipOnHover("SpinnerSomeScaleDots0"); break;
                    case 186: ImGui::SpinnerSomeScaleDots("SpinnerSomeScaleDots1", 16, 4, ImColor(255, 255, 255), 6.6f * velocity, 6, 1); ImGui::ShowTooltipOnHover("SpinnerSomeScaleDots1"); break;
                    case 187: ImGui::SpinnerPointsArcBounce("SpinnerPointsArcBounce2", 16, 2, ImColor(255, 255, 255), 3 * velocity, 12, 1, 0.5f); ImGui::ShowTooltipOnHover("SpinnerPointsArcBounce2"); break;
                    case 188: ImGui::SpinnerPointsArcBounce("SpinnerPointsArcBounce3", 16, 2, ImColor(255, 255, 255), 3 * velocity, 12, 2, 0.3f); ImGui::ShowTooltipOnHover("SpinnerPointsArcBounce3"); break;
                    case 189: ImGui::SpinnerPointsArcBounce("SpinnerPointsArcBounce4", 16, 2, ImColor(255, 255, 255), 3 * velocity, 12, 3, 0.3f); ImGui::ShowTooltipOnHover("SpinnerPointsArcBounce4"); break;
                    case 190: ImGui::SpinnerTwinBlocks("SpinnerTwinBlocks", 16, 7, ImColor(255, 255, 255, 30), ImColor::HSV(hue * 0.005f, 0.8f, 0.8f), 5 * velocity); ImGui::ShowTooltipOnHover("SpinnerTwinBlocks"); break;
                    case 191: ImGui::SpinnerAng("SpinnerAng90", 16, 4, ImColor(255, 255, 255), ImColor(255, 255, 255, 128), 8.f * velocity, ImGui::PI_DIV_2, 3); ImGui::ShowTooltipOnHover("SpinnerAng90"); break;
                    case 192: ImGui::SpinnerSplineAng("SpinnerSplineAng90", 16, 2, ImColor(255, 255, 255), ImColor(255, 255, 255, 128), 8.f * velocity, ImGui::PI_DIV_2, 0); ImGui::ShowTooltipOnHover("SpinnerSplineAng90"); break;
                    case 193: ImGui::Spinner<e_st_ang>("SpinnerAngNoBg", Radius{16}, Thickness{2}, Color{ImColor(255, 255, 255)}, BgColor{ImColor(255, 255, 255, 0)}, Speed{6 * velocity}, Angle{IM_PI}, Mode{0}); ImGui::ShowTooltipOnHover("SpinnerAngNoBg"); break;
                    case 194: ImGui::SpinnerBounceDots("SpinnerBounceDots2", 16, 2.5, ImColor(255, 255, 255), 1 * velocity, 6, 2); ImGui::ShowTooltipOnHover("SpinnerBounceDots2"); break;
                    case 195: ImGui::SpinnerRotateDots("SpinnerRotateDots", 16, 1.3, ImColor(255, 255, 255), 4 * velocity, 6, 0); ImGui::ShowTooltipOnHover("SpinnerRotateDots"); break;
                    case 196: ImGui::SpinnerRotateDots("SpinnerRotateDots", 16, 2.3, ImColor(255, 255, 255), 4 * velocity, 5, 2); ImGui::ShowTooltipOnHover("SpinnerRotateDots3"); break;
                    case 197: ImGui::SpinnerTwinAng360("SpinnerTwinAng360", 16, 11, 4, ImColor(255, 255, 255), ImColor(255, 0, 0), 4 * velocity); ImGui::ShowTooltipOnHover("SpinnerTwinAng360"); break;
                    case 198: ImGui::SpinnerDots("SpinnerDotsWoBg2", &nextdot2, 16, 4, ImColor(255, 255, 255), 0.3f * velocity, 6, 1.49f, 0); ImGui::ShowTooltipOnHover("SpinnerDotsWoBg2"); break;
                    case 199: ImGui::SpinnerDots("SpinnerDotsWoBg3", &nextdot2, 16, 4, ImColor(255, 255, 255), 0.3f * velocity, 4, 1.49f, 1); ImGui::ShowTooltipOnHover("SpinnerDotsWoBg3"); break;
                    case 200: ImGui::SpinnerIncScaleDots("SpinnerIncScaleDots2", 16, 4, ImColor(255, 255, 255), 6.6f * velocity, 8, 1.22, 1); ImGui::ShowTooltipOnHover("SpinnerIncScaleDots2"); break;
                    case 201: ImGui::SpinnerPulsar("SpinnerPulsar2", 16, 2, ImColor(255, 255, 255), 1 * velocity, true, PI_2, 1); ImGui::ShowTooltipOnHover("SpinnerPulsar2"); break;
                    case 202: ImGui::SpinnerAngTwin("SpinnerAngTwin4", 16, 13, 2, ImColor(255, 0, 0), ImColor(255, 255, 255), 1.6f * velocity, 3.14, 1, 2); ImGui::ShowTooltipOnHover("SpinnerAngTwin4"); break;
                    case 203: ImGui::SpinnerAngTwin("SpinnerAngTwin5", 14, 16, 2, ImColor(255, 0, 0), ImColor(255, 255, 255), 0.8f * velocity, 1.57, 3, 2); ImGui::ShowTooltipOnHover("SpinnerAngTwin5"); break;
                    case 204: ImGui::Spinner<e_st_dots>("SpinnerDotsX3", Radius{16}, Thickness{2.3}, Color{ImColor(255, 255, 255)}, FloatPtr{&nextdot}, Speed{1 * velocity}, Dots{3}, MinThickness{-1.f}, Mode{2}); ImGui::ShowTooltipOnHover("SpinnerDotsX3"); break;
                    case 205: ImGui::Spinner<e_st_dots>("SpinnerDotsX13", Radius{16}, Thickness{2.3}, Color{ImColor(255, 255, 255)}, FloatPtr{&nextdot}, Speed{1 * velocity}, Dots{13}, MinThickness{-1.f}, Mode{2}); ImGui::ShowTooltipOnHover("SpinnerDotsX13"); break;
                    case 206: ImGui::Spinner<e_st_angle>("SpinnerAng", Radius{16}, Thickness{2}, Color{ImColor(255, 255, 255)}, BgColor{ImColor(255, 255, 255, 128)}, Speed{2.8f * velocity}, Angle{PI_DIV_2}, Mode{4}); ImGui::ShowTooltipOnHover("SpinnerAng"); break;
                    case 207: ImGui::SpinnerTwinAng180("SpinnerTwinAngX", 16, 12, 2, ImColor(255, 255, 255), ImColor(255, 0, 0), 0.5f * velocity, PI_DIV_4, 2); ImGui::ShowTooltipOnHover("SpinnerTwinAngX"); break;
                    case 208: ImGui::SpinnerAng8("SpinnerAng8", 21, 2, ImColor(255, 255, 255), 0, 8.f * velocity, PI_DIV_4 * 6, 3, 0.5f); ImGui::ShowTooltipOnHover("SpinnerAng8"); break;
                    case 209: ImGui::SpinnerAng8("SpinnerAng8.1", 14.5, 2.5, ImColor(255, 255, 255), 0, 4 * velocity, PI_DIV_4, 4, 0.5f); ImGui::ShowTooltipOnHover("SpinnerAng8.1"); break;
                    case 210: ImGui::SpinnerAng8("SpinnerAng8.2", 12, 2.5, ImColor(255, 255, 255), 0, 5 * velocity, 5.0f, 5, 0.75f); ImGui::ShowTooltipOnHover("SpinnerAng8.2"); break;
                    case 211: ImGui::SpinnerAng8("SpinnerAng8.3", 19, 2.5, ImColor(255, 255, 255), 0, 5 * velocity, 5.0f, 0, 0.70f); ImGui::ShowTooltipOnHover("SpinnerAng8.3"); break;
                    case 212: ImGui::SpinnerRotateDots("SpinnerRotateDots", 16, 3, ImColor(255, 255, 255), 4 * velocity, 4, 3); ImGui::ShowTooltipOnHover("SpinnerRotateDots"); break;
                    case 213: ImGui::Spinner<e_st_dots>("SpinnerDots/3", Radius{16}, Thickness{4}, Color{ImColor(255, 255, 255)}, FloatPtr{&nextdot}, Speed{1 * velocity}, Dots{12}, MinThickness{-1.f}, Mode{3}); ImGui::ShowTooltipOnHover("SpinnerDots/3"); break;
                    case 214: ImGui::Spinner<e_st_dots>("SpinnerDots/4", Radius{16}, Thickness{4}, Color{ImColor(255, 255, 255)}, FloatPtr{&nextdot}, Speed{1 * velocity}, Dots{12}, MinThickness{-1.f}, Mode{4}); ImGui::ShowTooltipOnHover("SpinnerDots/4"); break;
                    case 215: ImGui::Spinner<e_st_vdots>("SpinnerVDots/1", Radius{16}, Thickness{4}, Color{ImColor(255, 255, 255)}, BgColor{ImColor::HSV(hue * 0.0011f, 0.8f, 0.8f)}, Speed{2.7f * velocity}, Dots{12}, MiddleDots{7}, Mode{1}); ImGui::ShowTooltipOnHover("SpinnerVDots/1"); break;
                    case 216: ImGui::SpinnerSinSquares("SpinnerSinSquares/1", 16, 2, ImColor(255, 255, 255), 1.f * velocity, 1); ImGui::ShowTooltipOnHover("SpinnerSinSquares/1"); break;
                    case 217: ImGui::SpinnerSinSquares("SpinnerSinSquares/2", 16, 2, ImColor(255, 255, 255), 1.f * velocity, 2); ImGui::ShowTooltipOnHover("SpinnerSinSquares/2"); break;
                    case 218: ImGui::SpinnerCamera("SpinnerCamera/1", 16, 8, [] (int i) { return ImColor::HSV(i * 0.25f, 0.8f, 0.8f); }, 2.8f * velocity, 4, 1); ImGui::ShowTooltipOnHover("SpinnerCamera/1"); break;
                    case 219: ImGui::SpinnerCamera("SpinnerCamera/1", 16, 8, [] (int i) { return ImColor::HSV(i * 0.25f, 0.8f, 0.8f); }, 1.8f * velocity, 3, 1); ImGui::ShowTooltipOnHover("SpinnerCamera/1"); break;
                    case 220: ImGui::SpinnerRotateSegments("SpinnerRotateSegments/1", 16, 1.4, ImColor(255, 255, 255), 3 * velocity, 1, 4, 1); ImGui::ShowTooltipOnHover("SpinnerRotateSegments/1"); break;
                    case 221: ImGui::SpinnerRotateSegments("SpinnerRotateSegments/1", 16, 4, ImColor(255, 255, 255), 3 * velocity, 4, 1, 1); ImGui::ShowTooltipOnHover("SpinnerRotateSegments/1"); break;
                    case 222: ImGui::Spinner<e_st_angle>("SpinnerAng", Radius{16}, Thickness{2}, Color{ImColor(255, 255, 255)}, BgColor{ImColor(255, 255, 255, 128)}, Speed{8 * velocity}, Angle{IM_PI}, Mode{1}); ImGui::ShowTooltipOnHover("SpinnerAng"); break;
                    case 223: ImGui::SpinnerArcRotation("SpinnerArcRotation/3", Radius{16}, Thickness{2}, Color{ImColor(255, 255, 255)}, 3 * velocity, 4, 3); ImGui::ShowTooltipOnHover("SpinnerArcRotation/3"); break;
                    case 224: ImGui::SpinnerArcFade("SpinnerArcFade/2", Radius{13}, Thickness{5}, Color{ImColor(255, 255, 255)}, 3 * velocity, 4, 1); ImGui::ShowTooltipOnHover("SpinnerArcFade/2"); break;
                    case 225: ImGui::SpinnerIncScaleDots("SpinnerIncScaleDots", Radius{16}, Thickness{4}, Color{ImColor(255, 255, 255)}, 4.6f * velocity, 16, 1, 5); ImGui::ShowTooltipOnHover("SpinnerIncScaleDots"); break;
                    case 226: ImGui::SpinnerTwinPulsar("SpinnerTwinPulsar/5", 16, 2, ImColor(255, 255, 255), 0.5f * velocity, 5, 5); ImGui::ShowTooltipOnHover("SpinnerTwinPulsar/5"); break;
                    case 227: ImGui::SpinnerTwinPulsar("SpinnerTwinPulsar/0", 16, 2, ImColor(255, 255, 255), 0.5f * velocity, 5, 0); ImGui::ShowTooltipOnHover("SpinnerTwinPulsar/0"); break;
                    case 228: ImGui::SpinnerCircularLines("SpinnerCircularLines/4", 16, ImColor(255, 255, 255), 1.5f * velocity, 16, 4); ImGui::ShowTooltipOnHover("SpinnerCircularLines"); break;
                    case 229: ImGui::SpinnerRotatedAtom("SpinnerRotatedAtom/5", 16, 2, ImColor(255, 255, 255), 2.1f * velocity, 2, 5); ImGui::ShowTooltipOnHover("SpinnerRotatedAtom/5"); break;
                    case 230: ImGui::SpinnerRotatedAtom("SpinnerRotatedAtom/2", 16, 2, ImColor(255, 255, 255), 2.1f * velocity, 2, 1); ImGui::ShowTooltipOnHover("SpinnerRotatedAtom/2"); break;
                    case 231: ImGui::SpinnerRotatedAtom("SpinnerRotatedAtom/2", 16, 2, ImColor(255, 255, 255), 2.1f * velocity, 3, 5); ImGui::ShowTooltipOnHover("SpinnerRotatedAtom/2"); break;
                    case 232: ImGui::SpinnerRainbowBalls("SpinnerRainbowBalls/1", 16, 4, ImColor::HSV(0.25f, 0.8f, 0.8f, 0.f), 1.5f * velocity, 3, 1); ImGui::ShowTooltipOnHover("SpinnerRainbowBalls/1"); break;
                    case 233: ImGui::SpinnerRainbowBalls("SpinnerRainbowBalls/5", 16, 4, ImColor::HSV(0.25f, 0.8f, 0.8f, 0.f), 1.5f * velocity, 4, 5); ImGui::ShowTooltipOnHover("SpinnerRainbowBalls/5"); break;

                    // ...
                    default: break;
                }
                ImGui::PopID();
                ImGui::EndChild();
                if (x == sidex - 1) {
                    ImGui::Dummy({0, 0});
                }
            }       
        }
        style.ItemSpacing = lastSpacing;
        style.WindowPadding = lastPadding;    

        // End
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Pie Menu"))
    {
        // trigger logic and output
        const int pieMenuMouseButtonTrigger = ImGuiMouseButton_Left;    // Tweakable
        ImGui::Text("click this text for a Pie Menu!");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s","Based on code posted by @thennequin here:\nhttps://gist.github.com/thennequin/64b4b996ec990c6ddc13a48c6a0ba68c");
        const bool pieMenuTriggered = ImGui::IsItemHovered() && ImGui::IsMouseClicked(pieMenuMouseButtonTrigger);
        if (pieMenuTriggered)   ImGui::OpenPopup("PieMenu");

        // example usage
        static const char* pieSelected = NULL;  // used to display output
        if (ImGui::Pie::BeginPopup("PieMenu", pieMenuMouseButtonTrigger))    {
            pieSelected = "";
            if (ImGui::Pie::MenuItem("Test1")) {pieSelected="Test1";}
            if (ImGui::Pie::MenuItem("Test2")) {pieSelected="Test2";}
            if (ImGui::Pie::MenuItem("Test3")) {pieSelected="Test3";}
            if (ImGui::Pie::BeginMenu("Sub"))    {
                if (ImGui::Pie::BeginMenu("Sub sub\nmenu")) {
                    if (ImGui::Pie::MenuItem("SubSub")) {pieSelected="SubSub";}
                    if (ImGui::Pie::MenuItem("SubSub2")) {pieSelected="SubSub2";}
                    ImGui::Pie::EndMenu();
                }
                if (ImGui::Pie::MenuItem("TestSub")) {pieSelected="TestSub";}
                if (ImGui::Pie::MenuItem("TestSub2")) {pieSelected="TestSub2";}
                ImGui::Pie::EndMenu();
            }
            if (ImGui::Pie::BeginMenu("Sub2"))   {
                if (ImGui::Pie::MenuItem("TestSub")) {pieSelected="TestSub";}
                if (ImGui::Pie::BeginMenu("Sub sub\nmenu"))  {
                    if (ImGui::Pie::MenuItem("SubSub")) {pieSelected="SubSub";}
                    if (ImGui::Pie::MenuItem("SubSub2")) {pieSelected="SubSub2";}
                    ImGui::Pie::EndMenu();
                }
                if (ImGui::Pie::MenuItem("TestSub2")) {pieSelected="TestSub2";}
                ImGui::Pie::EndMenu();
            }
            ImGui::Pie::EndPopup();
        }
        if (pieSelected)    {
            //ImGui::SameLine();
            ImGui::Text("Last selected pie menu item: %s",strlen(pieSelected)==0?"NONE":pieSelected);
        }

        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Msg Box"))
    {
        static bool show_msgbox_window = false;
        static const char* buttons[] = { "Quit", "Cancel", NULL };
        static MsgBox msgbox;
        msgbox.Init("Message?", ICON_MD_WARNING, "Are you really really sure you want to open a message box?", buttons, false);
        ImGui::Checkbox("Open MsgBox", &show_msgbox_window);
        if (show_msgbox_window)
            ImGui::OpenPopup("Message?");
        int selected = msgbox.Draw();
        switch (selected)
        {
            case 0: break;// No button pressed
            case 1: show_msgbox_window = false; break;// First button pressed and so forth
            case 2: show_msgbox_window = false; break;// Second button pressed, and so forth...
            default: break;
        }

        ImGui::TreePop();
    }
	if (ImGui::TreeNode("Color Bands"))
	{
        ImGui::PushItemWidth(300);
		static float col[4] = { 1, 1, 1, 1 };
		ImGui::ColorEdit4("Color", col);
		float const width = 300;
		float const height = 32.0f;
		static float gamma = 1.0f;
		ImGui::DragFloat("Gamma##Color", &gamma, 0.01f, 0.1f, 10.0f);
		static int division = 32;
		ImGui::DragInt("Division", &division, 1, 1, 128);

		ImGui::Text("HueBand");
		ImGui::DrawHueBand(ImGui::GetCursorScreenPos(), ImVec2(width, height), division, col, col[3], gamma);
		ImGui::InvisibleButton("##Zone", ImVec2(width, height), 0);

		ImGui::Text("LuminanceBand");
		ImGui::DrawLumianceBand(ImGui::GetCursorScreenPos(), ImVec2(width, height), division, ImVec4(col[0], col[1], col[2], col[3]), gamma);
		ImGui::InvisibleButton("##Zone", ImVec2(width, height), 0);

		ImGui::Text("SaturationBand");
		ImGui::DrawSaturationBand(ImGui::GetCursorScreenPos(), ImVec2(width, height), division, ImVec4(col[0], col[1], col[2], col[3]), gamma);
		ImGui::InvisibleButton("##Zone", ImVec2(width, height), 0);

        ImGui::Text("ContrastBand");
		ImGui::DrawContrastBand(ImGui::GetCursorScreenPos(), ImVec2(width, height), ImVec4(col[0], col[1], col[2], col[3]));
		ImGui::InvisibleButton("##Zone", ImVec2(width, height), 0);

		ImGui::Separator();
		ImGui::Text("Custom Color Band");
		static int frequency = 6;
		ImGui::SliderInt("Frequency", &frequency, 1, 32);
		static float alpha = 1.0f;
		ImGui::SliderFloat("alpha", &alpha, 0.0f, 1.0f);

		float const fFrequency = frequency;
		float const fAlpha = alpha;
		ImGui::DrawColorBandEx< true >(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), ImVec2(width, height),
			[fFrequency, fAlpha](float const t)
			{
				float r = ImSign(ImSin(fFrequency * 2.0f * IM_PI * t + 2.0f * IM_PI * 0.0f / fFrequency)) * 0.5f + 0.5f;
				float g = ImSign(ImSin(fFrequency * 2.0f * IM_PI * t + 2.0f * IM_PI * 2.0f / fFrequency)) * 0.5f + 0.5f;
				float b = ImSign(ImSin(fFrequency * 2.0f * IM_PI * t + 2.0f * IM_PI * 4.0f / fFrequency)) * 0.5f + 0.5f;

				return IM_COL32(r * 255, g * 255, b * 255, fAlpha * 255);
			},
			division, gamma);
		ImGui::InvisibleButton("##Zone", ImVec2(width, height), 0);

        ImGui::PopItemWidth();
		ImGui::TreePop();
	}
    if (ImGui::TreeNode("Color Ring"))
	{
        ImGui::PushItemWidth(300);
		static int division = 32;
		float const width = 300;
		ImGui::SliderInt("Division", &division, 3, 128);
		static float colorOffset = 0;
		ImGui::SliderFloat("Color Offset", &colorOffset, 0.0f, 2.0f);
		static float thickness = 0.5f;
		ImGui::SliderFloat("Thickness", &thickness, 1.0f / width, 1.0f);

		ImDrawList* pDrawList = ImGui::GetWindowDrawList();
		{
			ImVec2 curPos = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##Zone", ImVec2(width, width), 0);

			ImGui::DrawColorRingEx< true >(pDrawList, curPos, ImVec2(width, width), thickness,
				[](float t)
				{
					float r, g, b;
					ImGui::ColorConvertHSVtoRGB(t, 1.0f, 1.0f, r, g, b);

					return IM_COL32(r * 255, g * 255, b * 255, 255);
				}, division, colorOffset);
		}
		static float center = 0.5f;
		ImGui::DragFloat("Center", &center, 0.01f, 0.0f, 1.0f);
		static float colorDotBound = 0.5f;
		ImGui::SliderFloat("Alpha Pow", &colorDotBound, -1.0f, 1.0f);
		static int frequency = 6;
		ImGui::SliderInt("Frequency", &frequency, 1, 32);
		{
			ImGui::Text("Nearest");
			ImVec2 curPos = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##Zone", ImVec2(width, width) * 0.5f, 0);

			float fCenter = center;
			float fColorDotBound = colorDotBound;
			ImGui::DrawColorRingEx< false >(pDrawList, curPos, ImVec2(width, width * 0.5f), thickness,
				[fCenter, fColorDotBound](float t)
				{
					float r, g, b;
					ImGui::ColorConvertHSVtoRGB(t, 1.0f, 1.0f, r, g, b);

					ImVec2 const v0(ImCos(t * 2.0f * IM_PI), ImSin(t * 2.0f * IM_PI));
					ImVec2 const v1(ImCos(fCenter * 2.0f * IM_PI), ImSin(fCenter * 2.0f * IM_PI));

					float const dot = ImDot(v0, v1);
					float const angle = ImAcos(dot) / IM_PI;// / width;

					return IM_COL32(r * 255, g * 255, b * 255, (dot > fColorDotBound ? 1.0f : 0.0f) * 255);
				}, division, colorOffset);
		}
		{
			ImGui::Text("Custom");
			ImVec2 curPos = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##Zone", ImVec2(width, width) * 0.5f, 0);

			float const fFreq = (float)frequency;
			ImGui::DrawColorRingEx< true >(pDrawList, curPos, ImVec2(width, width) * 0.5f, thickness,
				[fFreq](float t)
				{
					float v = ImSign(ImCos(fFreq * 2.0f * IM_PI * t)) * 0.5f + 0.5f;

					return IM_COL32(v * 255, v * 255, v * 255, 255);
				}, division, colorOffset);
		}
        ImGui::PopItemWidth();
		ImGui::TreePop();
	}
    if (ImGui::TreeNode("Color Selector"))
	{
        ImGui::PushItemWidth(300);
		float const width = 300;
		float const height = 32.0f;
		static float offset = 0.0f;
		static int division = 32;
        static float gamma = 1.0f;
		ImGui::DragInt("Division", &division, 1.0f, 2, 256);
        ImGui::DragFloat("Gamma##Color", &gamma, 0.01f, 0.1f, 10.0f);
		static float alphaHue = 1.0f;
		static float alphaHideHue = 0.125f;
		ImGui::DragFloat("Offset##ColorSelector", &offset, 0.0f, 0.0f, 1.0f);
		ImGui::DragFloat("Alpha Hue", &alphaHue, 0.0f, 0.0f, 1.0f);
		ImGui::DragFloat("Alpha Hue Hide", &alphaHideHue, 0.0f, 0.0f, 1.0f);
		static float hueCenter = 0.5f;
		static float hueWidth = 0.1f;
		static float featherLeft = 0.125f;
		static float featherRight = 0.125f;
		ImGui::DragFloat("featherLeft", &featherLeft, 0.0f, 0.0f, 0.5f);
		ImGui::DragFloat("featherRight", &featherRight, 0.0f, 0.0f, 0.5f);
		
        ImGui::Spacing();
        ImGui::TextUnformatted("Hue:"); ImGui::SameLine();
        ImGui::HueSelector("Hue Selector", ImVec2(width, height), &hueCenter, &hueWidth, &featherLeft, &featherRight, 0.5f, 1.0f, division, alphaHue, alphaHideHue, offset);
		
        static bool rgb_color = false;
        ImGui::Checkbox("RGB Color Bar", &rgb_color);
        ImGui::Spacing();
        static float lumianceCenter = 0.0f;
        ImGui::TextUnformatted("Lum:"); ImGui::SameLine();
        ImGui::LumianceSelector("Lumiance Selector", ImVec2(width, height), &lumianceCenter, 0.0f, -1.f, 1.f, 1.0f, division, gamma, rgb_color);

        ImGui::Spacing();
        static float saturationCenter = 0.0f;
        ImGui::TextUnformatted("Sat:"); ImGui::SameLine();
        ImGui::SaturationSelector("Saturation Selector", ImVec2(width, height), &saturationCenter, 0.0f, -1.f, 1.f, 1.0f, division, gamma, rgb_color);
        
        ImGui::Spacing();
        static float contrastCenter = 1.0f;
        ImGui::TextUnformatted("Con:"); ImGui::SameLine();
        ImGui::ContrastSelector("Contrast Selector", ImVec2(width, height), &contrastCenter, 1.0f, 1.0f, rgb_color);

        ImGui::Spacing();
        static float gammaCenter = 1.0f;
        ImGui::TextUnformatted("Gma:"); ImGui::SameLine();
        ImGui::GammaSelector("Gamma Selector", ImVec2(width, height), &gammaCenter, 1.0f, 0.f, 4.f, 1.0f);

        ImGui::Spacing();
        static float temperatureCenter = 5000.0f;
        ImGui::TextUnformatted("Tmp:"); ImGui::SameLine();
        ImGui::TemperatureSelector("TemperatureSelector Selector", ImVec2(width, height), &temperatureCenter, 5000.0f, 2000.f, 8000.f, 1.0f);

        ImGui::Spacing();
        static ImVec4 rgba = ImVec4(0.0, 0.0, 0.0, 0.0);
        ImGui::BalanceSelector("Balance Selector", ImVec2(width / 2, height), &rgba, ImVec4(0, 0, 0, 0));

        ImGui::PopItemWidth();
        ImGui::TreePop();
	}
    if (ImGui::TreeNode("Slider Select"))
    {
        ImGui::PushItemWidth(300);

        static ImVec2 val2d(0.f, 0.f);
        static ImVec4 val3d(0.f, 0.f, 0.f, 0.f);
        ImGui::InputVec2("Vec2D", &val2d, ImVec2(-1.f, -1.f), ImVec2(1.f, 1.f));
        ImGui::SameLine();
        ImGui::InputVec3("Vec3D", &val3d, ImVec4(-1.f, -1.f, -1.f, -1.f), ImVec4(1.f, 1.f, 1.f, 1.f));
        ImGui::SameLine();
        static ImVec2 min(-0.5f, -0.5f);
		static ImVec2 max(0.5f, 0.5f);
		ImGui::RangeSelect2D("Range Select 2D", &min.x, &min.y, &max.x, &max.y, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f);

        ImGui::PopItemWidth();
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Bezier Select"))
    { 
        static float v[5] = { 0.950f, 0.050f, 0.795f, 0.035f }; 
        ImGui::BezierSelect("##easeInExpo", ImVec2(200, 200), v);
        ImGui::TreePop();
    }
    if ( ImGui::TreeNode( "Interactions" ) )
	{
        if ( ImGui::TreeNode( "Poly Convex Hovered" ) )
		{
            //float const size = ImGui::GetContentRegionAvail().x;
            float const size = 64;
			ImDrawList* pDrawList = ImGui::GetWindowDrawList();
			ImVec2 pos = ImGui::GetCursorScreenPos();
            std::vector<ImVec2> pos_norms = { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } };
            for ( ImVec2& v : pos_norms )
			{
				v.x *= size;
				v.y *= size;
				v += pos;
			}
            ImGui::SetCursorScreenPos(pos);
            bool hovered = IsMouseHoveringPolyConvex( pos, pos + ImVec2( size, size ), pos_norms);
			pDrawList->AddConvexPolyFilled( pos_norms.data(), 3, IM_COL32( hovered ? 255 : 0, hovered ? 0 : 255, 0, 255 ) );
            ImGui::Dummy( ImVec2( size, size ) );

            ImGui::SameLine();
            pos = ImGui::GetCursorScreenPos();
			std::vector<ImVec2> disk;
			disk.resize( 32 );
			for ( int k = 0; k < 32; ++k )
			{
				float angle = ( ( float )k ) * 2.0f * IM_PI / 32.0f;
				float cos0 = ImCos( angle );
				float sin0 = ImSin( angle );
				disk[ k ].x = pos.x + 0.5f * size + cos0 * size * 0.5f;
				disk[ k ].y = pos.y + 0.5f * size + sin0 * size * 0.5f;
			}
            hovered = IsMouseHoveringPolyConvex( pos, pos + ImVec2( size, size ), disk);
			pDrawList->AddConvexPolyFilled( disk.data(), 32, IM_COL32( hovered ? 255 : 0, hovered ? 0 : 255, 0, 255 ) );
			ImGui::Dummy( ImVec2( size, size ) );
            ImGui::TreePop();
        }
        if ( ImGui::TreeNode( "Poly Concave Hovered" ) )
		{
            float const size = 64;
			ImDrawList* pDrawList = ImGui::GetWindowDrawList();
			ImVec2 pos = ImGui::GetCursorScreenPos();
			int sz = 8;
            std::vector<ImVec2> pos_norms = { { 0.0f, 0.0f }, { 0.3f, 0.0f }, { 0.3f, 0.7f }, { 0.7f, 0.7f }, { 0.7f, 0.0f },
                                            { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
			for ( int k = 0; k < sz; ++k )
			{
				ImVec2& v = pos_norms[ k ];
				v.x *= size;
				v.y *= size;
				v += pos;
			}
            bool hovered = IsMouseHoveringPolyConcave( pos * 0.99f, pos + ImVec2( 1.01f * size, 1.01f * size ), pos_norms );
			pDrawList->AddConcavePolyFilled( pos_norms.data(), sz, IM_COL32( hovered ? 255 : 0, hovered ? 0 : 255, 0, 255 ) );
            ImGui::Dummy( ImVec2( size, size ) );

            ImGui::SameLine();
            pos = ImGui::GetCursorScreenPos();
            std::vector<ImVec2> ring;
			sz = 64;
			ring.resize( sz );
			srand( 97 );
			for ( int k = 0; k < sz; ++k )
			{
				float angle = -( ( float )k ) * 2.0f * IM_PI / 32.0f;
				float cos0 = ImCos( angle );
				float sin0 = ImSin( angle );
				float r = ( float )( rand() % ( ( int )roundf( size ) ) );
				ring[ k ].x = pos.x + size * 0.5f + r * 0.5f * cos0;
				ring[ k ].y = pos.y + size * 0.5f + r * 0.5f * sin0;
			}
			hovered = IsMouseHoveringPolyConcave( pos * 0.99f, pos + ImVec2( 1.01f * size, 1.01f * size ), ring );
			pDrawList->AddConcavePolyFilled( ring.data(), sz, IM_COL32( hovered ? 255 : 0, hovered ? 0 : 255, 0, 255 ) );
			ImGui::Dummy( ImVec2( size, size ) );

            ImGui::TreePop();
        }
        if ( ImGui::TreeNode( "Poly With Hole Hovered" ) )
		{
			float const size = 64;
			ImDrawList* pDrawList = ImGui::GetWindowDrawList();
			ImVec2 pos = ImGui::GetCursorScreenPos();
			int sz = 10;
			std::vector<ImVec2> pos_norms = { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f },
                                            { 0.3f, 0.3f }, { 0.7f, 0.3f }, { 0.7f, 0.7f }, { 0.3f, 0.7f }, { 0.3f, 0.3f } };
			for ( int k = 0; k < sz; ++k )
			{
				ImVec2& v = pos_norms[ k ];
				v.x *= size;
				v.y *= size;
				v += pos;
			}
			bool hovered = IsMouseHoveringPolyWithHole( pos * 0.99f, pos + ImVec2( 1.01f * size, 1.01f * size ), pos_norms );
			DrawShapeWithHole( pDrawList, pos_norms, IM_COL32( hovered ? 255 : 0, hovered ? 0 : 255, 0, 255 ) );
			ImGui::Dummy( ImVec2( size, size ) );
            ImGui::SameLine();
			pos = ImGui::GetCursorScreenPos();
			std::vector<ImVec2> ring;
			sz = 64;
			ring.resize( sz );
			float r;
			for ( int k = 0; k < 32; ++k )
			{
				float angle = -( ( float )k ) * 2.0f * IM_PI / 31.0f;
				float cos0 = ImCos( angle );
				float sin0 = ImSin( angle );
				r = size * ( ( ( float )( rand() % 1000 ) / 1000.0f ) * 0.25f + 0.75f );
				ring[ k ].x = pos.x + size * 0.5f + r * 0.5f * cos0;
				ring[ k ].y = pos.y + size * 0.5f + r * 0.5f * sin0;
			}
			srand( 97 );
			for ( int k = 32; k < 64; ++k )
			{
				float angle = ( ( float )( k - 32 ) ) * 2.0f * IM_PI / 31.0f;
				float cos0 = ImCos( angle );
				float sin0 = ImSin( angle );
				r = size * 0.75f * ( ( ( float )( rand() % 1000 ) / 1000.0f ) * 0.5f + 0.5f );
				ring[ k ].x = pos.x + size * 0.5f + r * 0.5f * cos0;
				ring[ k ].y = pos.y + size * 0.5f + r * 0.5f * sin0;
			}
			hovered = IsMouseHoveringPolyWithHole( pos, pos + ImVec2( size, size ), ring );
			DrawShapeWithHole( pDrawList, ring, IM_COL32( hovered ? 255 : 0, hovered ? 0 : 255, 0, 255 ) );
			ImGui::Dummy( ImVec2( size, size ) );
            ImGui::TreePop();
		}
        ImGui::TreePop();
    }
    if ( ImGui::TreeNode( "Triangles Pointers" ) )
	{
		float const width = ImGui::GetContentRegionAvail().x;

		static float angle = 0.0f;
		static float size = 16.0f;
		static float thickness = 1.0f;
		ImGui::SliderAngle( "Angle##Triangle", &angle );
		ImGui::SliderFloat( "Size##Triangle", &size, 1.0f, 64.0f );
		ImGui::SliderFloat( "Thickness##Triangle", &thickness, 1.0f, 5.0f );

		ImVec2 curPos = ImGui::GetCursorScreenPos();
		ImDrawList* pDrawList = ImGui::GetWindowDrawList();
		ImGui::InvisibleButton( "##Zone", ImVec2( width, 96.0f ), 0 );
		ImGui::InvisibleButton( "##Zone", ImVec2( width, 96.0f ), 0 );
		float fPointerLine = 64.0f;
		pDrawList->AddLine( ImVec2( curPos.x + 0.5f * 32.0f, curPos.y + fPointerLine ), ImVec2( curPos.x + 3.5f * 32.0f, curPos.y + fPointerLine ), IM_COL32( 0, 255, 0, 255 ), 2.0f );
		pDrawList->AddLine( ImVec2( curPos.x + 5.0f * 32.0f, curPos.y ), ImVec2( curPos.x + 5.0f * 32.0f, curPos.y + 72.0f ), IM_COL32( 0, 255, 0, 255 ), 2.0f );
		pDrawList->AddLine( ImVec2( curPos.x + 7.0f * 32.0f, curPos.y ), ImVec2( curPos.x + 7.0f * 32.0f, curPos.y + 72.0f ), IM_COL32( 0, 255, 0, 255 ), 2.0f );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 1.0f * 32.0f, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 3.0f * 32.0f, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 5.0f * 32.0f, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 7.0f * 32.0f, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
		ImGui::DrawTriangleCursor( pDrawList, ImVec2( curPos.x + 1.0f * 32.0f, curPos.y + fPointerLine ), angle, size, thickness, IM_COL32( 255, 0, 0, 255 ) );
		ImGui::DrawTriangleCursor( pDrawList, ImVec2( curPos.x + 3.0f * 32.0f, curPos.y + fPointerLine ), angle, size, thickness, IM_COL32( 255, 0, 0, 255 ) );
		ImGui::DrawTriangleCursor( pDrawList, ImVec2( curPos.x + 5.0f * 32.0f, curPos.y + fPointerLine ), angle, size, thickness, IM_COL32( 255, 0, 0, 255 ) );
		ImGui::DrawTriangleCursor( pDrawList, ImVec2( curPos.x + 7.0f * 32.0f, curPos.y + fPointerLine ), angle, size, thickness, IM_COL32( 255, 0, 0, 255 ) );

		fPointerLine *= 3.0f;
		pDrawList->AddLine( ImVec2( curPos.x + 0.5f * 32.0f, curPos.y + fPointerLine ), ImVec2( curPos.x + 3.5f * 32.0f, curPos.y + fPointerLine ), IM_COL32( 0, 255, 0, 255 ), 2.0f );
		pDrawList->AddLine( ImVec2( curPos.x + 5.0f * 32.0f, curPos.y ), ImVec2( curPos.x + 5.0f * 32.0f, curPos.y + fPointerLine ), IM_COL32( 0, 255, 0, 255 ), 2.0f );
		pDrawList->AddLine( ImVec2( curPos.x + 7.0f * 32.0f, curPos.y ), ImVec2( curPos.x + 7.0f * 32.0f, curPos.y + fPointerLine ), IM_COL32( 0, 255, 0, 255 ), 2.0f );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 1.0f * 32.0f, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 3.0f * 32.0f, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 5.0f * 32.0f, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 7.0f * 32.0f, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
		ImGui::DrawTriangleCursorFilled( pDrawList, ImVec2( curPos.x + 1.0f * 32.0f, curPos.y + fPointerLine ), angle, size, IM_COL32( 255, 0, 0, 255 ) );
		ImGui::DrawTriangleCursorFilled( pDrawList, ImVec2( curPos.x + 3.0f * 32.0f, curPos.y + fPointerLine ), angle, size, IM_COL32( 255, 0, 0, 255 ) );
		ImGui::DrawTriangleCursorFilled( pDrawList, ImVec2( curPos.x + 5.0f * 32.0f, curPos.y + fPointerLine ), angle, size, IM_COL32( 255, 0, 0, 255 ) );
		ImGui::DrawTriangleCursorFilled( pDrawList, ImVec2( curPos.x + 7.0f * 32.0f, curPos.y + fPointerLine ), angle, size, IM_COL32( 255, 0, 0, 255 ) );
        ImGui::TreePop();
	}
    if ( ImGui::TreeNode( "Signet Pointer" ) )
	{
		float const widthZone = ImGui::GetContentRegionAvail().x;

		static float angle = 0.0f;
		static float width = 16.0f;
		static float height = 21.0f;
		static float height_ratio = 1.0f / 3.0f;
		static float align01 = 0.5f;
		static float thickness = 5.0f;
		ImGui::SliderAngle( "Angle##Triangle", &angle );
		ImGui::SliderFloat( "Width##Triangle", &width, 1.0f, 64.0f );
		ImGui::SliderFloat( "Height##Triangle", &height, 1.0f, 128.0f );
		ImGui::SliderFloat( "Array Ratio##Triangle", &height_ratio, 0.0f, 1.0f );
		ImGui::SliderFloat( "Align##Triangle", &align01, 0.0f, 1.0f );
		ImGui::SliderFloat( "Thickness##Triangle", &thickness, 1.0f, 16.0f );

		ImVec2 curPos = ImGui::GetCursorScreenPos();
		ImDrawList* pDrawList = ImGui::GetWindowDrawList();
		ImGui::InvisibleButton( "##Zone", ImVec2( widthZone, height * 1.1f ), 0 );
		ImGui::InvisibleButton( "##Zone", ImVec2( widthZone, height * 1.1f ), 0 );
		float fPointerLine = 32.0f;
		float dx = 16.0f;
		pDrawList->AddLine( ImVec2( curPos.x + 0.5f * dx, curPos.y + fPointerLine ), ImVec2( curPos.x + 11.5f * dx, curPos.y + fPointerLine ), IM_COL32( 0, 255, 0, 255 ), 2.0f );
		ImVec4 vBlue( 91.0f / 255.0f, 194.0f / 255.0f, 231.0f / 255.0f, 1.0f );
		ImU32 uBlue = ImGui::GetColorU32( vBlue );
		ImGui::DrawSignetCursor( pDrawList, ImVec2( curPos.x + 1.0f * dx, curPos.y + fPointerLine ), width, height, height_ratio, align01, angle, thickness, uBlue );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 1.0f * dx, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
		ImGui::DrawSignetFilledCursor( pDrawList, ImVec2( curPos.x + 3.0f * dx, curPos.y + fPointerLine ), width, height, height_ratio, align01, angle, uBlue );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 3.0f * dx, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
		ImGui::DrawSignetCursor( pDrawList, ImVec2( curPos.x + 5.0f * dx, curPos.y + fPointerLine ), width, height, height_ratio, 0.0f, angle, thickness, uBlue );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 5.0f * dx, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
		ImGui::DrawSignetFilledCursor( pDrawList, ImVec2( curPos.x + 7.0f * dx, curPos.y + fPointerLine ), width, height, height_ratio, 0.0f, angle, uBlue );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 7.0f * dx, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
		ImGui::DrawSignetCursor( pDrawList, ImVec2( curPos.x + 9.0f * dx, curPos.y + fPointerLine ), width, height, height_ratio, 1.0f, angle, thickness, uBlue );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 9.0f * dx, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
		ImGui::DrawSignetFilledCursor( pDrawList, ImVec2( curPos.x + 11.0f * dx, curPos.y + fPointerLine ), width, height, height_ratio, 1.0f, angle, uBlue );
		pDrawList->AddCircleFilled( ImVec2( curPos.x + 11.0f * dx, curPos.y + fPointerLine ), 4.0f, IM_COL32( 255, 128, 0, 255 ), 16 );
        ImGui::TreePop();
	}
    if ( ImGui::TreeNode( "Linear Line Graduation" ) )
	{
		float const size = 256;
		ImDrawList* pDrawList = ImGui::GetWindowDrawList();
		static float mainLineThickness = 1.0f;
		static ImU32 mainCol = IM_COL32( 255, 255, 255, 255 );
		static int division0 = 3;  static float height0 = 32.0f; static float thickness0 = 5.0f; static float angle0 = 0; static ImU32 col0 = IM_COL32( 255, 0, 0, 255 );
		static int division1 = 5;  static float height1 = 16.0f; static float thickness1 = 2.0f; static float angle1 = 0; static ImU32 col1 = IM_COL32( 0, 255, 0, 255 );
		static int division2 = 10; static float height2 = 8.0f;  static float thickness2 = 1.0f; static float angle2 = 0; static ImU32 col2 = IM_COL32( 255, 255, 0, 255 );
		static int divisions[] = { division0, division1, division2 };
		static float heights[] = { height0, height1, height2 };
		static float thicknesses[] = { thickness0, thickness1, thickness2 };
		static float angles[] = { angle0, angle1, angle2 };
		static ImVec4 colors[] = { ImGui::ColorConvertU32ToFloat4( col0 ), ImGui::ColorConvertU32ToFloat4( col1 ), ImGui::ColorConvertU32ToFloat4( col2 ) };

		ImGui::DragFloat( "Main Thickness", &mainLineThickness, 1.0f, 1.0f, 16.0f );
		ImVec4 vMainCol = ImGui::ColorConvertU32ToFloat4( mainCol );
		if ( ImGui::ColorEdit3( "Main", &vMainCol.x ) )
			mainCol = ImGui::GetColorU32( vMainCol );

		ImGui::DragInt3( "Divisions", &divisions[ 0 ], 1.0f, 1, 10 );
		ImGui::DragFloat3( "Heights", &heights[ 0 ], 1.0f, 1.0f, 128.0f );
		ImGui::DragFloat3( "Thicknesses", &thicknesses[ 0 ], 1.0f, 1.0f, 16.0f );
		ImGui::PushMultiItemsWidths( 3, ImGui::CalcItemWidth() );
		ImGui::SliderAngle( "a0", &angles[ 0 ] ); ImGui::SameLine();
		ImGui::SliderAngle( "a1", &angles[ 1 ] ); ImGui::SameLine();
		ImGui::SliderAngle( "a2", &angles[ 2 ] );
		ImGui::PushMultiItemsWidths( 3, ImGui::CalcItemWidth() );
		if ( ImGui::ColorEdit3( "c0", &colors[ 0 ].x ) )
			col0 = ImGui::GetColorU32( colors[ 0 ] );
		ImGui::SameLine();
		if ( ImGui::ColorEdit3( "c1", &colors[ 1 ].x ) )
			col1 = ImGui::GetColorU32( colors[ 1 ] );
		ImGui::SameLine();
		if ( ImGui::ColorEdit3( "c2", &colors[ 2 ].x ) )
			col2 = ImGui::GetColorU32( colors[ 2 ] );

		float height = ImMax( heights[ 0 ], ImMax( heights[ 1 ], heights[ 2 ] ) );
		ImVec2 pos = ImGui::GetCursorScreenPos() + ImVec2( 0.0f, height );
		DrawLinearLineGraduation( pDrawList, pos, pos + ImVec2( size, 0.0f ),
								mainLineThickness, mainCol,
								divisions[ 0 ], heights[ 0 ], thicknesses[ 0 ], angles[ 0 ], col0,
								divisions[ 1 ], heights[ 1 ], thicknesses[ 1 ], angles[ 1 ], col1,
								divisions[ 2 ], heights[ 2 ], thicknesses[ 2 ], angles[ 2 ], col2 );
		ImGui::Dummy( ImVec2( size, height ) );
		DrawLinearLineGraduation( pDrawList, pos, pos + ImVec2( size, size ),
								mainLineThickness, mainCol,
								divisions[ 0 ], heights[ 0 ], thicknesses[ 0 ], angles[ 0 ], col0,
								divisions[ 1 ], heights[ 1 ], thicknesses[ 1 ], angles[ 1 ], col1,
								divisions[ 2 ], heights[ 2 ], thicknesses[ 2 ], angles[ 2 ], col2 );
		ImGui::Dummy( ImVec2( size, size ) );
        ImGui::TreePop();
	}
    if ( ImGui::TreeNode( "Log Line Graduation" ) )
	{
		float const size = 256;
		ImDrawList* pDrawList = ImGui::GetWindowDrawList();
		static float mainLineThickness = 1.0f;
		static ImU32 mainCol = IM_COL32( 255, 255, 255, 255 );
		static int division0 = 3;  static float height0 = 32.0f; static float thickness0 = 5.0f; static float angle0 = 0; static ImU32 col0 = IM_COL32( 255, 0, 0, 255 );
		static int division1 = 10;  static float height1 = 16.0f; static float thickness1 = 2.0f; static float angle1 = 0; static ImU32 col1 = IM_COL32( 0, 255, 0, 255 );
		static int divisions[] = { division0, division1 };
		static float heights[] = { height0, height1 };
		static float thicknesses[] = { thickness0, thickness1 };
		static float angles[] = { angle0, angle1  };
		static ImVec4 colors[] = { ImGui::ColorConvertU32ToFloat4( col0 ), ImGui::ColorConvertU32ToFloat4( col1 ) };

		ImGui::DragFloat( "Main Thickness", &mainLineThickness, 1.0f, 1.0f, 16.0f );
		ImVec4 vMainCol = ImGui::ColorConvertU32ToFloat4( mainCol );
		if ( ImGui::ColorEdit3( "Main", &vMainCol.x ) )
			mainCol = ImGui::GetColorU32( vMainCol );

		ImGui::DragInt2( "Divisions", &divisions[ 0 ], 1.0f, 1, 20 );
		ImGui::DragFloat2( "Heights", &heights[ 0 ], 1.0f, 1.0f, 128.0f );
		ImGui::DragFloat2( "Thicknesses", &thicknesses[ 0 ], 1.0f, 1.0f, 16.0f );
		ImGui::PushMultiItemsWidths( 2, ImGui::CalcItemWidth() );
		ImGui::SliderAngle( "a0", &angles[ 0 ] ); ImGui::SameLine();
		ImGui::SliderAngle( "a1", &angles[ 1 ] ); ImGui::SameLine();
		ImGui::SliderAngle( "a2", &angles[ 2 ] );
		ImGui::PushMultiItemsWidths( 2, ImGui::CalcItemWidth() );
		if ( ImGui::ColorEdit3( "c0", &colors[ 0 ].x ) )
			col0 = ImGui::GetColorU32( colors[ 0 ] );
		ImGui::SameLine();
		if ( ImGui::ColorEdit3( "c1", &colors[ 1 ].x ) )
			col1 = ImGui::GetColorU32( colors[ 1 ] );

		float height = ImMax( heights[ 0 ], heights[ 1 ] );
		ImVec2 pos = ImGui::GetCursorScreenPos() + ImVec2( 0.0f, height );
		DrawLogLineGraduation( pDrawList, pos, pos + ImVec2( size, 0.0f ),
							mainLineThickness, mainCol,
							divisions[ 0 ], heights[ 0 ], thicknesses[ 0 ], angles[ 0 ], col0,
							divisions[ 1 ], heights[ 1 ], thicknesses[ 1 ], angles[ 1 ], col1 );
		ImGui::Dummy( ImVec2( size, height ) );
		DrawLogLineGraduation( pDrawList, pos, pos + ImVec2( size, size ),
							mainLineThickness, mainCol,
							divisions[ 0 ], heights[ 0 ], thicknesses[ 0 ], angles[ 0 ], col0,
							divisions[ 1 ], heights[ 1 ], thicknesses[ 1 ], angles[ 1 ], col1 );
		ImGui::Dummy( ImVec2( size, size ) );
        ImGui::TreePop();
	}
    if ( ImGui::TreeNode( "Linear Circular Graduation" ) )
	{
		float const size = 256;
		ImDrawList* pDrawList = ImGui::GetWindowDrawList();
		static float mainLineThickness = 1.0f;
		static ImU32 mainCol = IM_COL32( 255, 255, 255, 255 );
		static int division0 = 3;  static float height0 = 32.0f; static float thickness0 = 5.0f; static float angle0 = 0; static ImU32 col0 = IM_COL32( 255, 0, 0, 255 );
		static int division1 = 5;  static float height1 = 16.0f; static float thickness1 = 2.0f; static float angle1 = 0; static ImU32 col1 = IM_COL32( 0, 255, 0, 255 );
		static int division2 = 10; static float height2 = 8.0f;  static float thickness2 = 1.0f; static float angle2 = 0; static ImU32 col2 = IM_COL32( 255, 255, 0, 255 );
		static int divisions[] = { division0, division1, division2 };
		static float heights[] = { height0, height1, height2 };
		static float thicknesses[] = { thickness0, thickness1, thickness2 };
		static float angles[] = { angle0, angle1, angle2 };
		static float start_angle = -IM_PI / 3.0f;
		static float end_angle = 4.0f * IM_PI / 3.0f;
		static float angles_bound[] = { start_angle, end_angle };
		static float radius = size * 0.5f - 2.0f * ImMax( height0, ImMax( height1, height2 ) );
		static int num_segments = 0;
		static ImVec4 colors[] = { ImGui::ColorConvertU32ToFloat4( col0 ), ImGui::ColorConvertU32ToFloat4( col1 ), ImGui::ColorConvertU32ToFloat4( col2 ) };

		ImGui::DragFloat( "Main Thickness", &mainLineThickness, 1.0f, 1.0f, 16.0f );
		ImVec4 vMainCol = ImGui::ColorConvertU32ToFloat4( mainCol );
		if ( ImGui::ColorEdit3( "Main", &vMainCol.x ) )
			mainCol = ImGui::GetColorU32( vMainCol );

		ImGui::DragInt3( "Divisions", &divisions[ 0 ], 1.0f, 1, 10 );
		ImGui::DragFloat3( "Heights", &heights[ 0 ], 1.0f, 1.0f, 128.0f );
		ImGui::DragFloat3( "Thicknesses", &thicknesses[ 0 ], 1.0f, 1.0f, 16.0f );
		ImGui::DragFloat( "Radius", &radius, 1.0f, 1.0f, size );
		ImGui::DragInt( "Segment", &num_segments, 1.0f, 0, 64 );
		ImGui::PushMultiItemsWidths( 2, ImGui::CalcItemWidth() );
		ImGui::SliderAngle( "start angle", &angles_bound[ 0 ], -360.0f, angles_bound[ 1 ] * 180.0f / IM_PI ); ImGui::SameLine();
		ImGui::SliderAngle( "end angle", &angles_bound[ 1 ], angles_bound[ 0 ] * 180.0f / IM_PI, 360.0f );
		ImGui::PushMultiItemsWidths( 3, ImGui::CalcItemWidth() );
		ImGui::SliderAngle( "a0", &angles[ 0 ] ); ImGui::SameLine();
		ImGui::SliderAngle( "a1", &angles[ 1 ] ); ImGui::SameLine();
		ImGui::SliderAngle( "a2", &angles[ 2 ] );
		ImGui::PushMultiItemsWidths( 3, ImGui::CalcItemWidth() );
		if ( ImGui::ColorEdit3( "c0", &colors[ 0 ].x ) )
			col0 = ImGui::GetColorU32( colors[ 0 ] );
		ImGui::SameLine();
		if ( ImGui::ColorEdit3( "c1", &colors[ 1 ].x ) )
			col1 = ImGui::GetColorU32( colors[ 1 ] );
		ImGui::SameLine();
		if ( ImGui::ColorEdit3( "c2", &colors[ 2 ].x ) )
			col2 = ImGui::GetColorU32( colors[ 2 ] );
		ImVec2 pos = ImGui::GetCursorScreenPos();
		DrawLinearCircularGraduation( pDrawList, pos + ImVec2( size * 0.5f, size * 0.5f ), radius, angles_bound[ 0 ], angles_bound[ 1 ], num_segments,
									mainLineThickness, mainCol,
									divisions[ 0 ], heights[ 0 ], thicknesses[ 0 ], angles[ 0 ], col0,
									divisions[ 1 ], heights[ 1 ], thicknesses[ 1 ], angles[ 1 ], col1,
									divisions[ 2 ], heights[ 2 ], thicknesses[ 2 ], angles[ 2 ], col2 );
		ImGui::Dummy( ImVec2( size, size ) );
        ImGui::TreePop();
	}
    if ( ImGui::TreeNode( "Log Circular Graduation" ) )
	{
		float const size = 256;
		ImDrawList* pDrawList = ImGui::GetWindowDrawList();
		static float mainLineThickness = 1.0f;
		static ImU32 mainCol = IM_COL32( 255, 255, 255, 255 );
		static int division0 =  3;  static float height0 = 32.0f; static float thickness0 = 5.0f; static float angle0 = 0; static ImU32 col0 = IM_COL32( 255, 0, 0, 255 );
		static int division1 = 10;  static float height1 = 16.0f; static float thickness1 = 2.0f; static float angle1 = 0; static ImU32 col1 = IM_COL32( 0, 255, 0, 255 );
		static int divisions[] = { division0, division1 };
		static float heights[] = { height0, height1 };
		static float thicknesses[] = { thickness0, thickness1 };
		static float angles[] = { angle0, angle1 };
		static float start_angle = -IM_PI / 3.0f;
		static float end_angle = 4.0f * IM_PI / 3.0f;
		static float angles_bound[] = { start_angle, end_angle };
		static float radius = size * 0.5f - 2.0f * ImMax( height0, height1 );
		static int num_segments = 0;
		static ImVec4 colors[] = { ImGui::ColorConvertU32ToFloat4( col0 ), ImGui::ColorConvertU32ToFloat4( col1 ) };

		ImGui::DragFloat( "Main Thickness", &mainLineThickness, 1.0f, 1.0f, 16.0f );
		ImVec4 vMainCol = ImGui::ColorConvertU32ToFloat4( mainCol );
		if ( ImGui::ColorEdit3( "Main", &vMainCol.x ) )
			mainCol = ImGui::GetColorU32( vMainCol );

		ImGui::DragInt2( "Divisions", &divisions[ 0 ], 1.0f, 1, 20 );
		ImGui::DragFloat2( "Heights", &heights[ 0 ], 1.0f, 1.0f, 128.0f );
		ImGui::DragFloat2( "Thicknesses", &thicknesses[ 0 ], 1.0f, 1.0f, 16.0f );
		ImGui::DragFloat( "Radius", &radius, 1.0f, 1.0f, size );
		ImGui::DragInt( "Segment", &num_segments, 1.0f, 0, 64 );
		ImGui::PushMultiItemsWidths( 2, ImGui::CalcItemWidth() );
		ImGui::SliderAngle( "start angle", &angles_bound[ 0 ], -360.0f, angles_bound[ 1 ] * 180.0f / IM_PI ); ImGui::SameLine();
		ImGui::SliderAngle( "end angle", &angles_bound[ 1 ], angles_bound[ 0 ] * 180.0f / IM_PI, 360.0f );
		ImGui::PushMultiItemsWidths( 2, ImGui::CalcItemWidth() );
		ImGui::SliderAngle( "a0", &angles[ 0 ] ); ImGui::SameLine();
		ImGui::SliderAngle( "a1", &angles[ 1 ] );
		ImGui::PushMultiItemsWidths( 2, ImGui::CalcItemWidth() );
		if ( ImGui::ColorEdit3( "c0", &colors[ 0 ].x ) )
			col0 = ImGui::GetColorU32( colors[ 0 ] );
		ImGui::SameLine();
		if ( ImGui::ColorEdit3( "c1", &colors[ 1 ].x ) )
			col1 = ImGui::GetColorU32( colors[ 1 ] );

		float height = ImMax( heights[ 0 ], heights[ 1 ] );
		ImVec2 pos = ImGui::GetCursorScreenPos() + ImVec2( 0.0f, height );
		DrawLogCircularGraduation( pDrawList, pos + ImVec2( size * 0.5f, size * 0.5f ), radius, angles_bound[ 0 ], angles_bound[ 1 ], num_segments,
								   mainLineThickness, mainCol,
								   divisions[ 0 ], heights[ 0 ], thicknesses[ 0 ], angles[ 0 ], col0,
								   divisions[ 1 ], heights[ 1 ], thicknesses[ 1 ], angles[ 1 ], col1 );
		ImGui::Dummy( ImVec2( size, size ) );
        ImGui::TreePop();
	}
    if (ImGui::TreeNode("virtual keyboard##VK"))
    {
        static int keyboardLogicalLayoutIndex = ImGui::KLL_QWERTY;
        static int keyboardPhysicalLayoutIndex = ImGui::KPL_ISO;
        static ImGui::VirtualKeyboardFlags virtualKeyboardFlags = ImGui::VirtualKeyboardFlags_ShowAllBlocks; // ShowAllBlocks displays all the keyboard parts
        static ImGuiKey lastKeyReturned = ImGuiKey_COUNT;

        const float w = ImGui::GetContentRegionAvail().x*0.15f;
        ImGui::SetNextItemWidth(w);
        ImGui::Combo("Logical Layout##VK",&keyboardLogicalLayoutIndex,ImGui::GetKeyboardLogicalLayoutNames(),ImGui::KLL_COUNT);
        ImGui::SameLine(0.f,w);
        ImGui::SetNextItemWidth(w);
        ImGui::Combo("Physical Layout##VK",&keyboardPhysicalLayoutIndex,ImGui::GetKeyboardPhysicalLayoutNames(),ImGui::KPL_COUNT);

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*0.35f);

        static const char* tooltips[6] = {"ShowBaseBlock","ShowFunctionBlock","ShowArrowBlock","ShowKeypadBlock","NoMouseInteraction","NoKeyboardInteraction"};
        int flagHovered = -1;
        if (ImGui::CheckboxFlags("Flags##VKFlags",&virtualKeyboardFlags,6,1,1,0,&flagHovered)) lastKeyReturned = ImGuiKey_COUNT;;
        if (flagHovered>=0 && flagHovered<6) ImGui::SetTooltip("hold SHIFT to toggle the \"%s\" flag",tooltips[flagHovered]);

        ImGui::Spacing();

        ImGuiKey keyReturned =  ImGuiKey_COUNT;

        const ImVec2 childWindowSize(0,350.f*ImGui::GetFontSize()/18.f);
        ImGui::SetNextWindowSize(childWindowSize);
        if (ImGui::BeginChild("VirtualKeyBoardChildWindow##VK",childWindowSize,false,ImGuiWindowFlags_HorizontalScrollbar)) {
            // Draw an arbitrary keyboard layout to visualize translated keys
            keyReturned = ImGui::VirtualKeyboard(virtualKeyboardFlags,(ImGui::KeyboardLogicalLayout) keyboardLogicalLayoutIndex,(ImGui::KeyboardPhysicalLayout) keyboardPhysicalLayoutIndex);
            // ---------------------------------------------------------------
        }
        ImGui::EndChild();
        if (keyReturned!=ImGuiKey_COUNT) lastKeyReturned = keyReturned;
        if (lastKeyReturned!=ImGuiKey_COUNT) ImGui::Text("Last returned ImGuiKey: \"%s\"",lastKeyReturned==ImGuiKey_None?"NONE":ImGui::GetKeyName(lastKeyReturned));

        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Splitter windows"))
    {
        float h = 200;
        static float hsz1 = 300;
        static float hsz2 = 300;
        static float vsz1 = 100;
        static float vsz2 = 100;
        ImGui::Splitter(true, 8.0f, &hsz1, &hsz2, 8, 8, h);
        ImGui::BeginChild("1", ImVec2(hsz1, h), true);
            ImGui::Text("Window 1");
        ImGui::EndChild();
        ImGui::SameLine();

        ImGui::BeginChild("2", ImVec2(hsz2, h), true);
            ImGui::Splitter(false, 8.0f, &vsz1, &vsz2, 8, 8, hsz2);
            ImGui::BeginChild("3", ImVec2(hsz2, vsz1), false);
                ImGui::Text("Window 2");
            ImGui::EndChild();
            ImGui::BeginChild("4", ImVec2(hsz2, vsz2), false);
                ImGui::Text("Window 3");
            ImGui::EndChild();
        ImGui::EndChild();

        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Drag Drop Test"))
    {
        ImGui::InvisibleButton("Drag_drop extra test", ImVec2(128, 128));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILES"))
            {
                ImGui::TextUnformatted((const char*)payload->Data);
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::TreePop();
    }
}

void ShowImKalmanDemoWindow()
{
    ImGuiIO& io = ImGui::GetIO();
    int64_t t1, t2, predicted_time, update_time;
    int state_size = 4;
    int measurement_size   = 2;
    static int noise_covariance_pow = 5;
    static int measurement_noise_covariance_pow = 1;
    static ImKalman kalman(state_size, measurement_size);
    static ImMat measurement;
    static bool first_step = true;
    measurement.create_type(1, measurement_size, IM_DT_FLOAT32);
    if (first_step)
    {
        kalman.covariance(1.f / (pow(10, noise_covariance_pow)), 1.f / (pow(10, measurement_noise_covariance_pow)));
        first_step = false;
    }

    //1.kalman previous state
    ImVec2 statePt = ImVec2(kalman.statePre.at<float>(0, 0), kalman.statePre.at<float>(0, 1));
    //2.kalman prediction
    t1 = ImGui::get_current_time_usec();
    ImMat prediction  = kalman.predicted();
    t2 = ImGui::get_current_time_usec();
    predicted_time = t2 - t1;
    ImVec2 predictPt = ImVec2(prediction.at<float>(0, 0), prediction.at<float>(0, 1));
    //3. kalman update
    measurement.at<float>(0, 0) = io.MousePos.x;
    measurement.at<float>(0, 1) = io.MousePos.y;
    t1 = ImGui::get_current_time_usec();
    kalman.update(measurement);
    t2 = ImGui::get_current_time_usec();
    update_time = t2 - t1;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddCircle(statePt, 3, IM_COL32(255,0,0,255));
    drawList->AddCircle(predictPt, 3, IM_COL32(0,255,0,255));
    drawList->AddCircle(io.MousePos, 3, IM_COL32(255,255,255,255));
#if defined(__APPLE__) || defined(_WIN32)
    ImGui::Text("Predicted time: %lldus", predicted_time);
    ImGui::Text("   Update time: %lldus", update_time);
#else
    ImGui::Text("Predicted time: %ldus", predicted_time);
    ImGui::Text("   Update time: %ldus", update_time);
#endif
    if (ImGui::SliderInt("noise covariance pow", &noise_covariance_pow, 1, 5))
    {
        kalman.covariance(1.f / (pow(10, noise_covariance_pow)), 1.f / (pow(10, measurement_noise_covariance_pow)));
    }
    if (ImGui::SliderInt("measurement noise covariance pow", &measurement_noise_covariance_pow, 1, 5))
    {
        kalman.covariance(1.f / (pow(10, noise_covariance_pow)), 1.f / (pow(10, measurement_noise_covariance_pow)));
    }
}

void ShowImFFTDemoWindow()
{
#define FFT_DATA_LENGTH 1024
#define SUB_LENGTH (FFT_DATA_LENGTH / 4)
    ImGuiIO &io = ImGui::GetIO();
    static float wave_scale = 1.0f;
    static float fft_scale = 1.0f;
    static float x_scale = 1.0f;
    ImMat time_domain;
    time_domain.create_type(FFT_DATA_LENGTH, IM_DT_FLOAT32);
    static int signal_type = 0;
    static const char * signal_item[] = {"Sine", "Composition", "Square wave", "Triangular Wave", "Sawtooth wave"};
    static int view_type = 0;
    static const char * view_item[] = {"FFT", "Amplitude", "Phase", "DB"};
    static ImGui::ImMat spectrogram;
    static ImTextureID spectrogram_texture = nullptr;
    ImGui::PushItemWidth(200);
    if (ImGui::Combo("Signal Type", &signal_type, signal_item, IM_ARRAYSIZE(signal_item)))
    {
        spectrogram.release();
    }
    ImGui::SameLine();
    ImGui::Combo("View Type", &view_type, view_item, IM_ARRAYSIZE(view_item));
    ImGui::PopItemWidth();
    // init time domain data
    switch (signal_type)
    {
        case 0: // Sine
        {
            for (int i = 0; i < FFT_DATA_LENGTH; i++)
            {
                float t = (float)i / (float)FFT_DATA_LENGTH;
                time_domain.at<float>(i) = 0.5 * sin(2 * M_PI * 100 * t);
            }
        }
        break;
        case 1: // Composition
        {
            for (int i = 0; i < FFT_DATA_LENGTH; i++)
            {
                float t = (float)i / (float)FFT_DATA_LENGTH;
                time_domain.at<float>(i) = 0.2 * sin(2 * M_PI * 100 * t) + 0.3 * sin(2 * M_PI * 200 * t) + 0.4 * sin(2 * M_PI * 300 * t);
            }
        }
        break;
        case 2: // Square wave
        {
            int sign = 1;
            float step = 0;
            float t = (float)FFT_DATA_LENGTH / 50.f;
            for (int i = 0; i < FFT_DATA_LENGTH; i++)
            {
                step ++; if (step >= t) { step = 0; sign = -sign; }
                time_domain.at<float>(i) = 0.5 * sign;
            }
        }
        break;
        case 3: // Triangular Wave
        {
            int sign = 1;
            float step = -1;
            float t = 50.f / (float)FFT_DATA_LENGTH;
            for (int i = 0; i < FFT_DATA_LENGTH; i++)
            {
                step += t * sign; if (step >= 1.0 || step <= -1.0) sign = -sign;
                time_domain.at<float>(i) = 0.5 * step;
            }
        }
        break;
        case 4: // Sawtooth wave
        {
            float step = -1;
            float t = 50.f / (float)FFT_DATA_LENGTH;
            for (int i = 0; i < FFT_DATA_LENGTH; i++)
            {
                step += t; if (step >= 1.0) step = -1;
                time_domain.at<float>(i) = 0.5 * step;
            }
        }
        break;
        default: break;
    }

    // spectrogram
    if (spectrogram.empty())
    {
        ImGui::ImSpectrogram(time_domain, spectrogram, 128);
        ImGui::ImDestroyTexture(&spectrogram_texture);
        ImGui::ImMatToTexture(spectrogram, spectrogram_texture);
    }
    // init frequency domain data
    ImGui::ImMat frequency_domain;
    frequency_domain.clone_from(time_domain);

    // do fft
    ImRFFT((float *)frequency_domain.data, frequency_domain.w, true);

    ImGui::ImMat amplitude;
    amplitude.create_type((FFT_DATA_LENGTH >> 1) + 1, IM_DT_FLOAT32);
    ImGui::ImReComposeAmplitude((float*)frequency_domain.data, (float *)amplitude.data, FFT_DATA_LENGTH);

    ImGui::ImMat phase;
    phase.create_type((FFT_DATA_LENGTH >> 1) + 1, IM_DT_FLOAT32);
    ImGui::ImReComposePhase((float*)frequency_domain.data, (float *)phase.data, FFT_DATA_LENGTH);

    ImGui::ImMat db;
    db.create_type((FFT_DATA_LENGTH >> 1) + 1, IM_DT_FLOAT32);
    ImGui::ImReComposeDB((float*)frequency_domain.data, (float *)db.data, FFT_DATA_LENGTH, false);

    // do ifft
    ImMat time_domain_out;
    time_domain_out.clone_from(frequency_domain);
    ImRFFT((float *)time_domain_out.data, time_domain_out.w, false);

    ImGui::BeginChild("FFT Result");
    // draw result
    ImVec2 channel_view_size = ImVec2(1024 * x_scale, 128);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.f));

    // draw time domain
    if (ImPlot::BeginPlot("##time_domain", channel_view_size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
    {
        ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxesLimits(0, time_domain.w, -1.0 * wave_scale, 1.0 * wave_scale, ImGuiCond_Always);
        ImPlot::PlotLine("##TimeDomain", (float *)time_domain.data, time_domain.w);
        ImPlot::EndPlot();
    }
    if (ImGui::IsItemHovered())
    {
        if (io.MouseWheel < -FLT_EPSILON) { wave_scale *= 0.9f; if (wave_scale < 0.1f) wave_scale = 0.1f; }
        if (io.MouseWheel >  FLT_EPSILON) { wave_scale *= 1.1f; if (wave_scale > 4.0f) wave_scale = 4.0f; }
        if (io.MouseWheelH < -FLT_EPSILON) { x_scale *= 0.9f; if (x_scale < 1.0f) x_scale = 1.0f; }
        if (io.MouseWheelH >  FLT_EPSILON) { x_scale *= 1.1f; if (x_scale > 16.0f) x_scale = 16.0f; }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { wave_scale = 1.0; x_scale = 1.0; }
    }

    // draw spectrogram
    if (spectrogram_texture)
    {
        ImGui::Image(spectrogram_texture, channel_view_size);
    }

    // draw frequency domain data
    float * fft_data = nullptr;
    int data_count = 0;
    double f_min = 0.f;
    double f_max = 1.f;
    switch (view_type)
    {
        case 0:
            fft_data = (float *)frequency_domain.data; data_count = frequency_domain.w;
            f_min = -8.f; f_max = 8.f;
        break;
        case 1:
            fft_data = (float *)amplitude.data; data_count = amplitude.w;
            f_min = 0.f; f_max = 32.f;
        break;
        case 2:
            fft_data = (float *)phase.data; data_count = phase.w;
            f_min = -180.f; f_max = 180.f;
        break;
        case 3:
            fft_data = (float *)db.data; data_count = db.w;
            f_min = 0.f; f_max = 32.f;
        break;
        default: break;
    }

    if (ImPlot::BeginPlot("##fft_domain", channel_view_size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
    {
        ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxesLimits(0, data_count, f_min * fft_scale, f_max * fft_scale, ImGuiCond_Always);
        ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.75f);
        ImPlot::PlotShaded("##FFTDomainAMP", (float *)fft_data, data_count);
        ImPlot::PopStyleVar();
        ImPlot::PlotLine("##FFTDomain", (float *)fft_data, data_count);
        ImPlot::EndPlot();
    }
    if (ImGui::IsItemHovered())
    {
        if (io.MouseWheel < -FLT_EPSILON) { fft_scale *= 0.9f; if (fft_scale < 0.1f) fft_scale = 0.1f; }
        if (io.MouseWheel >  FLT_EPSILON) { fft_scale *= 1.1f; if (fft_scale > 4.0f) fft_scale = 4.0f; }
        if (io.MouseWheelH < -FLT_EPSILON) { x_scale *= 0.9f; if (x_scale < 1.0f) x_scale = 1.0f; }
        if (io.MouseWheelH >  FLT_EPSILON) { x_scale *= 1.1f; if (x_scale > 16.0f) x_scale = 16.0f; }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { fft_scale = 1.0; x_scale = 1.0f; }
    }

    // draw time domain out(ifft)
    if (ImPlot::BeginPlot("##time_domain_out", channel_view_size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
    {
        ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxesLimits(0, time_domain_out.w, -1.0 * wave_scale, 1.0 * wave_scale, ImGuiCond_Always);
        ImPlot::PlotLine("##TimeDomainOut", (float *)time_domain_out.data, time_domain_out.w);
        ImPlot::EndPlot();
    }
    if (ImGui::IsItemHovered())
    {
        if (io.MouseWheel < -FLT_EPSILON) { wave_scale *= 0.9f; if (wave_scale < 0.1f) wave_scale = 0.1f; }
        if (io.MouseWheel >  FLT_EPSILON) { wave_scale *= 1.1f; if (wave_scale > 4.0f) wave_scale = 4.0f; }
        if (io.MouseWheelH < -FLT_EPSILON) { x_scale *= 0.9f; if (x_scale < 1.0f) x_scale = 1.0f; }
        if (io.MouseWheelH >  FLT_EPSILON) { x_scale *= 1.1f; if (x_scale > 16.0f) x_scale = 16.0f; }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { wave_scale = 1.0; x_scale = 1.0; }
    }

    ImGui::PopStyleColor();
    ImGui::EndChild();
}

void ShowImSTFTDemoWindow()
{
#if defined(__EMSCRIPTEN__)
#define STFT_DATA_LENGTH (32*128)
#else
#define STFT_DATA_LENGTH (256*128)
#endif
#define STFT_SUB_LENGTH (STFT_DATA_LENGTH / 4)
    ImGuiIO &io = ImGui::GetIO();
    static float wave_scale = 1.0f;
    static float fft_scale = 16.0f;
    static float x_scale = 1.0f;
    const float rate = 44100.f;
    ImMat time_domain;
    time_domain.create_type(STFT_DATA_LENGTH, IM_DT_FLOAT32);
    static int signal_type = 0;
    static const char * signal_item[] = { "Sine", "Sweep", "Inverse Sweep", "Segmentation", "Inverse Segmentation", "High-Frequency Disturbing" };
    static int fft_type = 0;
    static const char * fft_item[] = { "FFT", "STFT" };
    static ImGui::ImMat spectrogram;
    static ImTextureID spectrogram_texture = nullptr;
    ImGui::PushItemWidth(200);
    if (ImGui::Combo("Signal Type", &signal_type, signal_item, IM_ARRAYSIZE(signal_item)))
    {
        spectrogram.release();
    }
    ImGui::SameLine();
    if (ImGui::Combo("Spectrogram Type", &fft_type, fft_item, IM_ARRAYSIZE(fft_item)))
    {
        spectrogram.release();
    }
    ImGui::PopItemWidth();

    // init time domain data
    switch (signal_type)
    {
        case 0: // Sine
        {
            for (int i = 0; i < STFT_DATA_LENGTH; i++)
            {
                float t = (float)i / rate;
                time_domain.at<float>(i) = 0.5 * sin(2 * M_PI * 1000 * t);
            }
        }
        break;
        case 1: // Sweep
        {
            float f = 0.f;
            float fstep = 10000.f / (float)STFT_DATA_LENGTH;
            for (int i = 0; i < STFT_DATA_LENGTH; i++)
            {
                float t = (float)i / rate;
                time_domain.at<float>(i) = 0.5 * sin(2 * M_PI * f * t);
                f += fstep;
            }
        }
        break;
        case 2: // Inverse Sweep
        {
            float f = 0.f;
            float step = 10000.f / (float)STFT_DATA_LENGTH;
            for (int i = 0; i < STFT_DATA_LENGTH; i++)
            {
                float t = (float)i / rate;
                time_domain.at<float>(STFT_DATA_LENGTH - i - 1) = 0.5 * sin(2 * M_PI * f * t);
                f += step;
            }
        }
        break;
        case 3: // Segmentation
        {
            for (int i = 0; i < STFT_SUB_LENGTH; i++)
            {
                float t = (float)i / rate;
                time_domain.at<float>(i + STFT_SUB_LENGTH * 0) = 0.1 * sin(2 * M_PI * 1000 * t);
                time_domain.at<float>(i + STFT_SUB_LENGTH * 1) = 0.2 * sin(2 * M_PI * 2000 * t);
                time_domain.at<float>(i + STFT_SUB_LENGTH * 2) = 0.3 * sin(2 * M_PI * 3000 * t);
                time_domain.at<float>(i + STFT_SUB_LENGTH * 3) = 0.4 * sin(2 * M_PI * 4000 * t);
            }
        }
        break;
        case 4: // Inverse Segmentation
        {
            for (int i = 0; i < STFT_SUB_LENGTH; i++)
            {
                float t = (float)i / rate;
                time_domain.at<float>(i + STFT_SUB_LENGTH * 0) = 0.4 * sin(2 * M_PI * 4000 * t);
                time_domain.at<float>(i + STFT_SUB_LENGTH * 1) = 0.3 * sin(2 * M_PI * 3000 * t);
                time_domain.at<float>(i + STFT_SUB_LENGTH * 2) = 0.2 * sin(2 * M_PI * 2000 * t);
                time_domain.at<float>(i + STFT_SUB_LENGTH * 3) = 0.1 * sin(2 * M_PI * 1000 * t);
            }
        }
        break;
        case 5: // High-Frequency Disturbing
        {
            for (int i = 0; i < STFT_DATA_LENGTH; i++)
            {
                float t = (float)i / rate;
                if (i > STFT_DATA_LENGTH / 2 - 40 && i < STFT_DATA_LENGTH / 2 + 40)
                {
                    time_domain.at<float>(i) = 0.2 * sin(2 * M_PI * 40 * t) + 0.4 * sin(2 * M_PI * 80 * t) + 0.3 * sin(2 * M_PI * 800 * t);
                }
                else
                {
                    time_domain.at<float>(i) = 0.2 * sin(2 * M_PI * 40 * t) + 0.4 * sin(2 * M_PI * 80 * t);
                }
            }
        }
        default: break;
    }

    // spectrogram
    if (spectrogram.empty())
    {
        if (fft_type == 0)
            ImGui::ImSpectrogram(time_domain, spectrogram, 2048);
        else
            ImGui::ImSpectrogram(time_domain, spectrogram, 512, true, 128);
        ImGui::ImDestroyTexture(&spectrogram_texture);
        ImGui::ImMatToTexture(spectrogram, spectrogram_texture);
    }
    
    // init STFT
    const int window = STFT_DATA_LENGTH / 4;
    const int hope = window / 4;
    const int overlap = window - hope;
    ImGui::ImSTFT process(window, hope);
    ImGui::ImMat short_time_domain, padding_data;
    short_time_domain.create_type(window + 2, IM_DT_FLOAT32);
    padding_data.create_type(window, IM_DT_FLOAT32);
    ImGui::ImMat short_time_domain_out;
    short_time_domain_out.create_type(STFT_DATA_LENGTH, IM_DT_FLOAT32);
    ImGui::ImMat short_time_amplitude;
    short_time_amplitude.create_type((window >> 1) + 1, IM_DT_FLOAT32);
    int length = 0;
    // stft/istft/analyzer with slipping window
    while (length < STFT_DATA_LENGTH + overlap)
    {
        ImMat amplitude;
        amplitude.create_like(short_time_amplitude);
        float * in_data, *out_data;
        if (length < STFT_DATA_LENGTH)
            in_data = (float *)time_domain.data + length;
        else
            in_data = (float *)padding_data.data;
        process.stft(in_data, (float *)short_time_domain.data);

        // do analyzer
        ImGui::ImReComposeAmplitude((float *)short_time_domain.data, (float *)amplitude.data, window);
        for (int i = 0; i < amplitude.w; i++) short_time_amplitude.at<float>(i) += amplitude.at<float>(i);
        // analyzer end

        if (length >= overlap)
            out_data = (float *)short_time_domain_out.data + length - overlap;
        else
            out_data = (float *)padding_data.data;
        process.istft((float *)short_time_domain.data, out_data);
        length += hope;
    }
    ImGui::BeginChild("STFT Result");
    // draw result
    ImVec2 channel_view_size = ImVec2(1024 * x_scale, 128);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.f));

    // draw time domain
    if (ImPlot::BeginPlot("##time_domain", channel_view_size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
    {
        ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxesLimits(0, time_domain.w, -1.0 * wave_scale, 1.0 * wave_scale, ImGuiCond_Always);
        ImPlot::PlotLine("##TimeDomain", (float *)time_domain.data, time_domain.w);
        ImPlot::EndPlot();
    }

    if (ImGui::IsItemHovered())
    {
        if (io.MouseWheel < -FLT_EPSILON) { wave_scale *= 0.9f; if (wave_scale < 0.1f) wave_scale = 0.1f; }
        if (io.MouseWheel >  FLT_EPSILON) { wave_scale *= 1.1f; if (wave_scale > 4.0f) wave_scale = 4.0f; }
        if (io.MouseWheelH < -FLT_EPSILON) { x_scale *= 0.9f; if (x_scale < 1.0f) x_scale = 1.0f; }
        if (io.MouseWheelH >  FLT_EPSILON) { x_scale *= 1.1f; if (x_scale > 16.0f) x_scale = 16.0f; }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { wave_scale = 1.0; x_scale = 1.0; }
    }

    // draw spectrogram
    if (spectrogram_texture)
    {
        ImGui::Image(spectrogram_texture, channel_view_size);
    }

    // draw stft domain (amplitude)
    if (ImPlot::BeginPlot("##stft_domain", channel_view_size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
    {
        ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxesLimits(0, short_time_amplitude.w, 0.0 * fft_scale, 1.0 * fft_scale, ImGuiCond_Always);
        ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.75f);
        ImPlot::PlotShaded("##STFTDomainAMP", (float *)short_time_amplitude, short_time_amplitude.w);
        ImPlot::PopStyleVar();
        ImPlot::PlotLine("##STFTDomain", (float *)short_time_amplitude.data, short_time_amplitude.w);
        ImPlot::EndPlot();
    }
    if (ImGui::IsItemHovered())
    {
        if (io.MouseWheel < -FLT_EPSILON) { fft_scale *= 0.9f; if (fft_scale < 0.1f) fft_scale = 0.1f; }
        if (io.MouseWheel >  FLT_EPSILON) { fft_scale *= 1.1f; if (fft_scale > 32.0f) fft_scale = 32.0f; }
        if (io.MouseWheelH < -FLT_EPSILON) { x_scale *= 0.9f; if (x_scale < 1.0f) x_scale = 1.0f; }
        if (io.MouseWheelH >  FLT_EPSILON) { x_scale *= 1.1f; if (x_scale > 16.0f) x_scale = 16.0f; }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { fft_scale = 1.0; x_scale = 1.0; }
    }

    // draw time domain out(istft)
    if (ImPlot::BeginPlot("##time_domain_out", channel_view_size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
    {
        ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxesLimits(0, short_time_domain_out.w, -1.0 * wave_scale, 1.0 * wave_scale, ImGuiCond_Always);
        ImPlot::PlotLine("##TimeDomainOut", (float *)short_time_domain_out.data, short_time_domain_out.w);
        ImPlot::EndPlot();
    }
    if (ImGui::IsItemHovered())
    {
        if (io.MouseWheel < -FLT_EPSILON) { wave_scale *= 0.9f; if (wave_scale < 0.1f) wave_scale = 0.1f; }
        if (io.MouseWheel >  FLT_EPSILON) { wave_scale *= 1.1f; if (wave_scale > 4.0f) wave_scale = 4.0f; }
        if (io.MouseWheelH < -FLT_EPSILON) { x_scale *= 0.9f; if (x_scale < 1.0f) x_scale = 1.0f; }
        if (io.MouseWheelH >  FLT_EPSILON) { x_scale *= 1.1f; if (x_scale > 16.0f) x_scale = 16.0f; }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { wave_scale = 1.0; x_scale = 1.0; }
    }

    ImGui::PopStyleColor();
    ImGui::EndChild();
}

#if IMGUI_VULKAN_SHADER
void  ImVulkanTestWindow(const char* name, bool* p_open, ImGuiWindowFlags flags)
{
    ImGui::Begin(name, p_open, flags);
    static int loop_count = 200;
    static int block_count = 20;
    static int cmd_count = 1;
    static float fp32[8] = {0.f};
    static float fp32v4[8] = {0.f};
    static float fp32v8[8] = {0.f};
    static float fp16pv4[8] = {0.f};
    static float fp16pv8[8] = {0.f};
    static float fp16s[8] = {0.f};
    static float fp16sv4[8] = {0.f};
    static float fp16sv8[8] = {0.f};
    int device_count = ImGui::get_gpu_count();
    auto print_result = [](float gflops)
    {
        std::string result;
        if (gflops == -1)
            result = "  error";
        if (gflops == -233)
            result = "  not supported";
        if (gflops == 0)
            result = "  not tested";
        if (gflops > 1000)
            result = "  " + std::to_string(gflops / 1000.0) + " TFLOPS";
        else
            result = "  " + std::to_string(gflops) + " GFLOPS";
        return result;
    };
    for (int i = 0; i < device_count; i++)
    {
        ImGui::VulkanDevice* vkdev = ImGui::get_gpu_device(i);
        uint32_t driver_version = vkdev->info.driver_version();
        uint32_t api_version = vkdev->info.api_version();
        int device_type = vkdev->info.type();
        std::string driver_ver = std::to_string(VK_VERSION_MAJOR(driver_version)) + "." + 
                                std::to_string(VK_VERSION_MINOR(driver_version)) + "." +
                                std::to_string(VK_VERSION_PATCH(driver_version));
        std::string api_ver =   std::to_string(VK_VERSION_MAJOR(api_version)) + "." + 
                                std::to_string(VK_VERSION_MINOR(api_version)) + "." +
                                std::to_string(VK_VERSION_PATCH(api_version));
        std::string device_name = vkdev->info.device_name();
        uint32_t gpu_memory_budget = vkdev->get_heap_budget();
        uint32_t gpu_memory_usage = vkdev->get_heap_usage();
        ImGui::Text("Device[%d]", i);
        ImGui::Text("Driver:%s", driver_ver.c_str());
        ImGui::Text("   API:%s", api_ver.c_str());
        ImGui::Text("  Name:%s", device_name.c_str());
        ImGui::Text("Memory:%uMB/%uMB", gpu_memory_usage, gpu_memory_budget);
        ImGui::Text("Device Type:%s", device_type == 0 ? "Discrete" : device_type == 1 ? "Integrated" : device_type == 2 ? "Virtual" : "CPU");
        ImGui::PushID(i);
        if (ImGui::TreeNode("GPU Caps"))
        {
            ImGui::Text("fp16 packed: %s", vkdev->info.support_fp16_packed() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("fp16 storage: %s", vkdev->info.support_fp16_storage() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("fp16 uniform: %s", vkdev->info.support_fp16_uniform() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("fp16 arithmetic: %s", vkdev->info.support_fp16_arithmetic() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("int8 packed: %s", vkdev->info.support_int8_packed() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("int8 storage: %s", vkdev->info.support_int8_storage() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("int8 uniform: %s", vkdev->info.support_int8_uniform() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("int8 arithmetic: %s", vkdev->info.support_int8_arithmetic() ? ICON_TRUE : ICON_FALSE);
            ImGui::Separator();
            ImGui::Text("ycbcr conversion: %s", vkdev->info.support_ycbcr_conversion() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("cooperative matrix: %s", vkdev->info.support_cooperative_matrix() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("cooperative matrix 8/8/16: %s", vkdev->info.support_cooperative_matrix_8_8_16() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("cooperative matrix 16/8/8: %s", vkdev->info.support_cooperative_matrix_16_8_8() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("cooperative matrix 16/8/16: %s", vkdev->info.support_cooperative_matrix_16_8_16() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("cooperative matrix 16/16/16: %s", vkdev->info.support_cooperative_matrix_16_16_16() ? ICON_TRUE : ICON_FALSE);
            ImGui::Separator();
            ImGui::Text("subgroup basic: %s", vkdev->info.support_subgroup_basic() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("subgroup vote: %s", vkdev->info.support_subgroup_vote() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("subgroup ballot: %s", vkdev->info.support_subgroup_ballot() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("subgroup shuffle: %s", vkdev->info.support_subgroup_shuffle() ? ICON_TRUE : ICON_FALSE);
            ImGui::Separator();
            ImGui::Text("buggy storage buffer no l1: %s", vkdev->info.bug_storage_buffer_no_l1() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("buggy corrupted online pipeline cache: %s", vkdev->info.bug_corrupted_online_pipeline_cache() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("buggy buffer image load zero: %s", vkdev->info.bug_buffer_image_load_zero() ? ICON_TRUE : ICON_FALSE);
            ImGui::Text("buggy implicit fp16 arithmetic: %s", vkdev->info.bug_implicit_fp16_arithmetic() ? ICON_TRUE : ICON_FALSE);
            ImGui::TreePop();
        }
        ImGui::PopID();

        std::string buffon_label = "Perf Test##" + std::to_string(i);
        if (ImGui::Button(buffon_label.c_str(), ImVec2(120, 20)))
        {
            int _loop_count = device_type == 0 ? loop_count : loop_count / 5;
            fp32[i]     = ImGui::ImVulkanPeak(vkdev, _loop_count, block_count, cmd_count, 0, 0, 0);
            fp32v4[i]   = ImGui::ImVulkanPeak(vkdev, _loop_count, block_count, cmd_count, 0, 0, 1);
            fp32v8[i]   = ImGui::ImVulkanPeak(vkdev, _loop_count, block_count, cmd_count, 0, 0, 2);
            fp16pv4[i]  = ImGui::ImVulkanPeak(vkdev, _loop_count, block_count, cmd_count, 1, 1, 1);
            fp16pv8[i]  = ImGui::ImVulkanPeak(vkdev, _loop_count, block_count, cmd_count, 1, 1, 2);
            fp16s[i]    = ImGui::ImVulkanPeak(vkdev, _loop_count, block_count, cmd_count, 2, 1, 0);
            fp16sv4[i]  = ImGui::ImVulkanPeak(vkdev, _loop_count, block_count, cmd_count, 2, 1, 1);
            fp16sv8[i]  = ImGui::ImVulkanPeak(vkdev, _loop_count, block_count, cmd_count, 2, 1, 2);
        }
        ImGui::Text(" FP32 Scalar :%s", print_result(fp32[i]).c_str());
        ImGui::Text("   FP32 Vec4 :%s", print_result(fp32v4[i]).c_str());
        ImGui::Text("   FP32 Vec8 :%s", print_result(fp32v8[i]).c_str());
        ImGui::Text("  FP16p Vec4 :%s", print_result(fp16pv4[i]).c_str());
        ImGui::Text("  FP16p Vec8 :%s", print_result(fp16pv8[i]).c_str());
        ImGui::Text("FP16s Scalar :%s", print_result(fp16s[i]).c_str());
        ImGui::Text("  FP16s Vec4 :%s", print_result(fp16sv4[i]).c_str());
        ImGui::Text("  FP16s Vec8 :%s", print_result(fp16sv8[i]).c_str());
        
        ImGui::Separator();
    }
    ImGui::End();
}
#endif
} // namespace ImGui
