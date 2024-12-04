# 🔘 imgui_toggle

A small Dear ImGui widget that implements a modern toggle style switch. Requires C++11.

## Overview

Based on the discussion in [https://github.com/ocornut/imgui/issues/1537](https://github.com/ocornut/imgui/issues/1537), and on the implementation of `ImGui::Checkbox()`,
this small widget collection implements a customizable "Toggle" style button for Dear ImGui. A toggle represents a boolean on/off much like a checkbox, but is better suited
to some particular paradigms depending on the UI designer's goals. It can often more clearly indicate an enabled/disabled state.

`imgui_toggle` also supports an optional small animation, similar to that seen in mobile OS and web applications, which can further aid in user feedback.

Internally, `imgui_toggle` functions very similarly to `ImGui::Checkbox()`, with the exception that it activates on mouse down rather than the release. It supports drawing
a label in the same way, and toggling the value by clicking that associated label. The label can be hidden [as on other controls](https://github.com/ocornut/imgui/blob/master/docs/FAQ.md#q-how-can-i-have-widgets-with-an-empty-label).

## Preview

![`imgui_toggle` example animated gif](./.meta/imgui_toggle_example.gif)

_An example of `imgui_toggle`, produced by the [example code](./EXAMPLE.md), as an animated gif._

## Usage

Add the `*.cpp` and `*.h` files to your project, and include `imgui_toggle.h` in the source file you wish to use toggles.

See `imgui_toggle.h` for the API, or below for a brief example. As well, [EXAMPLE.md](./EXAMPLE.md) has a more thorough usage example.

```cpp

const ImVec4 green(0.16f, 0.66f, 0.45f, 1.0f);
const ImVec4 green_hover(0.0f, 1.0f, 0.57f, 1.0f);

const ImVec4 gray_dim(0.45f, 0.45f, 0.45f, 1.0f);
const ImVec4 gray(0.65f, 0.65f, 0.65f, 1.0f);

// use some lovely gray backgrounds for "off" toggles
// the default will use your theme's frame background colors.
ImGui::PushStyleColor(ImGuiCol_FrameBg, gray_dim);
ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, gray);

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

// pop the FrameBg/FrameBgHover color styles
ImGui::PopStyleColor(2);
```

## Styling

While `imgui_toggle` maintains a simple API for quick and easy toggles, a more complex one exists to allow the user to better customize the widget.

By using the overload that takes a `const ImGuiToggleConfig&`, a structure can be provided that provides a multitude of configuration parameters.

Notably this also allows providing a pointer to a `ImGuiTogglePalette` structure, which allows changing all the colors used to draw the widget. However, this method of configuration is not strictly necessary, as `imgui_toggle` will follow your theme colors as defined below if no palette or color replacement is specified.

### Theme Colors

Since `imgui_toggle` isn't part of Dear ImGui proper, it doesn't have any direct references in `ImGuiCol_` for styling. `imgui_toggle` in addition to the method described above, you can use `ImGui::PushStyleColor()` and `ImGui::PopStyleColor()` to adjust the following theme colors around your call to `ImGui::Toggle()`:

- `ImGuiCol_Text`: Will be used as the color of the knob portion of the toggle, and the color of the accessibility glyph if enabled and "on".
- `ImGuiCol_Button`: Will be used as the frame color of the toggle when it is in the "on" position, and the widget is not hovered.
- `ImGuiCol_ButtonHovered`: Will be used as the frame color of the toggle when it is in the "on" position, and the widget is hovered over.
- `ImGuiCol_FrameBg`: Will be used as the frame color of the toggle when it is in the "off" position, and the widget is not hovered. Also used for the color of the accessibility glyph if enabled and "off".
- `ImGuiCol_FrameBgHovered`: Will be used as the frame color of the toggle when it is in the "off" position, and the widget is hovered over.
- `ImGuiCol_Border`: Will be used as the color drawn as the border on the frame and knob if the related flags are passed.

Unfortunately, the dark gray and light gray used while the toggle is in the "off" position are currently defined by the widget code itself and not by any theme color.

## Future Considerations

As brought up by [ocornut](https://github.com/ocornut/imgui/issues/1537#issuecomment-355562097), if `imgui_toggle` were to be part of mainline Dear ImGui in the future,
there are some questions that should likely be answered. Most notably for me are the following:

- Should the toggle get it's own enums in the style system?
  - If so, should which colors should it define, and which would it be okay sharing?
  - If I were choosing, I feel the button and hovered styles as the "on" coloring are acceptable, and perhaps adding three more shared styles `ImGuiCol_Knob`, `ImGuiCol_FrameBgOff`, and `ImGuiCol_FrameBgOffHover` for use as the foreground knob color, and the "off" background states. (An `Active` may be needed too if switching to operate on input release instead of press.)
- Is the rendering quality good enough?
- Is the dependence on the `LastActiveIdTimer` acceptable for animation, and the user must accept that clicking quickly would skip previous animations?
- Should the toggle behave *exactly* like `ImGui::Checkbox()`, toggling on release rather than press?

----

## 📝 License

`imgui_toggle` is [licensed](./LICENSE) under the Zero-Clause BSD License (SPDX-License-Identifier: 0BSD). If you're interested in `imgui_toggle` under other terms, please contact the author.

Copyright © 2022 [Chris Marc Dailey](https://cmd.wtf)

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

## Acknowledgements

`imgui_toggle` drew inspiration from [ocornut's original share](https://github.com/ocornut/imgui/issues/1537#issuecomment-355562097),
his [animated variant](https://github.com/ocornut/imgui/issues/1537#issuecomment-355569554), [nerdtronik's shift to theme colors](https://github.com/ocornut/imgui/issues/1537#issuecomment-780262461),
and [ashifolfi's squircle](https://github.com/ocornut/imgui/issues/1537#issuecomment-1272612641) concept. Inspiration for border drawing came from [moebiussurfing](https://github.com/cmdwtf/imgui_toggle/issues/1#issue-1441329209).

As well, inspiration was derived from [Dear ImGui's current `Checkbox` implementation](https://github.com/ocornut/imgui/blob/529cba19b09cf6db206de2b9eaa3152ecb2feff8/imgui_widgets.cpp#L1102),
for the behavior, label drawing, and hopefully preparing to [handle mixed values/indeterminate states](https://github.com/ocornut/imgui/issues/2644) (albeit unsupported as of yet).
