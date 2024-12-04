[![Win](https://github.com/aiekick/ImCoolBar/actions/workflows/Win.yml/badge.svg)](https://github.com/aiekick/ImCoolBar/actions/workflows/Win.yml)
[![Linux](https://github.com/aiekick/ImCoolBar/actions/workflows/Linux.yml/badge.svg)](https://github.com/aiekick/ImCoolBar/actions/workflows/Linux.yml)
[![Osx](https://github.com/aiekick/ImCoolBar/actions/workflows/Osx.yml/badge.svg)](https://github.com/aiekick/ImCoolBar/actions/workflows/Osx.yml)

# ImCoolbar

# Minimal Sample

```cpp
	auto coolbar_button     = [](const char* label) -> bool {
		float w         = ImGui::GetCoolBarItemWidth();
		auto font_ptr   = ImGui::GetIO().Fonts->Fonts[0];
		font_ptr->Scale = ImGui::GetCoolBarItemScale();
		ImGui::PushFont(font_ptr);
		bool res = ImGui::Button(label, ImVec2(w, w));
		ImGui::PopFont();
		return res;
	};
		
	if (ImGui::BeginCoolBar("##CoolBarMain", ImCoolBar_Horizontal, ImVec2(0.5f, 1.0f))) {
		if (ImGui::CoolBarItem()) {
			if (coolbar_button("A")) {
			
			}
		}
		if (ImGui::CoolBarItem()) {
			if (coolbar_button("B")) {
			
			}
		}
		if (ImGui::CoolBarItem()) {
			if (coolbar_button("C")) {
			
			}
		}
		if (ImGui::CoolBarItem()) {
			if (coolbar_button("D")) {
			
			}
		}
		if (ImGui::CoolBarItem()) {
			if (coolbar_button("E")) {
			
			}
		}
		if (ImGui::CoolBarItem()) {
			if (coolbar_button("F")) {
			
			}
		}
		if (ImGui::CoolBarItem()) {
			if (coolbar_button("G")) {
			
			}
		}
		if (ImGui::CoolBarItem()) {
			if (coolbar_button("H")) {
			
			}
		}
		if (ImGui::CoolBarItem()) {
			if (coolbar_button("I")) {
			
			}
		}
		if (ImGui::CoolBarItem()) {
			if (coolbar_button("J")) {
			
			}
		}
		if (ImGui::CoolBarItem()) {
			if (coolbar_button("K")) {
			
			}
		}
		if (ImGui::CoolBarItem()) {
			if (coolbar_button("L")) {
			
			}
		}
		if (ImGui::CoolBarItem()) {
			if (coolbar_button("M")) {
			
			}
		}
		ImGui::EndCoolBar();
	}
```

Result : 

![alt text](https://github.com/aiekick/ImCoolBar/blob/DemoApp/doc/minimal_code.gif)

# Demo App 

![alt text](https://github.com/aiekick/ImCoolBar/blob/DemoApp/doc/DemoApp.gif)
