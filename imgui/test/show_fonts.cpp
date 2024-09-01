#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_helper.h>
#include <application.h>
#include "Config.h"

static bool DrawFrame(void* handle, bool app_will_quit)
{
    static bool show_latin = false;
    static bool show_punctuation = false;
    static bool show_greek_coptic = false;
    static bool show_hiragana_katakana = false;
    static bool show_korean_alphabets = false;
    static bool show_korean = false;
    static bool show_half_width = false;
    static bool show_cjk = false;
    static bool show_custom_icon = true;
    static bool show_others = false;
    bool app_done = false;
    auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);

    ImGui::Checkbox("Basic Latin", &show_latin); ImGui::SameLine();
    ImGui::Checkbox("General Punctuation", &show_punctuation); ImGui::SameLine();
    ImGui::Checkbox("Greek Coptic", &show_greek_coptic); ImGui::SameLine();
    ImGui::Checkbox("Hiragana Katakana", &show_hiragana_katakana); ImGui::SameLine();
    ImGui::Checkbox("Korean alphabets", &show_korean_alphabets); ImGui::SameLine();
    ImGui::Checkbox("Korean", &show_korean); ImGui::SameLine();
    ImGui::Checkbox("Half-width", &show_half_width); ImGui::SameLine();
    ImGui::Checkbox("CJK", &show_cjk); ImGui::SameLine();
    ImGui::Checkbox("Icons", &show_custom_icon); ImGui::SameLine();
    ImGui::Checkbox("others", &show_others);
    ImGuiContext& g = *GImGui;
    ImFontAtlas* atlas = g.IO.Fonts;
    if (atlas->Fonts.Size > 0)
    {
        ImFont* font = atlas->Fonts[0];
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImU32 glyph_col = ImGui::GetColorU32(ImGuiCol_Text);
        const float cell_size = font->FontSize * 1.2;
        const auto font_size = ImGui::CalcTextSize(" ");
        const float cell_spacing = ImGui::GetStyle().ItemSpacing.y;
        ImGui::Text("Glyphs (%d)", font->Glyphs.Size);
        for (unsigned int base = 0; base <= IM_UNICODE_CODEPOINT_MAX; base += 256)
        {
            // Skip ahead if a large bunch of glyphs are not present in the font (test in chunks of 4k)
            // This is only a small optimization to reduce the number of iterations when IM_UNICODE_MAX_CODEPOINT
            // is large // (if ImWchar==ImWchar32 we will do at least about 272 queries here)
            if (!(base & 4095) && font->IsGlyphRangeUnused(base, base + 4095))
            {
                base += 4096 - 256;
                continue;
            }

            if (!show_latin && base < 0x00FF)
                continue;

            if (!show_punctuation && base >= 0x2000 && base < 0x2AFF)
                continue;

            if (!show_greek_coptic && base >= 0x0300 && base < 0x03FF)
                continue;

            if (!show_hiragana_katakana && base >= 0x3000 && base < 0x30FF)
                continue;
            
            if (!show_korean_alphabets && base >= 0x3100 && base < 0x31FF)
                continue;

            if (!show_korean && base >= 0xAC00 && base < 0xD7A3)
                continue;
            
            if (!show_half_width && base >= 0xFF00 && base < 0xFFEF)
                continue;
            
            if (!show_cjk && base >= 0x4e00 && base < 0x9FAF)
                continue;

            if (!show_custom_icon && base >= 0xe000 && base < 0xff00)
                continue;
            
            if (!show_others && base >= 0x0400 && base < 0x04FF)
                continue;

            int count = 0;
            for (unsigned int n = 0; n < 256; n++)
                if (font->FindGlyphNoFallback((ImWchar)(base + n)))
                    count++;
            if (count <= 0)
                continue;
            if (!ImGui::TreeNode((void*)(intptr_t)base, "U+%04X..U+%04X (%d %s)", base, base + 255, count, count > 1 ? "glyphs" : "glyph"))
                continue;

            // Draw a 32x8 grid of glyphs
            char code_str[8];
            ImVec2 base_pos = ImGui::GetCursorScreenPos();
            for (unsigned int n = 0; n < 256; n++)
            {
                // We use ImFont::RenderChar as a shortcut because we don't have UTF-8 conversion functions
                // available here and thus cannot easily generate a zero-terminated UTF-8 encoded string.
                ImVec2 cell_p1(base_pos.x + (n % 32) * (cell_size + cell_spacing), base_pos.y + (n / 32) * (cell_size + cell_spacing + font_size.y));
                ImVec2 cell_p2(cell_p1.x + cell_size, cell_p1.y + cell_size);
                const ImFontGlyph* glyph = font->FindGlyphNoFallback((ImWchar)(base + n));
                draw_list->AddRect(cell_p1, cell_p2, glyph ? IM_COL32(255, 255, 255, 100) : IM_COL32(255, 255, 255, 50));
                if (!glyph)
                    continue;
                font->RenderChar(draw_list, cell_size, cell_p1, glyph_col, (ImWchar)(base + n));
                if (ImGui::IsMouseHoveringRect(cell_p1, cell_p2) && ImGui::BeginTooltip())
                {
                    ImGui::Text("Codepoint: U+%04X", glyph->Codepoint);
                    ImGui::Separator();
                    ImGui::Text("Visible: %d", glyph->Visible);
                    ImGui::Text("AdvanceX: %.1f", glyph->AdvanceX);
                    ImGui::Text("Pos: (%.2f,%.2f)->(%.2f,%.2f)", glyph->X0, glyph->Y0, glyph->X1, glyph->Y1);
                    ImGui::Text("UV: (%.3f,%.3f)->(%.3f,%.3f)", glyph->U0, glyph->V0, glyph->U1, glyph->V1);
                    ImGui::EndTooltip();
                }
                snprintf(code_str, 8, "%04X", glyph->Codepoint);
                draw_list->AddText(cell_p1 + ImVec2(4, cell_size), IM_COL32(255, 255, 255, 100), code_str);
            }
            ImGui::Dummy(ImVec2((cell_size + cell_spacing) * 32, (cell_size + cell_spacing + font_size.y) * 8));
            ImGui::TreePop();
        }
    }

    ImGui::End();
    if (app_will_quit)
        app_done = true;
    return app_done;
}

void Application_Setup(ApplicationWindowProperty& property)
{
    property.name = "Application_Font";
    property.docking = false;
    property.viewport = false;
    property.font_scale = 2.0f;
    property.application.Application_Frame = DrawFrame;
}