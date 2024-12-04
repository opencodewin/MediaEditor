#include "imgui_toggle.h"
#include "imgui_toggle_presets.h"

static void imgui_toggle_state(const ImGuiToggleConfig& config, ImGuiToggleStateConfig& state)
{
	// some values to use for slider limits
	const float border_thickness_max_pixels = 50.0f;
	const float max_height = config.Size.y > 0 ? config.Size.y : ImGui::GetFrameHeight();
	const float half_max_height = max_height * 0.5f;

	// knob offset controls how far into or out of the frame the knob should draw.
	ImGui::SliderFloat2("Knob Offset (px: x, y)", &state.KnobOffset.x, -half_max_height, half_max_height);

	// knob inset controls how many pixels the knob is set into the frame. negative values will cause it to grow outside the frame.
	// for circular knobs, we will just use a single value, while for we will use top/left/bottom/right offsets.
	const bool is_rounded = config.KnobRounding >= 1.0f;
	if (is_rounded)
	{
		float inset_average = state.KnobInset.GetAverage();
		ImGui::SliderFloat("Knob Inset (px)", &inset_average, -half_max_height, half_max_height);
		state.KnobInset = inset_average;
	}
	else
	{
		ImGui::SliderFloat4("Knob Inset (px: t, l, b, r)", state.KnobInset.Offsets, -half_max_height, half_max_height);
	}

	// how thick should the frame border be (if enabled)
	ImGui::SliderFloat("Frame Border Thickness (px)", &state.FrameBorderThickness, 0.0f, border_thickness_max_pixels);

	// how thick should the knob border be (if enabled)
	ImGui::SliderFloat("Knob Border Thickness (px)", &state.KnobBorderThickness, 0.0f, border_thickness_max_pixels);
}

static void imgui_toggle_simple()
{
	static bool values[] = { true, true, true, true, true, true, true, true };
	size_t value_index = 0;

	const ImVec4 green(0.16f, 0.66f, 0.45f, 1.0f);
	const ImVec4 green_hover(0.0f, 1.0f, 0.57f, 1.0f);
	const ImVec4 salmon(1.0f, 0.43f, 0.35f, 1.0f);
	const ImVec4 green_shadow(0.0f, 1.0f, 0.0f, 0.4f);

	// a default and default animated toggle
	ImGui::Toggle("Default Toggle", &values[value_index++]);
	ImGui::Toggle("Animated Toggle", &values[value_index++], ImGuiToggleFlags_Animated);

	// this toggle draws a simple border around it's frame and knob
	ImGui::Toggle("Bordered Knob", &values[value_index++], ImGuiToggleFlags_Bordered, 1.0f);

	// this toggle draws a simple shadow around it's frame and knob
	ImGui::PushStyleColor(ImGuiCol_BorderShadow, green_shadow);
	ImGui::Toggle("Shadowed Knob", &values[value_index++], ImGuiToggleFlags_Shadowed, 1.0f);

	// this toggle draws the shadow & and the border around it's frame and knob.
	ImGui::Toggle("Bordered + Shadowed Knob", &values[value_index++], ImGuiToggleFlags_Bordered | ImGuiToggleFlags_Shadowed, 1.0f);
	ImGui::PopStyleColor(1);

	// this toggle uses stack-pushed style colors to change the way it displays
	ImGui::PushStyleColor(ImGuiCol_Button, green);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, green_hover);
	ImGui::Toggle("Green Toggle", &values[value_index++]);
	ImGui::PopStyleColor(2);

	ImGui::Toggle("Toggle with A11y Labels", &values[value_index++], ImGuiToggleFlags_A11y);

	// this toggle shows no label
	ImGui::Toggle("##Toggle With Hidden Label", &values[value_index++]);
}

static void imgui_toggle_custom()
{
	static ImGuiToggleConfig config;
	static bool toggle_custom = true;

	ImGui::NewLine();

	ImGui::Toggle("Customized Toggle", &toggle_custom, config);

	ImGui::NewLine();

	// these first settings are used no matter the toggle's state.
	ImGui::Text("Persistent Toggle Settings");

	// animation duration controls how long the toggle animates, in seconds. if set to 0, animation is disabled.
	if (ImGui::SliderFloat("Animation Duration (seconds)", &config.AnimationDuration, ImGuiToggleConstants::AnimationDurationMinimum, 2.0f))
	{
		// if the user adjusted the animation duration slider, go ahead and turn on the animation flags.
		config.Flags |= ImGuiToggleFlags_Animated;
	}

	// frame rounding sets how round the frame is when drawn, where 0 is a rectangle, and 1 is a circle.
	ImGui::SliderFloat("Frame Rounding (scale)", &config.FrameRounding, ImGuiToggleConstants::FrameRoundingMinimum, ImGuiToggleConstants::FrameRoundingMaximum);

	// knob rounding sets how round the knob is when drawn, where 0 is a rectangle, and 1 is a circle.
	ImGui::SliderFloat("Knob Rounding (scale)", &config.KnobRounding, ImGuiToggleConstants::KnobRoundingMinimum, ImGuiToggleConstants::KnobRoundingMaximum);

	// size controls the width and the height of the toggle frame
	ImGui::SliderFloat2("Size (px: w, h)", &config.Size.x, 0.0f, 200.0f, "%.0f");

	// width ratio sets how wide the toggle is with relation to the frame height. if Size is non-zero, this is unused.
	ImGui::SliderFloat("Width Ratio (scale)", &config.WidthRatio, ImGuiToggleConstants::WidthRatioMinimum, ImGuiToggleConstants::WidthRatioMaximum);

	// a11y style sets the type of additional on/off indicator drawing
	if (ImGui::Combo("A11y Style", &config.A11yStyle,
		"Label\0"
		"Glyph\0"
		"Dot\0"
		"\0"))
	{
		// if the user adjusted the a11y style combo, go ahead and turn on the a11y flag.
		config.Flags |= ImGuiToggleFlags_A11y;
	}

	// some tabs to adjust the "state" settings of the toggle (configuration dependent on if the toggle is on or off.)
	if (ImGui::BeginTabBar("State"))
	{
		if (ImGui::BeginTabItem("\"Off State\" Settings"))
		{
			imgui_toggle_state(config, config.Off);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("\"On State\"Settings"))
		{
			imgui_toggle_state(config, config.On);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::Separator();

	// flags for various toggle features
	ImGui::Text("Flags");
	ImGui::Columns(2);
	ImGui::Text("Meta Flags");
	ImGui::NextColumn();
	ImGui::Text("Individual Flags");
	ImGui::Separator();
	ImGui::NextColumn();

	// should the toggle have borders (sets all border flags)
	ImGui::CheckboxFlags("Bordered", &config.Flags, ImGuiToggleFlags_Bordered);

	// should the toggle have shadows (sets all shadow flags)
	ImGui::CheckboxFlags("Shadowed", &config.Flags, ImGuiToggleFlags_Shadowed);

	ImGui::NextColumn();

	// should the toggle animate
	ImGui::CheckboxFlags("Animated", &config.Flags, ImGuiToggleFlags_Animated);

	// should the toggle have a bordered frame
	ImGui::CheckboxFlags("BorderedFrame", &config.Flags, ImGuiToggleFlags_BorderedFrame);

	// should the toggle have a bordered knob
	ImGui::CheckboxFlags("BorderedKnob", &config.Flags, ImGuiToggleFlags_BorderedKnob);

	// should the toggle have a shadowed frame
	ImGui::CheckboxFlags("ShadowedFrame", &config.Flags, ImGuiToggleFlags_ShadowedFrame);

	// should the toggle have a shadowed knob
	ImGui::CheckboxFlags("ShadowedKnob", &config.Flags, ImGuiToggleFlags_ShadowedKnob);

	// should the toggle draw a11y glyphs
	ImGui::CheckboxFlags("A11y", &config.Flags, ImGuiToggleFlags_A11y);
	ImGui::Columns();

	ImGui::Separator();

	// what follows are some configuration presets. check the source of those functions to see how they work.
	ImGui::Text("Configuration Style Presets");

	if (ImGui::Button("Reset to Default"))
	{
		config = ImGuiTogglePresets::DefaultStyle();
	}
	ImGui::SameLine();

	if (ImGui::Button("Rectangle"))
	{
		config = ImGuiTogglePresets::RectangleStyle();
	}
	ImGui::SameLine();

	if (ImGui::Button("Glowing"))
	{
		config = ImGuiTogglePresets::GlowingStyle();
	}
	ImGui::SameLine();

	if (ImGui::Button("iOS"))
	{
		config = ImGuiTogglePresets::iOSStyle();
	}
	ImGui::SameLine();

	if (ImGui::Button("Material"))
	{
		config = ImGuiTogglePresets::MaterialStyle();
	}
	ImGui::SameLine();

	if (ImGui::Button("Minecraft"))
	{
		config = ImGuiTogglePresets::MinecraftStyle();
	}
}

void ImGui::imgui_toggle_example()
{
	// use some lovely gray backgrounds for "off" toggles
	// the default would otherwise use your theme's frame background colors.
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.45f, 0.45f, 0.45f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.65f, 0.65f, 0.65f, 1.0f));

	// a toggle that will allow the user to view the demo for simple toggles or a custom toggle
	static bool show_custom_toggle = true;
	ImGui::Toggle(
		show_custom_toggle ? "Showing Custom Toggle" : "Showing Simple Toggles"
		, &show_custom_toggle);

	ImGui::Separator();

	if (show_custom_toggle)
	{
		imgui_toggle_custom();
	}
	else
	{
		imgui_toggle_simple();
	}

	// pop the color styles
	ImGui::PopStyleColor(2);
}
