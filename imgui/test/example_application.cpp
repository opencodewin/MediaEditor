#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_helper.h>
#include <application.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cerrno>
#include <imgui_cpu.h>

#include <immat.h>
#include "Config.h"
#if !IMGUI_ICONS
#define ICON_TRUE "T"
#define ICON_FALSE "F"
#else
#define ICON_TRUE u8"\ue5ca"
#define ICON_FALSE u8"\ue5cd"
#endif

static inline void box(ImGui::ImMat& image, int x1, int y1, int x2, int y2, int R, int G, int B)
{
    for (int j = y1; j <= y2; j++)
    {
        for (int i = x1; i <= x2; i++)
        {
            //unsigned int color = 0xFF000000 | (R << 16) | (G << 8) | B;
            //image.at<unsigned int>(i, j) = color;
            image.at<unsigned char>(i, j, 3) = 0xFF;
            image.at<unsigned char>(i, j, 2) = B;
            image.at<unsigned char>(i, j, 1) = G;
            image.at<unsigned char>(i, j, 0) = R;
        }
    }
}

static inline void color_bar(ImGui::ImMat& image, int x1, int y1, int x2, int y2)
{
    const unsigned char r[8] = {255,255,0,0,255,255,0,0};
    const unsigned char g[8] = {255,255,255,255,0,0,0,0};
    const unsigned char b[8] = {255,0,255,0,255,0,255,0};
    int len = x2 - x1 + 1;
    for (int i = 0; i < 8; i++)
    {
        box(image, x1 + len * i / 8, y1, x1 + len * (i + 1) / 8 - 1, y2, r[i], g[i], b[i]);
    }
}

static inline void gray_bar(ImGui::ImMat& image, int x1,int y1,int x2,int y2,int step)
{
    int len = x2 - x1 + 1;
    for (int i = 0; i < step; i++)
    {
        box(image, x1 + len * i / step, y1, x1 + len * (i + 1) / step - 1, y2, 255 * i / step, 255 * i / step, 255 * i / step);
    }
}

class Example
{
public:
    Example() 
    {
        // init color inspact
        color_bar(image, 0, 0, 255, 191);
        gray_bar(image, 0, 192, 255, 255, 13);
        // init draw mat
        draw_mat.clean(ImPixel(0.f, 0.f, 0.f, 1.f));
    };
    ~Example() 
    {
        ImGui::ImDestroyTexture(&ImageTexture);
        ImGui::ImDestroyTexture(&DrawMatTexture);
        ImGui::ImDestroyTexture(&CustomDrawTexture);
    }

public:
    bool show_demo_window = true;
    bool show_cpu_info = false;
    bool show_viewport_fullscreen = false;
    bool show_another_window = false;
    bool show_mat_draw_window = false;
    bool show_mat_fish_circle_draw = false;
    bool show_mat_rotate_window = false;
    bool show_mat_warp_matrix = false;

public:
    void DrawMatDemo();
    void DrawFishCircleDemo();
    void DrawRotateDemo();
    void WarpMatrixDemo();

public:
    ImGui::ImMat image {ImGui::ImMat(256, 256, 4, 1u, 4)};
    ImGui::ImMat draw_mat {ImGui::ImMat(512, 512, 4, 1u, 4)};
    ImGui::ImMat float_mat {ImGui::ImMat(128, 128, 4, 4u, 4)};
    ImGui::ImMat rgb_mat {ImGui::ImMat(128, 128, 3, 1u, 3)};
    ImGui::ImMat custom_mat {ImGui::ImMat(2048, 2048, 4, 1u, 4)};
    ImTextureID ImageTexture = 0;
    ImTextureID DrawMatTexture = 0;
    ImTextureID SmallTexture = 0;
    ImTextureID CustomDrawTexture = 0;
};

void Example::DrawMatDemo()
{
    float t = (float)ImGui::GetTime();
    float h = abs(sin(t * 0.2));
    float s = abs(sin(t * 0.1)) * 0.5 + 0.4;
    float h2 = abs(sin(t * 0.4));
    float h3 = abs(sin(t * 0.8));
    static int offset_x = 0;
    static int offset_y = 0;
    static int step_x = 2;
    static int step_y = 3;
    ImVec4 base_color = ImVec4(0.f, 0.f, 0.f, 1.f);
    ImVec4 light_color = ImVec4(0.f, 0.f, 0.f, 1.f);
    ImVec4 t_color = ImVec4(0.f, 0.f, 0.f, 1.f);
    ImGui::ColorConvertHSVtoRGB(h, s, 0.5f, base_color.x, base_color.y, base_color.z);
    ImGui::ColorConvertHSVtoRGB(h2, s, 0.5f, light_color.x, light_color.y, light_color.z);
    ImGui::ColorConvertHSVtoRGB(h3, s, 0.5f, t_color.x, t_color.y, t_color.z);
    static float arc = 0.0;
    draw_mat.clean(ImPixel(0.f, 0.f, 0.f, 1.f));
    arc += 2 * M_PI / 64 / 32;
    if (arc > 2 * M_PI / 64) arc = 0;
    float cx = draw_mat.w * 0.5f, cy = draw_mat.h * 0.5f;
    ImPixel line_color(base_color.x, base_color.y, base_color.z, 1.f);
    ImPixel circle_color(light_color.x, light_color.y, light_color.z, 1.f);
    ImPixel text_color(t_color.x, t_color.y, t_color.z, 1.f);
    float_mat.clean(text_color);
    float_mat.draw_line(ImPoint(0, 0), ImPoint(float_mat.w - 1, 0), 1, ImPixel(0, 0, 0, 1));
    float_mat.draw_line(ImPoint(0, 0), ImPoint(0, float_mat.h - 1), 1, ImPixel(0, 0, 0, 1));
    float_mat.draw_line(ImPoint(float_mat.w - 1, 0), ImPoint(float_mat.w - 1, float_mat.h - 1), 1, ImPixel(0, 0, 0, 1));
    float_mat.draw_line(ImPoint(0, float_mat.h - 1), ImPoint(float_mat.w - 1, float_mat.h - 1), 1, ImPixel(0, 0, 0, 1));
    float_mat.draw_line(ImPoint(float_mat.w / 2, 0), ImPoint(float_mat.w / 2, float_mat.h - 1), 1, ImPixel(0, 0, 0, 1));
    float_mat.draw_line(ImPoint(0, float_mat.h / 2), ImPoint(float_mat.w - 1, float_mat.h / 2), 1, ImPixel(0, 0, 0, 1));
    rgb_mat.clean(text_color);

    // draw line test
    for (int j = 0; j < 5; j++) 
    {
        float r1 = fminf(draw_mat.w, draw_mat.h) * (j + 0.5f) * 0.085f;
        float r2 = fminf(draw_mat.w, draw_mat.h) * (j + 1.5f) * 0.085f;
        float t = j * M_PI / 64.0f, r = (j + 1) * 0.5f;
        for (int i = 1; i <= 64; i++, t += 2.0f * M_PI / 64.0f)
        {
            float ct = cosf(t + arc), st = sinf(t + arc);
            draw_mat.draw_line(ImPoint(cx + r1 * ct, cy - r1 * st), ImPoint(cx + r2 * ct, cy - r2 * st), r, line_color);
        }
    }

    // draw circle test(smooth) 
    for (int j = 0; j < 5; j++)
    {
        float r = fminf(draw_mat.w, draw_mat.h) * (j + 1.5f) * 0.085f + 1;
        float t = (j + 1) * 0.5f;
        draw_mat.draw_circle(draw_mat.w / 2, draw_mat.h / 2, r, t, circle_color);
    }

    // draw circle test
    draw_mat.draw_circle(draw_mat.w / 2, draw_mat.h / 2, draw_mat.w / 2 - 1, ImPixel(1.0, 1.0, 1.0, 1.0));

    std::string text_str = "字体测试\nFont Test\n字体Test\nFont测试";
    ImGui::DrawTextToMat(draw_mat, ImPoint(50, 50), text_str.c_str(), text_color);

    // ImMat crop
    ImGui::ImMat crop_img = draw_mat.crop(ImPoint(20, 20), ImPoint(200, 200));
    ImGui::ImageMatCopyTo(crop_img, draw_mat, ImPoint(draw_mat.w - 180, draw_mat.h - 180));
    draw_mat.draw_rectangle(ImPoint(20, 20), ImPoint(200, 200), ImPixel(0, 1, 0, 1));
    draw_mat.draw_rectangle(ImPoint(draw_mat.w - 180, draw_mat.h - 180), ImPoint(draw_mat.w, draw_mat.h), ImPixel(0, 1, 0, 1));

    // ImMat resize
    ImGui::ImMat resize_img = ImGui::MatResize(crop_img, ImSize(112, 112));
    ImGui::ImageMatCopyTo(resize_img, draw_mat, ImPoint(draw_mat.w - 112, draw_mat.h - 112));
    draw_mat.draw_rectangle(ImPoint(draw_mat.w - 112, draw_mat.h - 112), ImPoint(draw_mat.w, draw_mat.h), ImPixel(1, 1, 1, 1));

    // ImMat.resize
    ImGui::ImMat resize_img2 = crop_img.resize(112, 112);
    ImGui::ImageMatCopyTo(resize_img2, draw_mat, ImPoint(draw_mat.w - 224, draw_mat.h - 224));
    draw_mat.draw_rectangle(ImPoint(draw_mat.w - 224, draw_mat.h - 224), ImPoint(draw_mat.w - 112, draw_mat.h - 112), ImPixel(1, 1, 0, 1));

    // ImMat WarpAffine
    const float o_size = 96;
    ImGui::ImMat src_matrix(2,4);
    src_matrix.at<float>(0,0) = 0.25 * o_size; src_matrix.at<float>(1, 0) = 0.25 * o_size;
    src_matrix.at<float>(0,1) = 0.75 * o_size; src_matrix.at<float>(1, 1) = 0.25 * o_size;
    src_matrix.at<float>(0,2) = 0.25 * o_size; src_matrix.at<float>(1, 2) = 0.75 * o_size;
    src_matrix.at<float>(0,3) = 0.75 * o_size; src_matrix.at<float>(1, 3) = 0.75 * o_size;
    ImGui::ImMat dst_matrix(2,4);
    dst_matrix.at<float>(0, 0) = 128 + 40; dst_matrix.at<float>(1, 0) = 128 + 20;
    dst_matrix.at<float>(0, 1) = 256 - 20; dst_matrix.at<float>(1, 1) = 128 + 40;
    dst_matrix.at<float>(0, 2) = 128 + 20; dst_matrix.at<float>(1, 2) = 256 - 20;
    dst_matrix.at<float>(0, 3) = 256 - 40; dst_matrix.at<float>(1, 3) = 256 - 20;
    ImGui::ImMat M = ImGui::similarTransform(dst_matrix, src_matrix);
    ImGui::ImMat affine_img = ImGui::MatWarpAffine(draw_mat, M, ImSize(o_size, o_size));
    ImGui::ImageMatCopyTo(affine_img, draw_mat, ImPoint(0, draw_mat.h - o_size));
    draw_mat.draw_rectangle(ImPoint(0, draw_mat.h - o_size), ImPoint(o_size, draw_mat.h), ImPixel(1, 0, 0, 1));
    draw_mat.draw_rectangle(ImPoint(128, 128), ImPoint(256, 256), ImPixel(1, 0, 0, 1));

    // ImMat WarpPerspective
    ImGui::ImMat src_matrix_p(2,4);
    src_matrix_p.at<float>(0,0) = 0.0 * resize_img2.w; src_matrix_p.at<float>(1, 0) = 0.0 * resize_img2.h;
    src_matrix_p.at<float>(0,1) = 1.0 * resize_img2.w; src_matrix_p.at<float>(1, 1) = 0.0 * resize_img2.h;
    src_matrix_p.at<float>(0,2) = 1.0 * resize_img2.w; src_matrix_p.at<float>(1, 2) = 1.0 * resize_img2.h;
    src_matrix_p.at<float>(0,3) = 0.0 * resize_img2.w; src_matrix_p.at<float>(1, 3) = 1.0 * resize_img2.h;
    ImGui::ImMat dst_matrix_p(2,4);
    dst_matrix_p.at<float>(0, 0) = 0.2 * resize_img2.w; dst_matrix_p.at<float>(1, 0) = 0.1 * resize_img2.h;
    dst_matrix_p.at<float>(0, 1) = 0.8 * resize_img2.w; dst_matrix_p.at<float>(1, 1) = 0.1 * resize_img2.h;
    dst_matrix_p.at<float>(0, 2) = 0.9 * resize_img2.w; dst_matrix_p.at<float>(1, 2) = 0.9 * resize_img2.h;
    dst_matrix_p.at<float>(0, 3) = 0.1 * resize_img2.w; dst_matrix_p.at<float>(1, 3) = 0.8 * resize_img2.h;
    ImGui::ImMat M_p = ImGui::getPerspectiveTransform(dst_matrix_p, src_matrix_p);
    ImGui::ImMat affine_img_p = ImGui::MatWarpPerspective(resize_img2, M_p, ImSize(resize_img2.w, resize_img2.h), IM_INTERPOLATE_BICUBIC);
    ImGui::ImageMatCopyTo(affine_img_p, draw_mat, ImPoint(draw_mat.w - resize_img2.w, 0));
    draw_mat.draw_rectangle(ImPoint(draw_mat.w - resize_img2.w, 0), ImPoint(draw_mat.w, resize_img2.h), ImPixel(1, 0, 0, 1));

    // ImMat blur
    ImGui::ImMat blur_mat = affine_img.blur(3);
    ImGui::ImageMatCopyTo(blur_mat, draw_mat, ImPoint(o_size, draw_mat.h - o_size));

    // ImMat adaptive_threshold
    ImGui::ImMat gray_mat = affine_img.cvtToGray();
    ImGui::ImMat gray_threshold_mat = gray_mat.adaptive_threshold(1.0, gray_mat.w / 10, 0.1);
    ImGui::ImMat threshold_mat = gray_threshold_mat.cvtToRGB(IM_CF_ABGR, IM_DT_INT8, false);
    ImGui::ImageMatCopyTo(threshold_mat, draw_mat, ImPoint(0, draw_mat.h - o_size * 2));

    // ImMat findContours
    static std::vector<std::vector<ImPoint>> contours;
    ImGui::findContours(gray_mat, contours);
    for( size_t i = 0; i< contours.size(); i++ )
    {
        for (int j = 0; j < contours[i].size() - 1; ++j)
        {
            draw_mat.draw_line(contours[i][j], contours[i][j + 1], ImPixel(0.0, 1.0, 0.0, 1.0), 1);
        }
        draw_mat.draw_line(contours[i][contours[i].size() - 1], contours[i][0], ImPixel(0.0, 1.0, 0.0, 1.0), 1);
    }

    draw_mat.draw_rectangle(ImPoint(0, 0), ImPoint(draw_mat.w, draw_mat.h), ImPixel(0, 0, 1, 1));
    ImGui::ImMatToTexture(draw_mat, DrawMatTexture);
    
    // mat copy to texture
    ImGui::ImCopyToTexture(DrawMatTexture, (unsigned char*)&float_mat, float_mat.w, float_mat.h, float_mat.c, offset_x, offset_y, true);

    offset_x += step_x;
    offset_y += step_y;
    if (offset_x < 0 || offset_x + float_mat.w >= draw_mat.w) { step_x = -step_x; offset_x += step_x; }
    if (offset_y < 0 || offset_y + float_mat.h >= draw_mat.h) { step_y = -step_y; offset_y += step_y; }
    
    ImGui::Image(DrawMatTexture, ImVec2(draw_mat.w, draw_mat.h));

    ImGui::ImMatToTexture(rgb_mat, SmallTexture);
    if (SmallTexture) ImGui::Image(SmallTexture, ImVec2(rgb_mat.w, rgb_mat.h));
}

void Example::DrawRotateDemo()
{
    static float angle = 360.0;
    angle--; if (angle < 0) angle = 360;
    draw_mat.clean(ImPixel(0.f, 0.f, 0.f, 0.f));
    float cx = draw_mat.w * 0.5f, cy = draw_mat.h * 0.5f;
    for (int j = 0; j < 5; j++)
    {
        float r = fminf(draw_mat.w, draw_mat.h) * (j + 1.5f) * 0.085f + 1;
        float t = (j + 1) * 0.5f;
        draw_mat.draw_circle(cx, cy, r, t, ImPixel(1, 0, 0, 1));
    }
    // draw rotate text test
    auto tmat = ImGui::CreateTextMat("Mat", ImPixel(1,1,0,1), ImPixel(0,0,0,0), 1.0, true);
    auto trmat = ImGui::MatRotate(tmat, angle);
    ImGui::ImageMatCopyTo(trmat, draw_mat, ImPoint(cx - trmat.w / 2, cy - trmat.h / 2));
    ImGui::ImMatToTexture(draw_mat, DrawMatTexture);
    ImGui::Image(DrawMatTexture, ImVec2(draw_mat.w, draw_mat.h));
}

void Example::DrawFishCircleDemo()
{
    //static bool orthographic = false;
    static int graphic_type = 0;
    bool change = false;
    change |= ImGui::RadioButton("Orthographic",  &graphic_type, 0); ImGui::SameLine();
    change |= ImGui::RadioButton("Equidistance",  &graphic_type, 1); ImGui::SameLine();
    change |= ImGui::RadioButton("Equisolid",  &graphic_type, 2); ImGui::SameLine();
    change |= ImGui::RadioButton("Stereographic",  &graphic_type, 3);
    ImGui::ImDestroyTexture(&CustomDrawTexture);
    auto HSVtoRGB = [](float t)
    {
        float r, g, b;
		ImGui::ColorConvertHSVtoRGB(t, 1.0f, 1.0f, r, g, b);
        return ImPixel(r, g, b, 1.0);
    };
    if (!CustomDrawTexture)
    {
        float rtc[10];
        custom_mat.clean(ImPixel(0.f, 0.f, 0.f, 1.f));
        float r = custom_mat.w / 2 - 0.5;
        for (int i = 0; i <= 90; i+=10)
        {
            float rd;
            switch (graphic_type)
            {
                case 0: rd = r * sin(i * M_PI / 180.0); break;
                case 1: rd = r * i / 90.0; break;
                case 2: rd = sqrt(2) * r * sin(i * M_PI / 360.0); break;
                case 3: rd = r * tan(i * M_PI / 360.0); break;
                default: rd = r * i / 90.0; break;
            }
            rtc[i / 10] = rd;
            custom_mat.draw_circle(custom_mat.w / 2 - 0.5, custom_mat.h / 2 - 0.5, rd, 1.0f, HSVtoRGB);
        }
        float t = 0;
        float cx = custom_mat.w * 0.5f - 0.5, cy = custom_mat.h * 0.5f - 0.5;
        for (int i = 0; i < 36; i++, t += 2.0f * M_PI / 36.0f)
        {
            float const t0 = fmodf(((float)(i)) / ((float)36), 1.0f);
            float ct = cosf(t), st = sinf(t);
            custom_mat.draw_line(ImPoint(cx + rtc[1] * ct, cy - rtc[1] * st), ImPoint(cx + r * ct, cy - r * st), 1.0, HSVtoRGB(t0));
            switch (i)
            {
                case 0:
                {
                    for (int tc = 1; tc <= 9; tc++)
                    {
                        auto tmat = ImGui::CreateTextMat(std::to_string(tc * 10).c_str(), ImPixel(1,1,1,1), ImPixel(0,0,0,0), 1.0, true);
                        auto trmat = ImGui::MatRotate(tmat, 270);
                        ImGui::ImageMatCopyTo(trmat, custom_mat, ImPoint(cx + rtc[tc] * ct - 32, cy - rtc[tc] * st - 16));
                    }
                }
                break;
                case 9 :
                {
                    for (int tc = 1; tc <= 9; tc++)
                    {
                        auto tmat = ImGui::CreateTextMat(std::to_string(tc * 10).c_str(), ImPixel(1,1,1,1), ImPixel(0,0,0,0), 1.0);
                        auto trmat = ImGui::MatRotate(tmat, 180);
                        ImGui::ImageMatCopyTo(trmat, custom_mat, ImPoint(cx + rtc[tc] * ct - 16, cy - rtc[tc] * st + 8));
                    }
                }
                break;
                case 18:
                {
                    for (int tc = 1; tc <= 9; tc++)
                    {
                        auto tmat = ImGui::CreateTextMat(std::to_string(tc * 10).c_str(), ImPixel(1,1,1,1), ImPixel(0,0,0,0), 1.0, true);
                        auto trmat = ImGui::MatRotate(tmat, 90);
                        ImGui::ImageMatCopyTo(trmat, custom_mat, ImPoint(cx + rtc[tc] * ct, cy - rtc[tc] * st - 16));
                    }
                }
                break;
                case 27 : 
                {
                    for (int tc = 1; tc <= 9; tc++)
                    {
                        auto tmat = ImGui::CreateTextMat(std::to_string(tc * 10).c_str(), ImPixel(1,1,1,1), ImPixel(0,0,0,0), 1.0);
                        ImGui::ImageMatCopyTo(tmat, custom_mat, ImPoint(cx + rtc[tc] * ct - 16, cy - rtc[tc] * st - 24));
                    }
                }
                break;
                default : break;
            }

            int degree = (i + 9) % 36 * 10;
            char degree_str[4];
            snprintf(degree_str, 4, "%03d", degree);
            auto tmat = ImGui::CreateTextMat(degree_str, ImPixel(1,1,1,1), ImPixel(0,0,0,0), 1.0);
            ImGui::ImMat dmat(tmat.w, tmat.h * 3, 4, 1u, 4);
            ImGui::ImageMatCopyTo(tmat, dmat, ImPoint(0, tmat.h * 1.5));
            auto trmat = ImGui::MatRotate(dmat, 360 - degree);
            
            // draw in circle degree (Orthographic 30°， others 40°)
            int tc_in = graphic_type == 0 ? 3 : 4;
            ImGui::ImageMatCopyTo(trmat, custom_mat, ImPoint(cx + rtc[tc_in] * ct - 24, cy - rtc[tc_in] * st - 24));
            // draw out circle degree (Orthographic 70°， others 80°)
            int tc_out = graphic_type == 0 ? 7 : 8;
            ImGui::ImageMatCopyTo(trmat, custom_mat, ImPoint(cx + rtc[tc_out] * ct - 24, cy - rtc[tc_out] * st - 24));
        }
        
        ImGui::ImMatToTexture(custom_mat, CustomDrawTexture);
    }
    if (CustomDrawTexture)
    {
        ImGui::ImShowVideoWindow(ImGui::GetWindowDrawList(), CustomDrawTexture, ImGui::GetCursorScreenPos(), ImVec2(1024, 1024));
    }
}

void Example::WarpMatrixDemo()
{
    const float width = 1920.f;
    const float height = 1080.f;
    ImPoint src_corners[4];
    ImPoint dst_corners[4];
    src_corners[0] = ImPoint(width / 1.80, height / 4.20);
    src_corners[1] = ImPoint(width / 1.15, height / 3.32);
    src_corners[2] = ImPoint(width / 1.33, height / 1.10);
    src_corners[3] = ImPoint(width / 1.93, height / 1.36);
    dst_corners[0] = ImPoint(0, 0);
    dst_corners[1] = ImPoint(width, 0);
    dst_corners[2] = ImPoint(width, height);
    dst_corners[3] = ImPoint(0, height);
    ImGui::ImMat M0 = ImGui::getPerspectiveTransform(dst_corners, src_corners);
    ImGui::ImMat M1 = ImGui::getAffineTransform(dst_corners, src_corners);
    for (int i = 0; i < 4; i++)
    {
        ImGui::Text("d: x=%.2f y=%.2f", dst_corners[i].x, dst_corners[i].y);
        ImGui::SameLine(200);
        ImGui::Text("s: x=%.2f y=%.2f", src_corners[i].x, src_corners[i].y);
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Perspective Transform:");
    for (int h = 0; h < M0.h; h++)
    {
        for (int w = 0; w < M0.w; w++)
        {
            ImGui::Text("%.2f", M0.at<float>(w, h));
            if ( w <  M0.w - 1)
                ImGui::SameLine((w + 1) * 100);
        }
    }
    ImGui::TextUnformatted("Affine Transform:");
    for (int h = 0; h < M1.h; h++)
    {
        for (int w = 0; w < M1.w; w++)
        {
            ImGui::Text("%.2f", M1.at<float>(w, h));
            if ( w <  M1.w - 1)
                ImGui::SameLine((w + 1) * 100);
        }
    }
}

void Example_Initialize(void** handle)
{
    srand((unsigned int)time(0));
    *handle = new Example();
    Example * example = (Example *)*handle;
}

void Example_Finalize(void** handle)
{
    if (handle && *handle)
    {
        Example * example = (Example *)*handle;
        delete example;
        *handle = nullptr;
    }
}

bool Example_Frame(void* handle, bool app_will_quit)
{
    bool app_done = false;
    auto& io = ImGui::GetIO();
    Example * example = (Example *)handle;
    if (!example)
        return true;

    if (!example->ImageTexture) 
    {
        example->ImageTexture = ImGui::ImCreateTexture(example->image.data, example->image.w, example->image.h, example->image.c);
    }
    // Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (example->show_demo_window)
        ImGui::ShowDemoWindow(&example->show_demo_window);

    if (example->show_cpu_info)
        ImGui::CPUInfo();

    // Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &example->show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("CPU info", &example->show_cpu_info);            // Edit bools Show CPU info
        if (ImGui::Checkbox("Full Screen Window", &example->show_viewport_fullscreen))
        {
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                auto platformio = ImGui::GetPlatformIO();
                //if (platformio.Platform_FullScreen) platformio.Platform_FullScreen(ImGui::GetMainViewport(), example->show_viewport_fullscreen);
                if (platformio.Platform_FullScreen) platformio.Platform_FullScreen(ImGui::GetWindowViewport(), example->show_viewport_fullscreen);
            }
            else
                Application_FullScreen(example->show_viewport_fullscreen);
        }
        ImGui::Checkbox("Another Window", &example->show_another_window);
        ImGui::Checkbox("ImMat Draw Window", &example->show_mat_draw_window);
        ImGui::Checkbox("ImMat Rotate Window", &example->show_mat_rotate_window);
        ImGui::Checkbox("ImMat Draw Fish circle Window", &example->show_mat_fish_circle_draw);
        ImGui::Checkbox("ImMat Warp Matrix", &example->show_mat_warp_matrix);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", ImGui::GetIO().DeltaTime * 1000.f, ImGui::GetIO().Framerate);
        ImGui::Text("Frames since last input: %d", ImGui::GetIO().FrameCountSinceLastUpdate);
        ImGui::Text("Time Date: %s", ImGuiHelper::date_time_string().c_str());
        ImGui::Separator();
        ImGui::Text("User Name: %s", ImGuiHelper::username().c_str());
        ImGui::Text("Home path: %s", ImGuiHelper::home_path().c_str());
        ImGui::Text("Temp path: %s", ImGuiHelper::temp_path().c_str());
        ImGui::Text("Working path: %s", ImGuiHelper::cwd_path().c_str());
        ImGui::Text("Exec path: %s", ImGuiHelper::exec_path().c_str());
        ImGui::Text("Setting path: %s", ImGuiHelper::settings_path("ImGui Example").c_str());
        ImGui::Separator();
        ImGui::Text("DataHome path: %s", ImGuiHelper::getDataHome().c_str());
        ImGui::Text("Config path: %s", ImGuiHelper::getConfigHome().c_str());
        ImGui::Text("Cache path: %s", ImGuiHelper::getCacheDir().c_str());
        ImGui::Text("State path: %s", ImGuiHelper::getStateDir().c_str());
        ImGui::Text("Desktop path: %s", ImGuiHelper::getDesktopFolder().c_str());
        ImGui::Text("Documents path: %s", ImGuiHelper::getDocumentsFolder().c_str());
        ImGui::Text("Download path: %s", ImGuiHelper::getDownloadFolder().c_str());
        ImGui::Text("Pictures path: %s", ImGuiHelper::getPicturesFolder().c_str());
        ImGui::Text("Public path: %s", ImGuiHelper::getPublicFolder().c_str());
        ImGui::Text("Music path: %s", ImGuiHelper::getMusicFolder().c_str());
        ImGui::Text("Video path: %s", ImGuiHelper::getVideoFolder().c_str());
        ImGui::Separator();
        ImGui::Text("Memory usage: %zu", ImGuiHelper::memory_usage());
        ImGui::Text("Memory Max usage: %zu", ImGuiHelper::memory_max_usage());
        ImGui::End();
    }

    // Show another simple window.
    if (example->show_another_window)
    {
        ImGui::Begin("Another Window", &example->show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            example->show_another_window = false;
        ImGui::End();
    }

    // Show ImMat line/circle demo
    if (example->show_mat_draw_window)
    {
        ImGui::SetNextWindowSize(ImVec2(512, 512), ImGuiCond_FirstUseEver);
        ImGui::Begin("ImMat draw Demo", &example->show_mat_draw_window);
        example->DrawMatDemo();
        ImGui::End();
    }

    // Show ImMat Rotate demo
    if (example->show_mat_rotate_window)
    {
        ImGui::SetNextWindowSize(ImVec2(512, 512), ImGuiCond_FirstUseEver);
        ImGui::Begin("ImMat Rotate Demo", &example->show_mat_rotate_window);
        example->DrawRotateDemo();
        ImGui::End();
    }
    // Show ImMat draw fish circle demo
    if (example->show_mat_fish_circle_draw)
    {
        ImGui::SetNextWindowSize(ImVec2(1024, 1024), ImGuiCond_FirstUseEver);
        ImGui::Begin("ImMat Draw Fish circle Demo", &example->show_mat_fish_circle_draw);
        example->DrawFishCircleDemo();
        ImGui::End();
    }

    // Show ImMat warp matrix demo
    if (example->show_mat_warp_matrix)
    {
        ImGui::SetNextWindowSize(ImVec2(512, 512), ImGuiCond_FirstUseEver);
        ImGui::Begin("ImMat warp matrix Demo", &example->show_mat_warp_matrix);
        example->WarpMatrixDemo();
        ImGui::End();
    }

    if (app_will_quit)
        app_done = true;
    return app_done;
}

bool Example_Splash_Screen(void* handle, bool& app_will_quit)
{
    const int delay = 20;
    static int x = 0;
    auto& io = ImGui::GetIO();
    ImGuiCond cond = ImGuiCond_None;
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | 
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize, cond);
    ImGui::Begin("Content", nullptr, flags);
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32_WHITE);
    ImGui::SetWindowFontScale(2.0);
    std::string str = "Example Splash";
    auto mark_size = ImGui::CalcTextSize(str.c_str());
    float xoft = (io.DisplaySize.x - mark_size.x) / 2;
    float yoft = (io.DisplaySize.y - mark_size.y - 32) / 2;
    ImGui::GetWindowDrawList()->AddText(ImVec2(xoft, yoft), IM_COL32_BLACK, str.c_str());
    ImGui::SetWindowFontScale(1.0);

    ImGui::End();

    if (x < delay)
    {
        ImGui::sleep(1);
        x++;
        return false;
    }
    return true;
}

static void Example_SetupContext(ImGuiContext* ctx, void* handle, bool in_splash)
{
    if (!ctx)
        return;
#ifdef USE_PLACES_FEATURE
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
        if (dialog) dialog->DeserializePlaces(line);
    };
    bookmark_ini_handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
    {
        ImGuiContext& g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        auto bookmark = ImGuiFileDialog::Instance()->SerializePlaces();
        out_buf->appendf("[%s][##%s]\n", handler->TypeName, handler->TypeName);
        out_buf->appendf("%s\n", bookmark.c_str());
        out_buf->append("\n");
    };
    ctx->SettingsHandlers.push_back(bookmark_ini_handler);
#endif
}

void Application_Setup(ApplicationWindowProperty& property)
{
    property.name = "Application_Example";
    property.font_scale = 2.0f;
    property.splash_screen_width = 600;
    property.splash_screen_height = 300;
    property.splash_screen_alpha = 0.9;
    property.application.Application_SetupContext = Example_SetupContext;
    property.application.Application_Initialize = Example_Initialize;
    property.application.Application_Finalize = Example_Finalize;
    property.application.Application_Frame = Example_Frame;
    property.application.Application_SplashScreen = Example_Splash_Screen;
}
