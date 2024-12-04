#pragma once
#include "imgui.h"
#include "immat.h"

/* returns the version number of this library */
IMGUI_API const char* libdither_version();
/* returns internal image data */
IMGUI_API ImGui::ImMat dither_test_image();
/* ********************************************* */
/* **** BOSCH HERMAN INSPIRED GRID DITHERER **** */
/* ********************************************* */

/* Uses grid dither algorithm to dither an image.
 * w: grid width
 * h: grid height
 * min_pixels: minimum amount of pixels to set to black in each grid. Must be between 0 and width * height.
 *             for best results it is recommended to have this number at most at (width * height / 2)
 * algorithm: when true uses a modified algorithm that yields contrast that is more true to the input image */
IMGUI_API void grid_dither(const ImGui::ImMat& img, int w, int h, int min_pixels, bool alt_algorithm, ImGui::ImMat& out);

/* ********************************** */
/* **** ERROR DIFFUSION DITHERER **** */
/* ********************************** */
typedef enum error_diffusion_type : int
{
    ED_XOT = 0,
    ED_DIAGONAL,
    ED_FLOYD_STEINBERG,
    ED_SHIAUFAN3,
    ED_SHIAUFAN2,
    ED_SHIAUFAN1,
    ED_STUCKI,
    ED_DIFFUSION_1D,
    ED_DIFFUSION_2D,
    ED_FAKE_FLOYD_STEINBERG,
    ED_JARVIS_JUDICE_NINKE,
    ED_ATKINSON,
    ED_BURKES,
    ED_SIERRA_3,
    ED_SIERRA_2ROW,
    ED_SIERRA_LITE,
    ED_STEVE_PIGEON,
    ED_ROBERT_KIST,
    ED_STEVENSON_ARCE
} ED_TYPE;
/* Uses the error diffusion dither algorithm to dither an image.
 * type: an error diffusion matrix structure type. This library contains many built-in matrices
 * serpentine: if the image should be traversed from top to bottom in a serpentine (left-to-right, right-to-left, etc.) manner
 * sigma: introduces jitter to the dither output to make it appear less regular. Recommended range: 0.0 - 1.0 */

IMGUI_API void error_diffusion_dither(const ImGui::ImMat& img, const ED_TYPE type, bool serpentine, float sigma, ImGui::ImMat& out);

/* ************************** */
/* **** ORDERED DITHERER **** */
/* ************************** */
typedef enum ordered_type : int
{
    OD_BLUE_NOISE_128X128 = 0,
    OD_BAYER2X2,
    OD_BAYER3X3,
    OD_BAYER4X4,
    OD_BAYER8X8,
    OD_BAYER16X16,
    OD_BAYER32X32,
    OD_DISPERSED_DOTS_1,
    OD_DISPERSED_DOTS_2,
    OD_ULICHNEY_VOID_DISPERSED_DOTS,
    OD_NON_RECTANGULAR_1,
    OD_NON_RECTANGULAR_2,
    OD_NON_RECTANGULAR_3,
    OD_NON_RECTANGULAR_4,
    OD_ULICHNEY_BAYER_5,
    OD_ULICHNEY,
    OD_BAYER_CLUSTERED_DOT_1,
    OD_BAYER_CLUSTERED_DOT_2,
    OD_BAYER_CLUSTERED_DOT_3,
    OD_BAYER_CLUSTERED_DOT_4,
    OD_BAYER_CLUSTERED_DOT_5,
    OD_BAYER_CLUSTERED_DOT_6,
    OD_BAYER_CLUSTERED_DOT_7,
    OD_BAYER_CLUSTERED_DOT_8,
    OD_BAYER_CLUSTERED_DOT_9,
    OD_BAYER_CLUSTERED_DOT_10,
    OD_BAYER_CLUSTERED_DOT_11,
    OD_CENTRAL_WHITE_POINT,
    OD_BALANCED_CENTERED_POINT,
    OD_DIAGONAL_ORDERED_MATRIX,
    OD_ULICHNEY_CLUSTERED_DOT,
    OD_MAGIC5X5_CIRCLE,
    OD_MAGIC6X6_CIRCLE,
    OD_MAGIC7X7_CIRCLE,
    OD_MAGIC4X4_45,
    OD_MAGIC6X6_45,
    OD_MAGIC8X8_45,
    OD_MAGIC4X4,
    OD_MAGIC6X6,
    OD_MAGIC8X8,
    OD_VARIABLE_2X2,
    OD_VARIABLE_4X4,
    OD_INTERLEAVED_GRADIENT_NOISE,
    OD_MATRIX_FROM_IMAGE
} OD_TYPE;
/* Uses the ordered dither algorithm to dither an image.
 * type: an OrderedDitherMatrix which determines how the image will be dithered
 * sigma: introduces jitter to the dither output to make it appear less regular. Recommended range 0.0 - 0.2 */
IMGUI_API void ordered_dither(const ImGui::ImMat& img, const OD_TYPE type, float sigma, ImGui::ImMat& out, const ImGui::ImMat& noise = {}, int step = 0, ImVec4 param = {});

/* ******************************** */
/* **** DOT DIFFUSION DITHERER **** */
/* ******************************** */
typedef enum dot_diffusion_type : int
{
    DD_KNUTH_CLASS = 0,
    DD_MINI_KNUTH_CLASS,
    DD_OPTIMIZED_KNUTH_CLASS,
    DD_MESE_8X8_CLASS,
    DD_MESE_16X16_CLASS,
    DD_GUOLIU_8X8_CLASS,
    DD_GUOLIU_16X16_CLASS,
    DD_SPIRAL_CLASS,
    DD_SPIRAL_INVERTED_CLASS
} DD_TYPE;
/* Uses grid dither algorithm to dither an image - allows for different combination of class and diffusion matrices */
IMGUI_API void dot_diffusion_dither(const ImGui::ImMat& img, const DD_TYPE type, ImGui::ImMat& out);

/* ******************************************* */
/* **** VARIABLE ERROR DIFFUSION DITHERER **** */
/* ******************************************* */
/* enum for specifying the specific algorithm for 'variable_error_diffusion_dither' */
typedef enum VarDitherType : int
{
    VD_OSTROMOUKHOV = 0,
    VD_ZHOUFANG
} VD_TYPE;
/* Uses variable error diffusion dither algorithm to dither an image.
 * type: Ostromoukhov or Zhoufang
 * serpentine: if the image should be traversed from top to bottom in a serpentine (left-to-right, right-to-left, etc.) manner */
IMGUI_API void variable_error_diffusion_dither(const ImGui::ImMat& img, const VD_TYPE type, bool serpentine, ImGui::ImMat& out);

/* **************************** */
/* **** THRESHOLD DITHERER **** */
/* **************************** */
/* automatically determines best threshold for the given image. Use as input for threshold in 'threshold_dither' */
IMGUI_API double auto_threshold(const ImGui::ImMat& img);
/* Uses thresholding algorithm to dither an image.
 * threshold: threshold for dithering a pixel as black. from 0.0 to 1.0.
 * noise: amount of noise. from 0.0 to 1.0. Recommended 0.55 */
IMGUI_API void threshold_dither(const ImGui::ImMat&  img, double threshold, double noise, ImGui::ImMat&  out);

/* ********************** */
/* **** DBS DITHERER **** */
/* ********************** */
/* Uses the direct binary search (DBS) dither algorithm to dither an image. */
// v: value from 0-7. The higher the value, the coarser the output dither will be.
IMGUI_API void dbs_dither(const ImGui::ImMat& img, int v, ImGui::ImMat& out);

/* *************************************** */
/* **** KACKER AND ALLEBACH DITHERING **** */
/* *************************************** */
/* Uses the Kacker and Allebach dither algorithm to dither an image.
 * random: when false, dither output will always be the same for the same image; otherwise there will be randomness */
IMGUI_API void kallebach_dither(const ImGui::ImMat& img, bool random, ImGui::ImMat& out);

/* **************************** */
/* **** RIEMERSMA DITHERER **** */
/* **************************** */
typedef enum riemersma_type : int
{
    RD_HILBERT_CURVE = 0,
    RD_HILBERTMOD_CURVE,
    RD_PEANO_CURVE,
    RD_FASS0_CURVE,
    RD_FASS1_CURVE,
    RD_FASS2_CURVE,
    RD_GOSPER_CURVE,
    RD_FASS_SPIRAL
} RD_TYPE;
/* Uses the Riemersma dither algorithm to dither an image.
 * use_riemersma: when false, uses a slightly improved algorithm for better visual results. */
IMGUI_API void riemersma_dither(const ImGui::ImMat& img, const RD_TYPE type, bool use_riemersma, ImGui::ImMat& out);

/* ************************** */
/* **** PATTERN DITHERER **** */
/* ************************** */
typedef enum pattern_type : int
{
    PD_2X2_PATTERN = 0,
    PD_3X3_PATTERN_V1,
    PD_3X3_PATTERN_V2,
    PD_3X3_PATTERN_V3,
    PD_4X4_PATTERN,
    PD_5X2_PATTERN
} PD_TYPE;
/* Uses the pattern dither algorithm to dither an image. */
IMGUI_API void pattern_dither(const ImGui::ImMat& img, const PD_TYPE type, ImGui::ImMat& out);

/* ****************************************** */
/* **** LIPPENS AND PHILIPS DOT DITHERER **** */
/* ****************************************** */
typedef enum lippens_philips_type : int
{
    LP_V1,
    LP_V2,
    LP_V3,
    LP_GUO_LIU_16X16,
    LP_MESE_AND_VAIDYANATHAN_16X16,
    LP_KNUTH
} LP_TYPE;
/* Uses Lippens and Philip's dot dither algorithm to dither an image. */
IMGUI_API void dotlippens_dither(const ImGui::ImMat& img, const LP_TYPE type, ImGui::ImMat& out);
