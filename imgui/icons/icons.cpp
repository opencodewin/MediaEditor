#include "imgui.h"
#include "imgui_internal.h"

extern const char StepMath_compressed_data_base85[];
#define ICON_MIN_MATH 0x2000
#define ICON_MAX_MATH 0x2A80

bool ImGui::LoadInternalIcons(ImFontAtlas* atlas, float scale)
{
    float icons_size = 16.0f * scale;
    ImFontConfig icons_config;
    icons_config.OversampleH    = 1;
    icons_config.OversampleV    = 1;
    icons_config.MergeMode      = true; 
    icons_config.PixelSnapH     = true;
    icons_config.SizePixels     = icons_size * 1.0f;
    icons_config.EllipsisChar   = (ImWchar)-1; //(ImWchar)0x0085;
    
    // Audio Icons
    icons_config.GlyphOffset.y  = scale * 4.0f * IM_FLOOR(icons_config.SizePixels / icons_size);  // Add +4 offset per 16 units
    static const ImWchar fad_icons_ranges[] = { ICON_MIN_FAD, ICON_MAX_FAD, 0 };
	atlas->AddFontFromMemoryCompressedBase85TTF(fontaudio_compressed_data_base85, icons_size, &icons_config, fad_icons_ranges);

    icons_config.GlyphOffset.y = scale * 2.0f * IM_FLOOR(icons_config.SizePixels / icons_size);  // Add +2 offset per 16 units
    // FileDialog Icons
    static const ImWchar icons_ranges[] = { ICON_MIN_IGFD, ICON_MAX_IGFD, 0 };
	atlas->AddFontFromMemoryCompressedBase85TTF(FONT_ICON_BUFFER_NAME_IGFD, icons_size, &icons_config, icons_ranges);

    // Awesome brands Icons
    static const ImWchar fab_icons_ranges[] = { ICON_MIN_FAB, ICON_MAX_FAB, 0 };
	atlas->AddFontFromMemoryCompressedBase85TTF(fa_brands_compressed_data_base85, icons_size, &icons_config, fab_icons_ranges);

    // Awesome Icons
    static const ImWchar fa_icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	atlas->AddFontFromMemoryCompressedBase85TTF(fa_regular_compressed_data_base85, icons_size, &icons_config, fa_icons_ranges);
    atlas->AddFontFromMemoryCompressedBase85TTF(fa_solid_compressed_data_base85, icons_size, &icons_config, fa_icons_ranges);

    // Fork Awesome Icons
    static const ImWchar fk_icons_ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 };
	atlas->AddFontFromMemoryCompressedBase85TTF(fork_webfont_compressed_data_base85, icons_size, &icons_config, fk_icons_ranges);

    // Fork Material Design Icons
    static const ImWchar md_icons_ranges[] = { ICON_MIN_MD, ICON_MAX_16_MD, 0 };
	atlas->AddFontFromMemoryCompressedBase85TTF(MaterialIcons_compressed_data_base85, icons_size, &icons_config, md_icons_ranges);

    // Kenney Game icons
    static const ImWchar ki_icons_ranges[] = { ICON_MIN_KI, ICON_MAX_KI, 0 };
	atlas->AddFontFromMemoryCompressedBase85TTF(kenney_compressed_data_base85, icons_size, &icons_config, ki_icons_ranges);

    // StepMath
    static const ImWchar math_icons_ranges[] = { ICON_MIN_MATH, ICON_MAX_MATH, 0 };
	atlas->AddFontFromMemoryCompressedBase85TTF(StepMath_compressed_data_base85, icons_size, &icons_config, math_icons_ranges);

    // Code icon
    static const ImWchar code_icons_ranges[] = { ICON_MIN_CI, ICON_MAX_CI, 0 };
	atlas->AddFontFromMemoryCompressedBase85TTF(Code_compressed_data_base85, icons_size, &icons_config, code_icons_ranges);
#if 0
    // Lucide icon
    icons_config.GlyphOffset.y  = scale * 4.0f * IM_FLOOR(icons_config.SizePixels / icons_size);  // Add +4 offset per 16 units
    static const ImWchar lucide_icons_ranges[] = { ICON_MIN_LC, ICON_MAX_LC, 0 };
	atlas->AddFontFromMemoryCompressedBase85TTF(lucide_compressed_data_base85, icons_size, &icons_config, lucide_icons_ranges);
#endif
    return true;
}