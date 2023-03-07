#include <stdio.h>
#include <imgui.h>
#include <imgui_helper.h>
#include <application.h>
#include <ImGuiFileDialog.h>
//#define STB_IMAGE_IMPLEMENTATION
//#include <stb_image.h>
//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include <stb_image_write.h>
#include <immat.h>
#include <ImVulkanShader.h>
#include "AlphaBlending_vulkan.h"
#include "LinearBlur_vulkan.h"
#include "BookFlip_vulkan.h"
#include "Bounce_vulkan.h"
#include "BowTie_vulkan.h"
#include "Burn_vulkan.h"
#include "BurnOut_vulkan.h"
#include "ButterflyWave_vulkan.h"
#include "CannabisLeaf_vulkan.h"
#include "CircleBlur_vulkan.h"
#include "CircleCrop_vulkan.h"
#include "ColorPhase_vulkan.h"
#include "ColourDistance_vulkan.h"
#include "CrazyParametric_vulkan.h"
#include "Crosshatch_vulkan.h"
#include "CrossWarp_vulkan.h"
#include "CrossZoom_vulkan.h"
#include "Cube_vulkan.h"
#include "DirectionalWarp_vulkan.h"
#include "DoomScreen_vulkan.h"
#include "Door_vulkan.h"
#include "Doorway_vulkan.h"
#include "Dreamy_vulkan.h"
#include "DreamyZoom_vulkan.h"
#include "Fade_vulkan.h"
#include "Flyeye_vulkan.h"
#include "GlitchDisplace_vulkan.h"
#include "GlitchMemories_vulkan.h"
#include "GridFlip_vulkan.h"
#include "Heart_vulkan.h"
#include "Hexagonalize_vulkan.h"
#include "KaleidoScope_vulkan.h"
#include "LuminanceMelt_vulkan.h"
#include "Morph_vulkan.h"
#include "Mosaic_vulkan.h"
#include "Move_vulkan.h"
#include "MultiplyBlend_vulkan.h"
#include "PageCurl_vulkan.h"
#include "Perlin_vulkan.h"
#include "Pinwheel_vulkan.h"
#include "Pixelize_vulkan.h"
#include "Polar_vulkan.h"
#include "PolkaDots_vulkan.h"
#include "Radial_vulkan.h"
#include "RandomSquares_vulkan.h"
#include "Ripple_vulkan.h"
#include "Rolls_vulkan.h"
#include "RotateScale_vulkan.h"
#include "SimpleZoom_vulkan.h"
#include "Slider_vulkan.h"
#include "SquaresWire_vulkan.h"
#include "Squeeze_vulkan.h"
#include "StereoViewer_vulkan.h"
#include "Swap_vulkan.h"
#include "Swirl_vulkan.h"
#include "WaterDrop_vulkan.h"
#include "Wind_vulkan.h"
#include "WindowBlinds_vulkan.h"
#include "WindowSlice_vulkan.h"
#include "Wipe_vulkan.h"
#include "ZoomInCircles_vulkan.h"
#include <CopyTo_vulkan.h>
#include <unistd.h>
std::string g_source_1;
std::string g_source_2;
std::string g_dist;
std::string g_source_name_1;
std::string g_source_name_2;
std::string g_dist_name;

ImGui::ImMat g_mat_1;
ImGui::ImMat g_mat_2;
ImGui::ImMat g_mat_d;
ImTextureID g_texture_1 = nullptr;
ImTextureID g_texture_2 = nullptr;
ImTextureID g_texture_d = nullptr;
ImGui::CopyTo_vulkan * g_copy = nullptr;

static const char* fusion_items[] = {
    "AlphaBlending",
    "Blur",
    "BookFlip",
    "Bounce",
    "BowTie",
    "Burn",
    "BurnOut",
    "ButterflyWave",
    "CannabisLeaf",
    "CircleBlur",
    "CircleCrop",
    "ColorPhase",
    "ColourDistance",
    "CrazyParametric",
    "Crosshatch",
    "CrossWarp",
    "CrossZoom",
    "Cube",
    "DirectionalWarp",
    "DoomScreen",
    "Door",
    "Doorway",
    "Dreamy",
    "DreamyZoom",
    "Fade",
    "Flyeye",
    "GlitchDisplace",
    "GlitchMemories",
    "GridFlip",
    "Heart",
    "Hexagonalize",
    "KaleidoScope",
    "LuminanceMelt",
    "Morph",
    "Mosaic",
    "Move",
    "MultiplyBlend",
    "PageCurl",
    "Perlin",
    "Pinwheel",
    "Pixelize",
    "Polar",
    "PolkaDots",
    "Radial",
    "RandomSquares",
    "Ripple",
    "Rolls",
    "RotateScale",
    "SimpleZoom",
    "Slider",
    "SquaresWire",
    "Squeeze",
    "StereoViewer",
    "Swap",
    "Swirl",
    "WaterDrop",
    "Wind",
    "WindowBlinds",
    "WindowSlice",
    "Wipe",
    "ZoomInCircles"
};

static void ShowVideoWindow(ImDrawList *draw_list, ImTextureID texture, ImVec2& pos, ImVec2& size, float& offset_x, float& offset_y, float& tf_x, float& tf_y, float aspectRatio, ImVec2 start = ImVec2(0.f, 0.f), ImVec2 end = ImVec2(1.f, 1.f), bool bLandscape = true)
{
    if (texture)
    {
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(("##video_window" + std::to_string((long long)texture)).c_str(), size);
        bool bViewisLandscape = size.x >= size.y ? true : false;
        bViewisLandscape |= bLandscape;
        bool bRenderisLandscape = aspectRatio > 1.f ? true : false;
        bool bNeedChangeScreenInfo = bViewisLandscape ^ bRenderisLandscape;
        float adj_w = bNeedChangeScreenInfo ? size.y : size.x;
        float adj_h = bNeedChangeScreenInfo ? size.x : size.y;
        float adj_x = adj_h * aspectRatio;
        float adj_y = adj_h;
        if (adj_x > adj_w) { adj_y *= adj_w / adj_x; adj_x = adj_w; }
        tf_x = (size.x - adj_x) / 2.0;
        tf_y = (size.y - adj_y) / 2.0;
        offset_x = pos.x + tf_x;
        offset_y = pos.y + tf_y;
        draw_list->AddImage(
            texture,
            ImVec2(offset_x, offset_y),
            ImVec2(offset_x + adj_x, offset_y + adj_y),
            start,
            end
        );
        tf_x = offset_x + adj_x;
        tf_y = offset_y + adj_y;
    }
}

static void load_image(std::string path, ImGui::ImMat & mat)
{
    int width = 0, height = 0, component = 0;
    uint8_t * data = nullptr;
    data = stbi_load(path.c_str(), &width, &height, &component, 4);
    if (data)
    {
        mat.release();
        mat.create_type(width, height, component, data, IM_DT_INT8);
    }
}

// stbi image custom context
typedef struct {
    int last_pos;
    void *context;
} stbi_mem_context;

static void custom_stbi_write_mem(void *context, void *data, int size)
{
    stbi_mem_context *c = (stbi_mem_context*)context; 
    char *dst = (char *)c->context;
    char *src = (char *)data;
    int cur_pos = c->last_pos;
    for (int i = 0; i < size; i++) {
        dst[cur_pos++] = src[i];
    }
    c->last_pos = cur_pos;
}

static bool binary_to_compressed_c(const char* filename, const char* symbol, void * data, int data_sz, int w, int h, int cols, int rows)
{
    // Read file
    FILE* f = fopen(filename, "wb");
    if (!f) return false;

    // Compress
    int maxlen = data_sz + 512 + (data_sz >> 2) + sizeof(int); // total guess
    char* compressed = (char*)data;
    int compressed_sz = data_sz;

    //fprintf(f, "// File: '%s' (%d bytes)\n", filename, (int)data_sz);
    //fprintf(f, "// Exported using binary_to_compressed_c.cpp\n");
    const char* static_str = "    ";
    const char* compressed_str = "";
    {
        fprintf(f, "%sconst unsigned int %s_%swidth = %d;\n", static_str, symbol, compressed_str, w);
        fprintf(f, "%sconst unsigned int %s_%sheight = %d;\n", static_str, symbol, compressed_str, h);
        fprintf(f, "%sconst unsigned int %s_%scols = %d;\n", static_str, symbol, compressed_str, cols);
        fprintf(f, "%sconst unsigned int %s_%srows = %d;\n", static_str, symbol, compressed_str, rows);
        fprintf(f, "%sconst unsigned int %s_%ssize = %d;\n", static_str, symbol, compressed_str, (int)compressed_sz);
        fprintf(f, "%sconst unsigned int %s_%sdata[%d/4] =\n{", static_str, symbol, compressed_str, (int)((compressed_sz + 3) / 4)*4);
        int column = 0;
        for (int i = 0; i < compressed_sz; i += 4)
        {
            unsigned int d = *(unsigned int*)(compressed + i);
            if ((column++ % 12) == 0)
                fprintf(f, "\n    0x%08x, ", d);
            else
                fprintf(f, "0x%08x, ", d);
        }
        fprintf(f, "\n};");
    }

    // Cleanup
    fclose(f);
    return true;
}

static void transition(int col, int row, int cols, int rows, int type, ImGui::ImMat& mat_a, ImGui::ImMat& mat_b, ImGui::ImMat& result)
{
    ImGui::ImMat mat_t;
    mat_t.type = mat_a.type;
    float progress = (float)(row * cols + col) / (float)(rows * cols - 1);
    switch (type)
    {
        case 0:
        {
            ImGui::AlphaBlending_vulkan m_fusion(0);
            float alpha = 1.0f - progress;
            m_fusion.blend(mat_a, mat_b, mat_t, alpha);
        }
        break;
        case 1:
        {
            float m_intensity {0.1};
            int m_passes {6};
            ImGui::LinearBlur_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_intensity, m_passes);
        }
        break;
        case 2:
        {
            ImGui::BookFlip_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress);
        }
        break;
        case 3:
        {
            ImPixel m_shadowColor {0.0f, 0.0f, 0.0f, 0.6f};
            float m_shadow_height {0.075};
            float m_bounces {3.f};
            ImGui::Bounce_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_shadowColor, m_shadow_height, m_bounces);
        }
        break;
        case 4:
        {
            ImGui::BowTieHorizontal_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, 0);
        }
        break;
        case 5:
        {
            ImPixel m_backColor {0.9f, 0.4f, 0.2f, 1.0f};
            ImGui::Burn_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_backColor);
        }
        break;
        case 6:
        {
            ImPixel m_shadowColor {0.0f, 0.0f, 0.0f, 1.0f};
            float m_smoothness {0.03};
            ImGui::BurnOut_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_shadowColor, m_smoothness);
        }
        break;
        case 7:
        {
            float m_amplitude   {1.f};
            float m_waves       {10.f};
            float m_colorSeparation {0.3f};
            ImGui::ButterflyWave_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_amplitude, m_waves, m_colorSeparation);
        }
        break;
        case 8:
        {
            ImGui::CannabisLeaf_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress);
        }
        break;
        case 9:
        {
            float m_smoothness  {0.3f};
            int m_open          {0};
            ImGui::CircleBlur_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_smoothness, m_open);
        }
        break;
        case 10:
        {
            ImPixel m_backColor {0.0f, 0.0f, 0.0f, 1.0f};
            ImGui::CircleCrop_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_backColor);
        }
        break;
        case 11:
        {
            ImPixel m_fromColor {0.0f, 0.2f, 0.4f, 0.0f};
            ImPixel m_toColor   {0.6f, 0.8f, 1.0f, 1.0f};
            ImGui::ColorPhase_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_fromColor, m_toColor);
        }
        break;
        case 12:
        {
            float m_power       {5.0f};
            ImGui::ColourDistance_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_power);
        }
        break;
        case 13:
        {
            float m_amplitude   {60.0f};
            float m_smoothness  {0.1f};
            float m_pa          {2};
            float m_pb          {1};
            ImGui::CrazyParametric_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_amplitude, m_smoothness, m_pa, m_pb);
        }
        break;
        case 14:
        {
            float m_threshold   {3.0f};
            float m_fadeEdge    {0.1f};
            ImGui::Crosshatch_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_threshold, m_fadeEdge);
        }
        break;
        case 15:
        {
            ImGui::CrossWarp_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress);
        }
        break;
        case 16:
        {
            float m_strength    {0.4f};
            ImGui::CrossZoom_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_strength);
        }
        break;
        case 17:
        {
            float m_persp       {0.6f};
            float m_unzoom      {0.05f};
            float m_reflection  {0.4f};
            float m_floating    {1.0f};
            ImGui::Cube_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_persp, m_unzoom, m_reflection, m_floating);
        }
        break;
        case 18:
        {
            float m_smoothness   {0.5f};
            ImVec2 m_direction   {-1.0, 1.0};
            ImGui::DirectionalWarp_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_smoothness, m_direction.x, m_direction.y);
        }
        break;
        case 19:
        {
            float m_amplitude   {2.f};
            float m_noise       {0.1f};
            float m_frequency   {0.5f};
            float m_dripScale   {0.5f};
            int m_bars          {10};
            ImGui::DoomScreen_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_amplitude, m_noise, m_frequency, m_dripScale, m_bars);
        }
        break;
        case 20:
        {
            bool m_bOpen        {true};
            bool m_bHorizon     {true};
            ImGui::Door_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_bOpen, m_bHorizon);
        }
        break;
        case 21:
        {
            float m_reflection  {0.4f};
            float m_perspective {0.2f};
            float m_depth       {3.f};
            ImGui::Doorway_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_reflection, m_perspective, m_depth);
        }
        break;
        case 22:
        {
            ImGui::Dreamy_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress);
        }
        break;
        case 23:
        {
            float m_rotation    {6.f};
            float m_scale       {1.2f};
            ImGui::DreamyZoom_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_rotation, m_scale);
        }
        break;
        case 24:
        {
            ImPixel m_color {0.0f, 0.0f, 0.0f, 1.0f};
            int m_type {1};
            ImGui::Fade_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_type, m_color);
        }
        break;
        case 25:
        {
            float m_size        {0.04f};
            float m_zoom        {50.f};
            float m_colorSeparation {0.3f};
            ImGui::Flyeye_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_size, m_zoom, m_colorSeparation);
        }
        break;
        case 26:
        {
            ImGui::GlitchDisplace_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress);
        }
        break;
        case 27:
        {
            ImGui::GlitchMemories_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress);
        }
        break;
        case 28:
        {
            ImPixel m_backColor {0.0f, 0.0f, 0.0f, 1.0f};
            float m_pause       {0.1};
            float m_dividerWidth {0.05};
            float m_randomness  {0.1};
            int m_size_x        {4};
            int m_size_y        {4};
            ImGui::GridFlip_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_backColor, m_pause, m_dividerWidth, m_randomness, m_size_x, m_size_y);
        }
        break;
        case 29:
        {
            ImGui::Heart_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress);
        }
        break;
        case 30:
        {
            float m_horizontalHexagons  {8.f};
            int m_steps         {20};
            ImGui::Hexagonalize_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_horizontalHexagons, m_steps);
        }
        break;
        case 31:
        {
            float m_speed       {1.0f};
            float m_angle       {1.0f};
            float m_power       {1.5f};
            ImGui::KaleidoScope_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_speed, m_angle, m_power);
        }
        break;
        case 32:
        {
            float m_threshold   {0.8f};
            int m_direction     {0};
            int m_above         {0};
            ImGui::LuminanceMelt_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_threshold, m_direction, m_above);
        }
        break;
        case 33:
        {
            float m_strength    {0.1f};
            ImGui::Morph_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_strength);
        }
        break;
        case 34:
        {
            int m_size_x        {2};
            int m_size_y        {-1};
            ImGui::Mosaic_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_size_x, m_size_y);
        }
        break;
        case 35:
        {
            ImVec2 m_direction   {-1.0, 1.0};
            ImGui::Move_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_direction.x, m_direction.y);
        }
        break;
        case 36:
        {
            ImGui::MultiplyBlend_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress);
        }
        break;
        case 37:
        {
            ImGui::PageCurl_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress);
        }
        break;
        case 38:
        {
            float m_scale       {4.0f};
            float m_smoothness  {0.01f};
            float m_seed        {12.9898f};
            ImGui::Perlin_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_scale, m_smoothness, m_seed);
        }
        break;
        case 39:
        {
            float m_speed       {2.f};
            ImGui::Pinwheel_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_speed);
        }
        break;
        case 40:
        {
            int m_size          {20};
            int m_steps         {50};
            ImGui::Pixelize_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_size, m_steps);
        }
        break;
        case 41:
        {
            int m_segments      {5};
            ImGui::Polar_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_segments);
        }
        break;
        case 42:
        {
            float m_dots        {20.f};
            ImGui::PolkaDots_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_dots);
        }
        break;
        case 43:
        {
            float m_smoothness   {1.f};
            ImGui::Radial_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_smoothness);
        }
        break;
        case 44:
        {
            float m_smoothness  {0.5f};
            int m_size          {10};
            ImGui::RandomSquares_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_smoothness, m_size);
        }
        break;
        case 45:
        {
            float m_speed       {30.f};
            float m_amplitude   {30.f};
            ImGui::Ripple_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_amplitude, m_speed);
        }
        break;
        case 46:
        {
            int m_type       {0};
            bool m_RotDown   {false};
            ImGui::Rolls_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_RotDown, m_type);
        }
        break;
        case 47:
        {
            float m_rotations   {1.f};
            float m_scale       {8.0f};
            ImPixel m_backColor {0.15f, 0.15f, 0.15f, 1.0f};
            ImGui::RotateScale_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_backColor, m_rotations, m_scale);
        }
        break;
        case 48:
        {
            float m_quickness   {0.8f};
            ImGui::SimpleZoom_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_quickness);
        }
        break;
        case 49:
        {
            int m_type {0};
            bool m_Out {true};
            ImGui::Slider_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_Out, m_type);
        }
        break;
        case 50:
        {
            float m_smoothness  {1.6f};
            int m_size          {10};
            ImVec2 m_direction  {1.0, -0.5};
            ImGui::SquaresWire_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_smoothness, m_size, m_direction.x, m_direction.y);
        }
        break;
        case 51:
        {
            float m_separation  {0.04f};
            ImGui::Squeeze_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_separation);
        }
        break;
        case 52:
        {
            float m_zoom        {0.88f};
            float m_corner_radius {0.22f};
            ImGui::StereoViewer_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_zoom, m_corner_radius);
        }
        break;
        case 53:
        {
            float m_reflection  {0.4f};
            float m_perspective {0.2f};
            float m_depth       {3.f};
            ImGui::Swap_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_reflection, m_perspective, m_depth);
        }
        break;
        case 54:
        {
            float m_radius      {1.f};
            ImGui::Swirl_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_radius);
        }
        break;
        case 55:
        {
            float m_speed       {30.f};
            float m_amplitude   {30.f};
            ImGui::WaterDrop_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_speed, m_amplitude);
        }
        break;
        case 56:
        {
            float m_size        {0.2f};
            ImGui::Wind_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_size);
        }
        break;
        case 57:
        {
            ImGui::WindowBlinds_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress);
        }
        break;
        case 58:
        {
            float m_smoothness  {1.f};
            float m_count       {10.f};
            ImGui::WindowSlice_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_smoothness, m_count);
        }
        break;
        case 59:
        {
            int m_type {0};
            ImGui::Wipe_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress, m_type);
        }
        break;
        case 60:
        {
            ImGui::ZoomInCircles_vulkan m_fusion(0);
            m_fusion.transition(mat_a, mat_b, mat_t, progress);
        }
        break;
        default: break;
    }
    g_copy->copyTo(mat_t, result, col * mat_a.w, row * mat_a.h);
}

// Application Framework Functions
void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "Transition Maker";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    //property.power_save = false;
    property.width = 1680;
    property.height = 900;

    // get opt
    if (property.argc > 1 && property.argv)
    {
        int o = -1;
        const char *option_str = "1:2:o:";
        while ((o = getopt(property.argc, property.argv, option_str)) != -1)
        {
            switch (o)
            {
                case '1': g_source_1 = std::string(optarg); break;
                case '2': g_source_2 = std::string(optarg); break;
                case 'o': g_dist = std::string(optarg); break;
                default: break;
            }
        }
        if (!g_source_1.empty())
        {
            auto pos = g_source_1.find_last_of("\\/");
            if (pos != std::string::npos)
                g_source_name_1 = g_source_1.substr(pos + 1);
            load_image(g_source_1, g_mat_1);
        }
        if (!g_source_2.empty())
        {
            auto pos = g_source_2.find_last_of("\\/");
            if (pos != std::string::npos)
                g_source_name_2 = g_source_2.substr(pos + 1);
            load_image(g_source_2, g_mat_2);
        }
        if (!g_dist.empty())
        {
            auto pos = g_dist.find_last_of("\\/");
            if (pos != std::string::npos)
                g_dist_name = g_dist.substr(pos + 1);
        }
    }
}

void Application_SetupContext(ImGuiContext* ctx)
{
#ifdef USE_BOOKMARK
    ImGuiSettingsHandler bookmark_ini_handler;
    bookmark_ini_handler.TypeName = "BookMark";
    bookmark_ini_handler.TypeHash = ImHashStr("BookMark");
    bookmark_ini_handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) -> void*
    {
        return ImGuiFileDialog::Instance();
    };
    bookmark_ini_handler.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) -> void
    {
        IGFD::FileDialog * dialog = (IGFD::FileDialog *)entry;
        dialog->DeserializeBookmarks(line);
    };
    bookmark_ini_handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
    {
        ImGuiContext& g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        auto bookmark = ImGuiFileDialog::Instance()->SerializeBookmarks();
        out_buf->appendf("[%s][##%s]\n", handler->TypeName, handler->TypeName);
        out_buf->appendf("%s\n", bookmark.c_str());
        out_buf->append("\n");
    };
    ctx->SettingsHandlers.push_back(bookmark_ini_handler);
#endif
}

void Application_Initialize(void** handle)
{

}

void Application_Finalize(void** handle)
{
    if (g_texture_1) ImGui::ImDestroyTexture(g_texture_1);
    if (g_texture_2) ImGui::ImDestroyTexture(g_texture_2);
    if (g_texture_d) ImGui::ImDestroyTexture(g_texture_d);
}

void Application_DropFromSystem(std::vector<std::string>& drops)
{

}

bool Application_Frame(void * handle, bool app_will_quit)
{
    bool app_done = false;
    auto& io = ImGui::GetIO();
    static int fusion_index = 0;
    static int fusion_col_images = 4;
    static int fusion_row_images = 4;
    static int fusion_image_index = 0;
    static int output_quality = 90;
    static void * data_memory = nullptr;
    static stbi_mem_context image_context {0, nullptr};
    static ImGui::ImMat result;
    if (!data_memory) { data_memory = malloc(4 * 1024 * 1024); image_context.last_pos = 0; image_context.context = data_memory; }
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);

    const char *filters = "Image Files(*.png){.png,.PNG},.*";
    const char *dst_filters = "Image Files(*.jpg){.jpg,.HPG},.*";
    ImVec2 minSize = ImVec2(600, 800);
	ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
    ImGuiFileDialogFlags vflags = ImGuiFileDialogFlags_CaseInsensitiveExtention | ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ShowBookmark;
    if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Choose Source One File"))
            ImGuiFileDialog::Instance()->OpenDialog("##MediaSourceDlgKey", ICON_IGFD_FOLDER_OPEN " Choose Source One File", 
                                                    filters, 
                                                    g_source_1.empty() ? "." : g_source_1,
                                                    1, IGFDUserDatas("Source 1"), vflags);
    ImGui::SameLine(0);
    ImGui::TextUnformatted(g_source_name_1.c_str());

    if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Choose Source Two File"))
            ImGuiFileDialog::Instance()->OpenDialog("##MediaSourceDlgKey", ICON_IGFD_FOLDER_OPEN " Choose Source Two File", 
                                                    filters, 
                                                    g_source_2.empty() ? "." : g_source_2,
                                                    1, IGFDUserDatas("Source 2"), vflags);
    ImGui::SameLine(0);
    ImGui::TextUnformatted(g_source_name_2.c_str());

    if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Choose Dist File"))
            ImGuiFileDialog::Instance()->OpenDialog("##MediaDistDlgKey", ICON_IGFD_FOLDER_OPEN " Choose Dist File", 
                                                    dst_filters, 
                                                    g_dist.empty() ? "." : g_dist,
                                                    1, IGFDUserDatas("Dist"), vflags | ImGuiFileDialogFlags_ConfirmOverwrite);
    ImGui::SameLine(0);
    ImGui::TextUnformatted(g_dist_name.c_str());

    if (ImGui::Button("Save Image"))
    {
        if (!g_dist.empty() && !result.empty())
        {
            stbi_write_jpg(g_dist.c_str(), result.w, result.h, result.c, result.data, output_quality);
        }
    }

    if (ImGui::Button("Generate"))
    {
        if (!g_dist.empty() && !result.empty())
        {
            image_context.last_pos = 0;
            int ret = stbi_write_jpg_to_func(custom_stbi_write_mem, &image_context, result.w, result.h, result.c, result.data, output_quality);
            if (ret)
            {
                binary_to_compressed_c("/Users/dicky/Desktop/logo.cpp", "logo", image_context.context, image_context.last_pos, g_mat_1.w, g_mat_1.h, fusion_col_images, fusion_row_images);
            }
        }
    }

    bool need_update = false;
    if (ImGui::Combo("Fusion", &fusion_index, fusion_items, IM_ARRAYSIZE(fusion_items), 30))
    {
        fusion_image_index = 0;
        need_update = true;
    }
    if (ImGui::SliderInt("Image cols", &fusion_col_images, 2, 8))
    {
        fusion_image_index = 0;
        need_update = true;
    }
    if (ImGui::SliderInt("Image rows", &fusion_row_images, 2, 8))
    {
        fusion_image_index = 0;
        need_update = true;
    }

    ImGui::SliderInt("Image quality", &output_quality, 40, 100);

    if (!g_mat_1.empty() && !g_texture_1) ImGui::ImMatToTexture(g_mat_1, g_texture_1);
    if (!g_mat_2.empty() && !g_texture_2) ImGui::ImMatToTexture(g_mat_2, g_texture_2);
    if (!g_copy) g_copy = new ImGui::CopyTo_vulkan(0);

    auto draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 FirstVideoPos = window_pos + ImVec2(4, 4);
    ImVec2 FirstVideoSize = ImVec2(window_size.x / 2 - 8, window_size.y / 4);
    ImRect FirstVideoRect(FirstVideoPos, FirstVideoPos + FirstVideoSize);
    ImVec2 SecondVideoPos = window_pos + ImVec2(window_size.x / 2 + 4, 4);
    ImVec2 SecondVideoSize = ImVec2(window_size.x / 2 - 8, window_size.y / 4);
    ImRect SecondVideoRect(SecondVideoPos, SecondVideoPos + SecondVideoSize);
    ImVec2 DistVideoPos = window_pos + ImVec2(4, FirstVideoSize.y + 32);
    ImVec2 DistVideoSize = ImVec2(window_size.x / 2 - 8, window_size.y / 4);
    ImRect DistVideoRect(DistVideoPos, DistVideoPos + DistVideoSize);
    ImVec2 DistImagePos = window_pos + ImVec2(window_size.x / 2 + 4, FirstVideoSize.y + 32);
    ImVec2 DistImageSize = ImVec2(window_size.x / 2 - 8, window_size.y / 4);
    ImRect DistImageRect(DistImagePos, DistImagePos + DistImageSize);

    ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
    ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
    float offset_x = 0, offset_y = 0;
    float tf_x = 0, tf_y = 0;
    // Draw source 1
    if (g_texture_1)
    {
        ShowVideoWindow(draw_list, g_texture_1, FirstVideoPos, FirstVideoSize, offset_x, offset_y, tf_x, tf_y, (float)g_mat_1.w / (float)g_mat_1.h);
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);
    }

    // Draw source 2
    if (g_texture_2)
    {
        ShowVideoWindow(draw_list, g_texture_2, SecondVideoPos, SecondVideoSize, offset_x, offset_y, tf_x, tf_y, (float)g_mat_2.w / (float)g_mat_2.h);
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);
    }

    // Draw result
    if (g_texture_d)
    {
        ImGui::SetCursorScreenPos(DistVideoPos);
        int col = (fusion_image_index / 4) % fusion_col_images;
        int row = (fusion_image_index / 4) / fusion_col_images;
        float start_x = (float)col / (float)fusion_col_images;
        float start_y = (float)row / (float)fusion_row_images;
        ShowVideoWindow(draw_list, g_texture_d, DistVideoPos, DistVideoSize, offset_x, offset_y, tf_x, tf_y, (float)g_mat_1.w / (float)g_mat_1.h, ImVec2(start_x, start_y), ImVec2(start_x + 1.f / (float)fusion_col_images, start_y + 1.f / (float)fusion_row_images));
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);

        fusion_image_index ++; if (fusion_image_index >= fusion_col_images * fusion_row_images * 4) fusion_image_index = 0;

        ShowVideoWindow(draw_list, g_texture_d, DistImagePos, DistImageSize, offset_x, offset_y, tf_x, tf_y, (float)ImGui::ImGetTextureWidth(g_texture_d) / (float)ImGui::ImGetTextureHeight(g_texture_d));
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);
    }

    // prepare for fusion
    if (!g_mat_1.empty() && !g_mat_2.empty() && !g_texture_d)
    {
        result.create_type(g_mat_1.w * fusion_col_images, g_mat_1.h * fusion_row_images, 4, IM_DT_INT8);
        for (int h = 0; h < fusion_row_images; h++)
        {
            for (int w = 0; w < fusion_col_images; w++)
            {
                transition(w, h, fusion_col_images, fusion_row_images, fusion_index, g_mat_1, g_mat_2, result);
            }
        }
        ImGui::ImMatToTexture(result, g_texture_d);
    }
    // handle file open
    if (ImGuiFileDialog::Instance()->Display("##MediaSourceDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
    {
        if (ImGuiFileDialog::Instance()->IsOk() == true)
        {
            auto userDatas = std::string((const char*)ImGuiFileDialog::Instance()->GetUserDatas());
            if (userDatas.compare("Source 1") == 0)
            {
                g_source_1 = ImGuiFileDialog::Instance()->GetFilePathName();
                g_source_name_1 = ImGuiFileDialog::Instance()->GetCurrentFileName();
                load_image(g_source_1, g_mat_1);
                need_update = true;
            }
            else if (userDatas.compare("Source 2") == 0)
            {
                g_source_2 = ImGuiFileDialog::Instance()->GetFilePathName();
                g_source_name_2 = ImGuiFileDialog::Instance()->GetCurrentFileName();
                load_image(g_source_2, g_mat_2);
                need_update = true;
            }
        }
        // close
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("##MediaDistDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
    {
        if (ImGuiFileDialog::Instance()->IsOk() == true)
        {
            g_dist = ImGuiFileDialog::Instance()->GetFilePathName();
            g_dist_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
        }
        // close
        ImGuiFileDialog::Instance()->Close();
    }

    if (need_update)
    {
        if (g_texture_1) { ImGui::ImDestroyTexture(g_texture_1); g_texture_1 = nullptr; }
        if (g_texture_2) { ImGui::ImDestroyTexture(g_texture_2); g_texture_2 = nullptr; }
        if (g_texture_d) { ImGui::ImDestroyTexture(g_texture_d); g_texture_d = nullptr; }
    }

    ImGui::End();
    if (app_will_quit)
    {
        if (g_texture_1) { ImGui::ImDestroyTexture(g_texture_1); g_texture_1 = nullptr; }
        if (g_texture_2) { ImGui::ImDestroyTexture(g_texture_2); g_texture_2 = nullptr; }
        if (g_texture_d) { ImGui::ImDestroyTexture(g_texture_d); g_texture_d = nullptr; }
        if (g_copy) { delete g_copy; g_copy = nullptr; }
        if (data_memory) { free(data_memory); data_memory = nullptr; }
        app_done = true;
    }
    return app_done;
}
