#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <immat.h>
#include "bmp.h"
#include "libdither.h"


/* A quick demo of ditherlib.
 * This program loads a .bmp file, converts it to linear grayscale, and runs all available ditherers on it,
 * saving the output to individual .bmp files.
 */

void save_and_free_image(char* basename, std::string filename, ImGui::ImMat& out) {
    // The out buffer is a simple array with the length 'width * height', with one byte-per pixel.
    // If a byte in the buffer is 0 then it's black. 255 is white.
    Bmp* bmp = bmp_rgb24(out.w, out.h);
    Pixel p;
    p.r = 255; p.g = 255; p.b = 255;
    for(int y = 0; y < out.h; y++)
        for (int x = 0; x < out.w; x++)
            if (out.at<uint8_t>(x, y)!= 0)
                bmp_setpixel(bmp, x, y, p);
    char fn[2048];
    snprintf(fn, 2048, "%s_%s", basename, filename.c_str());
    bmp_save(bmp, fn);
    bmp_free(bmp);
}

ImGui::ImMat bmp_to_ditherimage(char *filename, bool correct_gamma) {
    // load a BMP image
    ImGui::ImMat dither_image;
    Bmp* bmp = bmp_load(filename);
    if (!bmp)
        return dither_image;
    // create an empty DitherImage. DitherImages are in linear color space, storing color values in the 0.0 - 1.0 range
    // DitherImages are the input for all dither functions.

    Pixel p;
    dither_image.create_type(bmp->width, bmp->height, 1, IM_DT_INT8);
    // set pixel in the dither image
    for(int y = 0; y < bmp->height; y++) {
        for (int x = 0; x < bmp->width; x++) {
            // here we read the source pixels from the BMP image. The source pixels are in sRGB color space and the
            // color values range from 0 - 255.
            bmp_getpixel(bmp, x, y, &p);
            // DitherImage_set_pixel converts the color space from sRGB to linear and adjusts the value range from
            // 0 - 255 of the input image to DitherImage's 0.0 - 1.0 range.
            float gray = p.r * 0.299 + p.g * 0.587 + p.b * 0.114;
            dither_image.at<uint8_t>(x, y) = (uint8_t)gray;
            //DitherImage_set_pixel(dither_image, x, y, p.r, p.g, p.b, correct_gamma);
        }
    }
    bmp_free(bmp); // free the input BMP image - we don't need it anymore.
    return dither_image;
}

void strip_ext(char* fname) {
    /* strips away a file's extension */
    char *end = fname + strlen(fname);
    while (end > fname && *end != '.')
        --end;
    if (end > fname)
        *end = '\0';
}

int main(int argc, char* argv[]) {
    if(argc < 3) {
        printf("USAGE: demo image.bmp noise.bmp\n");
        return 0;
    }
    char* filename = argv[1];
    char* noise_filename = argv[2];
    char* basename = (char*)calloc(strlen(argv[1]) + 1, sizeof(char));
    strcpy(basename, argv[1]);
    strip_ext(basename);
    ImGui::ImMat dither_image = bmp_to_ditherimage(filename, true);  // load an image as DitherImage
    if(!dither_image.empty()) {
        /* All ditherers take the input DitherImage as first parameter, and the output buffer ('out') as last parameter.
         * Ditherers will never modify the DitherImage input.
         * The expected output buffer is a flat buffer with 1-byte-per-pixel and a lenght of image-width * image-height.
         * Black pixels in the buffer have a value of 0 and white pixels have a value of 255. */
        ImGui::ImMat out_image;
        out_image.create_like(dither_image);
        /* Grid Dithering */
        printf("running Grid Ditherer...\n");
        grid_dither(dither_image, 4, 4, 0, true, out_image);
        save_and_free_image(basename, "grid.bmp", out_image);

        /* Dot Diffusion */
        printf("running Dot Diffusion: Knuth...\n");
        out_image.fill(0);
        dot_diffusion_dither(dither_image, DD_KNUTH_CLASS, out_image);
        save_and_free_image(basename, "dd_knuth.bmp", out_image);

        printf("running Dot Diffusion: Mini-Knuth...\n");
        out_image.fill(0);
        dot_diffusion_dither(dither_image, DD_MINI_KNUTH_CLASS, out_image);
        save_and_free_image(basename, "dd_mini-knuth.bmp", out_image);

        printf("running Dot Diffusion: Optimized Knuth...\n");
        out_image.fill(0);
        dot_diffusion_dither(dither_image, DD_OPTIMIZED_KNUTH_CLASS, out_image);
        save_and_free_image(basename, "dd_opt-knuth.bmp", out_image);

        printf("running Dot Diffusion: Mese and Vaidyanathan 8x8...\n");
        out_image.fill(0);
        dot_diffusion_dither(dither_image, DD_MESE_8X8_CLASS, out_image);
        save_and_free_image(basename, "dd_mese8x8.bmp", out_image);

        printf("running Dot Diffusion: Mese and Vaidyanathan 16x16...\n");
        out_image.fill(0);
        dot_diffusion_dither(dither_image, DD_MESE_16X16_CLASS, out_image);
        save_and_free_image(basename, "dd_mese16x16.bmp", out_image);\
    
        printf("running Dot Diffusion: Guo Liu 8x8...\n");
        out_image.fill(0);
        dot_diffusion_dither(dither_image, DD_GUOLIU_8X8_CLASS, out_image);
        save_and_free_image(basename, "dd_guoliu8x8.bmp", out_image);

        printf("running Dot Diffusion: Guo Liu 16x16...\n");
        out_image.fill(0);
        dot_diffusion_dither(dither_image, DD_GUOLIU_16X16_CLASS, out_image);
        save_and_free_image(basename, "dd_guoliu16x16.bmp", out_image);

        printf("running Dot Diffusion: Spiral...\n");
        out_image.fill(0);
        dot_diffusion_dither(dither_image, DD_SPIRAL_CLASS, out_image);
        save_and_free_image(basename, "dd_spiral.bmp", out_image);

        printf("running Dot Diffusion: Inverted Spiral...\n");
        out_image.fill(0);
        dot_diffusion_dither(dither_image, DD_SPIRAL_INVERTED_CLASS, out_image);
        save_and_free_image(basename, "dd_inv_spiral.bmp", out_image);

        /* Error Diffusion dithering */
        printf("running Error Diffusion: Xot...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_XOT, true, 0.0, out_image);
        save_and_free_image(basename, "ed_xot.bmp", out_image);

        printf("running Error Diffusion: Diagonal Diffusion...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_DIAGONAL, false, 0.0, out_image);
        save_and_free_image(basename, "ed_diagonal.bmp", out_image);

        printf("running Error Diffusion: Floyd Steinberg...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_FLOYD_STEINBERG, false, 0.0, out_image);
        save_and_free_image(basename, "ed_floyd-steinberg.bmp", out_image);

        printf("running Error Diffusion: Shiau Fan 3...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_SHIAUFAN3, false, 0.0, out_image);
        save_and_free_image(basename, "ed_shiaufan3.bmp", out_image);

        printf("running Error Diffusion: Shiau Fan 2...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_SHIAUFAN2, true, 0.0, out_image);
        save_and_free_image(basename, "ed_shiaufan2.bmp", out_image);

        printf("running Error Diffusion: Shiau Fan 1...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_SHIAUFAN1, false, 0.0, out_image);
        save_and_free_image(basename, "ed_shiaufan1.bmp", out_image);

        printf("running Error Diffusion: Stucki...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_STUCKI, false, 0.0, out_image);
        save_and_free_image(basename, "ed_stucki.bmp", out_image);

        printf("running Error Diffusion: 1 Dimensional...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_DIFFUSION_1D, true, 0.0, out_image);
        save_and_free_image(basename, "ed_1d.bmp", out_image);

        printf("running Error Diffusion: 2 Dimensional...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_DIFFUSION_2D, true, 0.0, out_image);
        save_and_free_image(basename, "ed_2d.bmp", out_image);

        printf("running Error Diffusion: Fake Floyd Steinberg...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_FAKE_FLOYD_STEINBERG, false, 0.0, out_image);
        save_and_free_image(basename, "ed_fake_floyd_steinberg.bmp", out_image);

        printf("running Error Diffusion: Jarvis-Judice-Ninke...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_JARVIS_JUDICE_NINKE, false, 0.0, out_image);
        save_and_free_image(basename, "ed_jjn.bmp", out_image);

        printf("running Error Diffusion: Atkinson...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_ATKINSON, false, 0.0, out_image);
        save_and_free_image(basename, "ed_atkinson.bmp", out_image);

        printf("running Error Diffusion: Burkes...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_BURKES, false, 0.0, out_image);
        save_and_free_image(basename, "ed_burkes.bmp", out_image);

        printf("running Error Diffusion: Sierra 3...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_SIERRA_3, false, 0.0, out_image);
        save_and_free_image(basename, "ed_sierra3.bmp", out_image);

        printf("running Error Diffusion: Sierra 2-Row...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_SIERRA_2ROW, false, 0.0, out_image);
        save_and_free_image(basename, "ed_sierra2row.bmp", out_image);

        printf("running Error Diffusion: Sierra Lite...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_SIERRA_LITE, true, 0.0, out_image);
        save_and_free_image(basename, "ed_sierra_lite.bmp", out_image);

        printf("running Error Diffusion: Steve Pigeon...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_STEVE_PIGEON, false, 0.0, out_image);
        save_and_free_image(basename, "ed_steve_pigeon.bmp", out_image);

        printf("running Error Diffusion: Robert Kist...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_ROBERT_KIST, false, 0.0, out_image);
        save_and_free_image(basename, "ed_robert_kist.bmp", out_image);

        printf("running Error Diffusion: Stevenson-Arce...\n");
        out_image.fill(0);
        error_diffusion_dither(dither_image, ED_STEVENSON_ARCE, true, 0.0, out_image);
        save_and_free_image(basename, "ed_stevenson_arce.bmp", out_image);

        /* Ordered Dithering */
        printf("running Ordered Dithering: Blue Noise...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BLUE_NOISE_128X128, 0.0, out_image);
        save_and_free_image(basename, "od_blue_noise.bmp", out_image);

        printf("running Ordered Dithering: Bayer 2x2...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER2X2, 0.0, out_image);
        save_and_free_image(basename, "od_bayer2x2.bmp", out_image);

        printf("running Ordered Dithering: Bayer 3x3...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER3X3, 0.0, out_image);
        save_and_free_image(basename, "od_bayer3x3.bmp", out_image);

        printf("running Ordered Dithering: Bayer 4x4...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER4X4, 0.0, out_image);
        save_and_free_image(basename, "od_bayer4x4.bmp", out_image);

        printf("running Ordered Dithering: Bayer 8x8...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER8X8, 0.0, out_image);
        save_and_free_image(basename, "od_bayer8x8.bmp", out_image);

        printf("running Ordered Dithering: Bayer 16x16...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER16X16, 0.0, out_image);
        save_and_free_image(basename, "od_bayer16x16.bmp", out_image);

        printf("running Ordered Dithering: Bayer 32x32...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER32X32, 0.0, out_image);
        save_and_free_image(basename, "od_bayer32x32.bmp", out_image);

        printf("running Ordered Dithering: Dispersed Dots 1...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_DISPERSED_DOTS_1, 0.0, out_image);
        save_and_free_image(basename, "od_disp_dots1.bmp", out_image);

        printf("running Ordered Dithering: Dispersed Dots 2...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_DISPERSED_DOTS_2, 0.0, out_image);
        save_and_free_image(basename, "od_disp_dots2.bmp", out_image);

        printf("running Ordered Dithering: Ulichney Void Dispersed Dots...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_ULICHNEY_VOID_DISPERSED_DOTS, 0.0, out_image);
        save_and_free_image(basename, "od_ulichney_vdd.bmp", out_image);

        printf("running Ordered Dithering: Non-Rectangular 1...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_NON_RECTANGULAR_1, 0.0, out_image);
        save_and_free_image(basename, "od_non_rect1.bmp", out_image);

        printf("running Ordered Dithering: Non-Rectangular 2...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_NON_RECTANGULAR_2, 0.0, out_image);
        save_and_free_image(basename, "od_non_rect2.bmp", out_image);

        printf("running Ordered Dithering: Non-Rectangular 3...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_NON_RECTANGULAR_3, 0.0, out_image);
        save_and_free_image(basename, "od_non_rect3.bmp", out_image);

        printf("running Ordered Dithering: Non-Rectangular 4...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_NON_RECTANGULAR_4, 0.0, out_image);
        save_and_free_image(basename, "od_non_rect4.bmp", out_image);

        printf("running Ordered Dithering: Ulichney Bayer 5x5...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_ULICHNEY_BAYER_5, 0.0, out_image);
        save_and_free_image(basename, "od_ulichney_bayer5.bmp", out_image);

        printf("running Ordered Dithering: Ulichney...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_ULICHNEY, 0.0, out_image);
        save_and_free_image(basename, "od_ulichney.bmp", out_image);

        printf("running Ordered Dithering: Clustered Dot 1...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER_CLUSTERED_DOT_1, 0.0, out_image);
        save_and_free_image(basename, "od_clustered_dot1.bmp", out_image);

        printf("running Ordered Dithering: Clustered Dot 2...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER_CLUSTERED_DOT_2, 0.0, out_image);
        save_and_free_image(basename, "od_clustered_dot2.bmp", out_image);

        printf("running Ordered Dithering: Clustered Dot 3...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER_CLUSTERED_DOT_3, 0.0, out_image);
        save_and_free_image(basename, "od_clustered_dot3.bmp", out_image);

        printf("running Ordered Dithering: Clustered Dot 4...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER_CLUSTERED_DOT_4, 0.0, out_image);
        save_and_free_image(basename, "od_clustered_dot4.bmp", out_image);

        printf("running Ordered Dithering: Clustered Dot 5...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER_CLUSTERED_DOT_5, 0.0, out_image);
        save_and_free_image(basename, "od_clustered_dot5.bmp", out_image);

        printf("running Ordered Dithering: Clustered Dot 6...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER_CLUSTERED_DOT_6, 0.0, out_image);
        save_and_free_image(basename, "od_clustered_dot6.bmp", out_image);

        printf("running Ordered Dithering: Clustered Dot 7...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER_CLUSTERED_DOT_7, 0.0, out_image);
        save_and_free_image(basename, "od_clustered_dot7.bmp", out_image);

        printf("running Ordered Dithering: Clustered Dot 8...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER_CLUSTERED_DOT_8, 0.0, out_image);
        save_and_free_image(basename, "od_clustered_dot8.bmp", out_image);

        printf("running Ordered Dithering: Clustered Dot 9...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER_CLUSTERED_DOT_9, 0.0, out_image);
        save_and_free_image(basename, "od_clustered_dot9.bmp", out_image);

        printf("running Ordered Dithering: Clustered Dot 10...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER_CLUSTERED_DOT_10, 0.0, out_image);
        save_and_free_image(basename, "od_clustered_dot10.bmp", out_image);

        printf("running Ordered Dithering: Clustered Dot 11...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BAYER_CLUSTERED_DOT_11, 0.0, out_image);
        save_and_free_image(basename, "od_clustered_dot11.bmp", out_image);

        printf("running Ordered Dithering: Central White Point...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_CENTRAL_WHITE_POINT, 0.0, out_image);
        save_and_free_image(basename, "od_ctrl_wp.bmp", out_image);

        printf("running Ordered Dithering: Balanced Central White Point...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_BALANCED_CENTERED_POINT, 0.0, out_image);
        save_and_free_image(basename, "od_balanced_ctrl_wp.bmp", out_image);

        printf("running Ordered Dithering: Diagonal Ordered...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_DIAGONAL_ORDERED_MATRIX, 0.0, out_image);
        save_and_free_image(basename, "od_diag_ordered.bmp", out_image);

        printf("running Ordered Dithering: Ulichney Clustered Dot...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_ULICHNEY_CLUSTERED_DOT, 0.0, out_image);
        save_and_free_image(basename, "od_ulichney_clust_dot.bmp", out_image);

        printf("running Ordered Dithering: ImageMagick 5x5 Circle...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_MAGIC5X5_CIRCLE, 0.0, out_image);
        save_and_free_image(basename, "od_magic5x5_circle.bmp", out_image);

        printf("running Ordered Dithering: ImageMagick 6x6 Circle...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_MAGIC6X6_CIRCLE, 0.0, out_image);
        save_and_free_image(basename, "od_magic6x6_circle.bmp", out_image);

        printf("running Ordered Dithering: ImageMagick 7x7 Circle...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_MAGIC7X7_CIRCLE, 0.0, out_image);
        save_and_free_image(basename, "od_magic7x7_circle.bmp", out_image);

        printf("running Ordered Dithering: ImageMagick 4x4 45-degrees...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_MAGIC4X4_45, 0.0, out_image);
        save_and_free_image(basename, "od_magic4x4_45.bmp", out_image);

        printf("running Ordered Dithering: ImageMagick 6x6 45-degrees...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_MAGIC6X6_45, 0.0, out_image);
        save_and_free_image(basename, "od_magic6x6_45.bmp", out_image);

        printf("running Ordered Dithering: ImageMagick 8x8 45-degrees...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_MAGIC8X8_45, 0.0, out_image);
        save_and_free_image(basename, "od_magic8x8_45.bmp", out_image);

        printf("running Ordered Dithering: ImageMagick 4x4...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_MAGIC4X4, 0.0, out_image);
        save_and_free_image(basename, "od_magic4x4.bmp", out_image);

        printf("running Ordered Dithering: ImageMagick 6x6...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_MAGIC6X6, 0.0, out_image);
        save_and_free_image(basename, "od_magic6x6.bmp", out_image);

        printf("running Ordered Dithering: ImageMagick 8x8...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_MAGIC8X8, 0.0, out_image);
        save_and_free_image(basename, "od_magic8x8.bmp", out_image);

        printf("running Ordered Dithering: Variable 2x2 Matix...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_VARIABLE_2X2, 0.0, out_image, ImGui::ImMat(), 55);
        save_and_free_image(basename, "od_variable2x2.bmp", out_image);

        printf("running Ordered Dithering: Variable 4x4 Matix...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_VARIABLE_4X4, 0.0, out_image, ImGui::ImMat(), 14);
        save_and_free_image(basename, "od_variable4x4.bmp", out_image);

        printf("running Ordered Dithering: Interleaved Gradient Noise...\n");
        out_image.fill(0);
        ordered_dither(dither_image, OD_INTERLEAVED_GRADIENT_NOISE, 0.0, out_image, ImGui::ImMat(), 4, ImVec4(52.9829189, 0.06711056, 0.00583715, 0));
        save_and_free_image(basename, "od_interleaved_gradient.bmp", out_image);

        printf("running Ordered Dithering: Blue Noise image based...\n");
        auto matrix_image = bmp_to_ditherimage(noise_filename, false);
        out_image.fill(0);
        ordered_dither(dither_image, OD_MATRIX_FROM_IMAGE, 0.0, out_image, matrix_image);
        save_and_free_image(basename, "od_blue_noise_image.bmp", out_image);

        /* Variable Error Diffusion Dithering */
        printf("running Variable Error Diffusion: Ostromoukhov...\n");
        out_image.fill(0);
        variable_error_diffusion_dither(dither_image, VD_OSTROMOUKHOV, true, out_image);
        save_and_free_image(basename, "ved_ostromoukhov.bmp", out_image);

        printf("running Variable Error Diffusion: Zhou Fang...\n");
        out_image.fill(0);
        variable_error_diffusion_dither(dither_image, VD_ZHOUFANG, true, out_image);
        save_and_free_image(basename, "ved_zhoufang.bmp", out_image);

        /* Thresholding */
        printf("running Thresholding...\n");
        out_image.fill(0);
        double threshold = auto_threshold(dither_image);  // determine optimal threshold value
        threshold_dither(dither_image, threshold, 0.55, out_image);
        save_and_free_image(basename, "threshold.bmp", out_image);

        /* Direct Binary Search (DBS) Dithering */
        for (int i = 0; i < 7; i++) {
            char filename[2048];
            printf("running Direct Binary Search (DBS): formula %i...\n", i);
            out_image.fill(0);
            dbs_dither(dither_image, i, out_image);
            snprintf(filename, 2048, "dbs%i.bmp", i);
            save_and_free_image(basename, filename, out_image);
        }

        /* Kacker and Allebach dithering */
        printf("running Kacker and Allebach dithering...\n");
        out_image.fill(0);
        kallebach_dither(dither_image, true, out_image);
        save_and_free_image(basename, "kallebach.bmp", out_image);

        /* Riemersma Dithering */

        for (int i = 0; i <= 1; i++) {
            char filename[2048];
            char* namepart = i == 0 ? (char*)"rim_mod" : (char*)"rim";
            char* descpart = i == 0 ? (char*)"Modified " : (char*)"";

            printf("running %sRiemersma dithering: Hilbert curve...\n", descpart);
            out_image.fill(0);
            riemersma_dither(dither_image, RD_HILBERT_CURVE, (bool) i, out_image);
            snprintf(filename, 2048, "%s_hilbert.bmp", namepart);
            save_and_free_image(basename, filename, out_image);

            printf("running %sRiemersma dithering: modified Hilbert curve...\n", descpart);
            out_image.fill(0);
            riemersma_dither(dither_image, RD_HILBERTMOD_CURVE, (bool) i, out_image);
            snprintf(filename, 2048, "%s_hilbert_mod.bmp", namepart);
            save_and_free_image(basename, filename, out_image);

            printf("running %sRiemersma dithering: Peano curve...\n", descpart);
            out_image.fill(0);
            riemersma_dither(dither_image, RD_PEANO_CURVE, (bool) i, out_image);
            snprintf(filename, 2048, "%s_peano.bmp", namepart);
            save_and_free_image(basename, filename, out_image);

            printf("running %sRiemersma dithering: Fass-0 curve...\n", descpart);
            out_image.fill(0);
            riemersma_dither(dither_image, RD_FASS0_CURVE, (bool) i, out_image);
            snprintf(filename, 2048, "%s_fass0.bmp", namepart);
            save_and_free_image(basename, filename, out_image);

            printf("running %sRiemersma dithering: Fass-1 curve...\n", descpart);
            out_image.fill(0);
            riemersma_dither(dither_image, RD_FASS1_CURVE, (bool) i, out_image);
            snprintf(filename, 2048, "%s_fass1.bmp", namepart);
            save_and_free_image(basename, filename, out_image);

            printf("running %sRiemersma dithering: Fass-2 curve...\n", descpart);
            out_image.fill(0);
            riemersma_dither(dither_image, RD_FASS2_CURVE, (bool) i, out_image);
            snprintf(filename, 2048, "%s_fass2.bmp", namepart);
            save_and_free_image(basename, filename, out_image);

            printf("running %sRiemersma dithering: Gosper curve...\n", descpart);
            out_image.fill(0);
            riemersma_dither(dither_image, RD_GOSPER_CURVE, (bool) i, out_image);
            snprintf(filename, 2048, "%s_gosper.bmp", namepart);
            save_and_free_image(basename, filename, out_image);

            printf("running %sRiemersma dithering: Fass Spiral...\n", descpart);
            out_image.fill(0);
            riemersma_dither(dither_image, RD_FASS_SPIRAL, (bool) i, out_image);
            snprintf(filename, 2048, "%s_fass_spiral.bmp", namepart);
            save_and_free_image(basename, filename, out_image);
        }

        /* Pattern dithering */
        printf("running Pattern dithering: 2x2 pattern...\n");
        out_image.fill(0);
        pattern_dither(dither_image, PD_2X2_PATTERN, out_image);
        save_and_free_image(basename, "pattern2x2.bmp", out_image);

        printf("running Pattern dithering: 3x3 pattern v1...\n");
        out_image.fill(0);
        pattern_dither(dither_image, PD_3X3_PATTERN_V1, out_image);
        save_and_free_image(basename, "pattern3x3_v1.bmp", out_image);

        printf("running Pattern dithering: 3x3 pattern v2...\n");
        out_image.fill(0);
        pattern_dither(dither_image, PD_3X3_PATTERN_V2, out_image);
        save_and_free_image(basename, "pattern3x3_v2.bmp", out_image);

        printf("running Pattern dithering: 3x3 pattern v3...\n");
        out_image.fill(0);
        pattern_dither(dither_image, PD_3X3_PATTERN_V3, out_image);
        save_and_free_image(basename, "pattern3x3_v3.bmp", out_image);

        printf("running Pattern dithering: 4x4 pattern...\n");
        out_image.fill(0);
        pattern_dither(dither_image, PD_4X4_PATTERN, out_image);
        save_and_free_image(basename, "pattern4x4.bmp", out_image);

        printf("running Pattern dithering: 5x2 pattern...\n");
        out_image.fill(0);
        pattern_dither(dither_image, PD_5X2_PATTERN, out_image);
        save_and_free_image(basename, "pattern5x2.bmp", out_image);

        /* Dot Lippens */
        printf("running Lippens and Philips: v1...\n");
        out_image.fill(0);
        dotlippens_dither(dither_image, LP_V1, out_image);
        save_and_free_image(basename, "dlippens1.bmp", out_image);

        printf("running Lippens and Philips: v2...\n");
        out_image.fill(0);
        dotlippens_dither(dither_image, LP_V2, out_image);
        save_and_free_image(basename, "dlippens2.bmp", out_image);

        printf("running Lippens and Philips: v3...\n");
        out_image.fill(0);
        dotlippens_dither(dither_image, LP_V3, out_image);
        save_and_free_image(basename, "dlippens3.bmp", out_image);

        printf("running Lippens and Philips: Guo Liu 16x16...\n");
        out_image.fill(0);
        dotlippens_dither(dither_image, LP_GUO_LIU_16X16, out_image);
        save_and_free_image(basename, "dlippens_guoliu16.bmp", out_image);

        printf("running Lippens and Philips: Mese and Vaidyanathan 16x16...\n");
        out_image.fill(0);
        dotlippens_dither(dither_image, LP_MESE_AND_VAIDYANATHAN_16X16, out_image);
        save_and_free_image(basename, "dlippens_mese16.bmp", out_image);

        printf("running Lippens and Philips: Knuth...\n");
        out_image.fill(0);
        dotlippens_dither(dither_image, LP_KNUTH, out_image);
        save_and_free_image(basename, "dlippens_knuth.bmp", out_image);
    }
    return 0;
}
