#define MODULE_API_EXPORTS
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "libdither.h"
#include "random.h"
#include "dither_ordered_data.h"

struct OrderedDitherMatrix {
    double divisor;
    int* buffer;  // buffer for flat matrix array
    int  width;
    int  height;
};

static OrderedDitherMatrix* OrderedDitherMatrix_new(int width, int height, double divisor, const int* matrix) {
    OrderedDitherMatrix* self = (OrderedDitherMatrix*)calloc(1, sizeof(OrderedDitherMatrix));
    self->buffer = (int*)calloc(width * height, sizeof(int));
    memcpy(self->buffer, matrix, width * height * sizeof(int));
    self->divisor = divisor;
    self->width = width;
    self->height = height;
    return self;
}

static void OrderedDitherMatrix_free(OrderedDitherMatrix* self) {
    if(self) {
        free(self->buffer);
        free(self);
        self = NULL;
    }
}

static OrderedDitherMatrix* get_bayer2x2_matrix() { return OrderedDitherMatrix_new(2, 2, 4.0, bayer2x2_matrix); }
static OrderedDitherMatrix* get_bayer3x3_matrix() { return OrderedDitherMatrix_new(3, 3, 9.0, bayer3x3_matrix); }
static OrderedDitherMatrix* get_bayer4x4_matrix() { return OrderedDitherMatrix_new(4, 4, 16.0, bayer4x4_matrix); }
static OrderedDitherMatrix* get_bayer8x8_matrix() { return OrderedDitherMatrix_new(8, 8, 64.0, bayer8x8_matrix); }
static OrderedDitherMatrix* get_bayer16x16_matrix() { return OrderedDitherMatrix_new(16, 16, 256.0, bayer16x16_matrix); }
static OrderedDitherMatrix* get_bayer32x32_matrix() { return OrderedDitherMatrix_new(32, 32, 1024.0, bayer32x32_matrix); }
static OrderedDitherMatrix* get_dispersed_dots_1_matrix() { return OrderedDitherMatrix_new(6, 6, 36.0, dispersed_dots_1_matrix); }
static OrderedDitherMatrix* get_dispersed_dots_2_matrix() { return OrderedDitherMatrix_new(8, 8, 64.0, dispersed_dots_2_matrix); }
static OrderedDitherMatrix* get_ulichney_void_dispersed_dots_matrix() { return OrderedDitherMatrix_new(14, 14, 196.0, ulichney_void_dispersed_dots_matrix); }
static OrderedDitherMatrix* get_non_rectangular_1_matrix() { return OrderedDitherMatrix_new(5, 5, 4.8, non_rectangular_1_matrix); }
static OrderedDitherMatrix* get_non_rectangular_2_matrix() { return OrderedDitherMatrix_new(8, 8, 8.0, non_rectangular_2_matrix); }
static OrderedDitherMatrix* get_non_rectangular_3_matrix() { return OrderedDitherMatrix_new(10, 10, 10.0, non_rectangular_3_matrix); }
static OrderedDitherMatrix* get_non_rectangular_4_matrix() { return OrderedDitherMatrix_new(10, 5, 10.0, non_rectangular_4_matrix); }
static OrderedDitherMatrix* get_ulichney_bayer_5_matrix() { return OrderedDitherMatrix_new(8, 8, 1000.0, ulichney_bayer_5_matrix); }
static OrderedDitherMatrix* get_ulichney_matrix() { return OrderedDitherMatrix_new(4, 4, 16.0, ulichney_matrix); }
static OrderedDitherMatrix* get_bayer_clustered_dot_1_matrix() { return OrderedDitherMatrix_new(8, 8, 64.0, bayer_clustered_dot_1_matrix); }
static OrderedDitherMatrix* get_bayer_clustered_dot_2_matrix() { return OrderedDitherMatrix_new(5, 3, 15.0, bayer_clustered_dot_2_matrix); }
static OrderedDitherMatrix* get_bayer_clustered_dot_3_matrix() { return OrderedDitherMatrix_new(3, 5, 15.0, bayer_clustered_dot_3_matrix); }
static OrderedDitherMatrix* get_bayer_clustered_dot_4_matrix() { return OrderedDitherMatrix_new(6, 6, 18.0, bayer_clustered_dot_4_matrix); }
static OrderedDitherMatrix* get_bayer_clustered_dot_5_matrix() { return OrderedDitherMatrix_new(8, 8, 32.0, bayer_clustered_dot_5_matrix); }
static OrderedDitherMatrix* get_bayer_clustered_dot_6_matrix() { return OrderedDitherMatrix_new(16, 16, 128.0, bayer_clustered_dot_6_matrix); }
static OrderedDitherMatrix* get_bayer_clustered_dot_7_matrix() { return OrderedDitherMatrix_new(6, 6, 36.0, bayer_clustered_dot_7_matrix); }
static OrderedDitherMatrix* get_bayer_clustered_dot_8_matrix() { return OrderedDitherMatrix_new(5, 5, 25.0, bayer_clustered_dot_8_matrix); }
static OrderedDitherMatrix* get_bayer_clustered_dot_9_matrix() { return OrderedDitherMatrix_new(6, 6, 36.0, bayer_clustered_dot_9_matrix); }
static OrderedDitherMatrix* get_bayer_clustered_dot_10_matrix() { return OrderedDitherMatrix_new(6, 6, 36.0, bayer_clustered_dot_10_matrix); }
static OrderedDitherMatrix* get_bayer_clustered_dot_11_matrix() { return OrderedDitherMatrix_new(8, 8, 64.0, bayer_clustered_dot_11_matrix); }
static OrderedDitherMatrix* get_central_white_point_matrix() { return OrderedDitherMatrix_new(6, 6, 36.0, central_white_point_matrix); }
static OrderedDitherMatrix* get_balanced_centered_point_matrix() { return OrderedDitherMatrix_new(6, 6, 36.0, balanced_centered_point_matrix); }
static OrderedDitherMatrix* get_diagonal_ordered_matrix_matrix() { return OrderedDitherMatrix_new(8, 8, 32.0, diagonal_ordered_matrix_matrix); }
static OrderedDitherMatrix* get_ulichney_clustered_dot_matrix() { return OrderedDitherMatrix_new(8, 8, 1000.0, ulichney_clustered_dot_matrix); }
static OrderedDitherMatrix* get_magic5x5_circle_matrix() { return OrderedDitherMatrix_new(5, 5, 26.0, magic5x5_circle_matrix); }
static OrderedDitherMatrix* get_magic6x6_circle_matrix() { return OrderedDitherMatrix_new(6, 6, 37.0, magic6x6_circle_matrix); }
static OrderedDitherMatrix* get_magic7x7_circle_matrix() { return OrderedDitherMatrix_new(7, 7, 50.0, magic7x7_circle_matrix); }
static OrderedDitherMatrix* get_magic4x4_45_matrix() { return OrderedDitherMatrix_new(4, 4, 9.0, magic4x4_45_matrix); }
static OrderedDitherMatrix* get_magic6x6_45_matrix() { return OrderedDitherMatrix_new(6, 6, 19.0, magic6x6_45_matrix); }
static OrderedDitherMatrix* get_magic8x8_45_matrix() { return OrderedDitherMatrix_new(8, 8, 33.0, magic8x8_45_matrix); }
static OrderedDitherMatrix* get_magic4x4_matrix() { return OrderedDitherMatrix_new(4, 4, 17.0, magic4x4_matrix); }
static OrderedDitherMatrix* get_magic6x6_matrix() { return OrderedDitherMatrix_new(6, 6, 37.0, magic6x6_matrix); }
static OrderedDitherMatrix* get_magic8x8_matrix() { return OrderedDitherMatrix_new(8, 8, 65.0, magic8x8_matrix); }

static OrderedDitherMatrix* get_matrix_from_image(const ImGui::ImMat& img) {
    // convert an image into a dither matrix. E.g. for using noise textures
    int* matrix = (int*)calloc(img.w * img.h, sizeof(int));
    for(int y = 0; y < img.h; y++) {
        for (int x = 0; x < img.w; x++) {
            size_t addr = y * img.w + x;
            matrix[addr] = (int)round(img.at<uint8_t>(x, y) / 255.0 * INT_MAX);
        }
    }
    OrderedDitherMatrix* m = OrderedDitherMatrix_new(img.w, img.h, INT_MAX, matrix);
    free(matrix);
    return m;
}

static OrderedDitherMatrix* get_interleaved_gradient_noise(int size, double a, double b, double c) {
    int* matrix = (int*)calloc(size * size, sizeof(int));
    bool is_zero = true;
    for(int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            double vd = a * (b * x + c * y);
            int vi = (int)((vd - floor(vd)) * INT_MAX);
            matrix[y * size + x] = vi;
            if(vi > 0)
                is_zero = false;
        }
    }
    if(is_zero) {
        free(matrix);
        return NULL;
    }
    OrderedDitherMatrix* m = OrderedDitherMatrix_new(size, size, INT_MAX, matrix);
    free(matrix);
    return m;
}

static OrderedDitherMatrix* get_variable_4x4_matrix(int step) {
    double thresholds4x4variable[16] = { -7.5,  0.5, -5.5,  2.5,
                                          4.5, -3.5,  6.5, -1.5,
                                         -4.5,  3.5, -6.5,  1.5,
                                         7.5, -0.5,  5.5, -2.5};
    int* matrix = (int*)calloc(16, sizeof(int));
    for(int y = 0; y < 4; y++) {
        for(int x = 0; x < 4; x++) {
            double t = 127.5 + step * thresholds4x4variable[(x & 3) + ((y & 3) << 2)];
            matrix[y * 4 + x] = (int)floor((t > 255) ? 255 : ((t < 0) ? 0 : t));
        }
    }
    OrderedDitherMatrix* m = OrderedDitherMatrix_new(4, 4, 255, matrix);
    free(matrix);
    return m;
}

static OrderedDitherMatrix* get_variable_2x2_matrix(int step) {
    double thresholds2x2variable[4] = {-1.5, 1.5, 0.5, -0.5};
    int* matrix = (int*)calloc(4, sizeof(int));
    for(int y = 0; y < 2; y++) {
        for(int x = 0; x < 2; x++) {
            double t = 127.5 + step * thresholds2x2variable[(x & 1) + ((y & 1) << 1)];
            matrix[y * 2 + x] = (int)floor((t > 255) ? 255 : ((t < 0) ? 0 : t));
        }
    }
    OrderedDitherMatrix* m = OrderedDitherMatrix_new(2, 2, 255, matrix);
    free(matrix);
    return m;
}

static OrderedDitherMatrix* get_blue_noise_128x128() {
    OrderedDitherMatrix* m = OrderedDitherMatrix_new(128, 128, 0xff, blue_noise_raw);
    return m;
}

static void ordered_dither(const ImGui::ImMat& img, const OrderedDitherMatrix* matrix, float sigma, ImGui::ImMat& out)
{
    /* Ordered dithering
     * sigma: introduces noise into the final dither to make it look less regular.
     * */
    int matrix_size = matrix->width * matrix->height;
    float* dmatrix = (float*)calloc((size_t)matrix_size, sizeof(float));
    float divisor = 1.0 / matrix->divisor;
    #pragma omp parallel for num_threads(OMP_THREADS)
    for(int i = 0; i < matrix_size; i++) {
        dmatrix[i] = (double)matrix->buffer[i] * divisor - 0.5;
    }
    #pragma omp parallel for num_threads(OMP_THREADS)
    for(int y = 0; y < img.h; y++) {
        for(int x = 0; x < img.w; x++) {
            float px = img.at<uint8_t>(x, y) / 255.f;
            px += dmatrix[(y % matrix->height) * matrix->width + (x % matrix->width)];
            if(sigma > 0.0)
                px += box_muller(sigma, 0.5) - 0.5;
            if(px > 0.5)
                out.at<uint8_t>(x, y) = 0xFF;
        }
    }
    free(dmatrix);
}

void ordered_dither(const ImGui::ImMat& img, const OD_TYPE type, float sigma, ImGui::ImMat& out, const ImGui::ImMat& noise, int step, ImVec4 param)
{
    OrderedDitherMatrix* om = nullptr;
    switch (type)
    {
        case OD_BLUE_NOISE_128X128 : om = get_blue_noise_128x128(); break;
        case OD_BAYER2X2 : om = get_bayer2x2_matrix(); break;
        case OD_BAYER3X3 : om = get_bayer3x3_matrix(); break;
        case OD_BAYER4X4 : om = get_bayer4x4_matrix(); break;
        case OD_BAYER8X8 : om = get_bayer8x8_matrix(); break;
        case OD_BAYER16X16 : om = get_bayer16x16_matrix(); break;
        case OD_BAYER32X32 : om = get_bayer32x32_matrix(); break;
        case OD_DISPERSED_DOTS_1 : om = get_dispersed_dots_1_matrix(); break;
        case OD_DISPERSED_DOTS_2 : om = get_dispersed_dots_2_matrix(); break;
        case OD_ULICHNEY_VOID_DISPERSED_DOTS : om = get_ulichney_void_dispersed_dots_matrix(); break;
        case OD_NON_RECTANGULAR_1 : om = get_non_rectangular_1_matrix(); break;
        case OD_NON_RECTANGULAR_2 : om = get_non_rectangular_2_matrix(); break;
        case OD_NON_RECTANGULAR_3 : om = get_non_rectangular_3_matrix(); break;
        case OD_NON_RECTANGULAR_4 : om = get_non_rectangular_4_matrix(); break;
        case OD_ULICHNEY_BAYER_5 : om = get_ulichney_bayer_5_matrix(); break;
        case OD_ULICHNEY : om = get_ulichney_matrix(); break;
        case OD_BAYER_CLUSTERED_DOT_1 : om = get_bayer_clustered_dot_1_matrix(); break;
        case OD_BAYER_CLUSTERED_DOT_2 : om = get_bayer_clustered_dot_2_matrix(); break;
        case OD_BAYER_CLUSTERED_DOT_3 : om = get_bayer_clustered_dot_3_matrix(); break;
        case OD_BAYER_CLUSTERED_DOT_4 : om = get_bayer_clustered_dot_4_matrix(); break;
        case OD_BAYER_CLUSTERED_DOT_5 : om = get_bayer_clustered_dot_5_matrix(); break;
        case OD_BAYER_CLUSTERED_DOT_6 : om = get_bayer_clustered_dot_6_matrix(); break;
        case OD_BAYER_CLUSTERED_DOT_7 : om = get_bayer_clustered_dot_7_matrix(); break;
        case OD_BAYER_CLUSTERED_DOT_8 : om = get_bayer_clustered_dot_8_matrix(); break;
        case OD_BAYER_CLUSTERED_DOT_9 : om = get_bayer_clustered_dot_9_matrix(); break;
        case OD_BAYER_CLUSTERED_DOT_10 : om = get_bayer_clustered_dot_10_matrix(); break;
        case OD_BAYER_CLUSTERED_DOT_11 : om = get_bayer_clustered_dot_11_matrix(); break;
        case OD_CENTRAL_WHITE_POINT : om = get_central_white_point_matrix(); break;
        case OD_BALANCED_CENTERED_POINT : om = get_balanced_centered_point_matrix(); break;
        case OD_DIAGONAL_ORDERED_MATRIX : om = get_diagonal_ordered_matrix_matrix(); break;
        case OD_ULICHNEY_CLUSTERED_DOT : om = get_ulichney_clustered_dot_matrix(); break;
        case OD_MAGIC5X5_CIRCLE : om = get_magic5x5_circle_matrix(); break;
        case OD_MAGIC6X6_CIRCLE : om = get_magic6x6_circle_matrix(); break;
        case OD_MAGIC7X7_CIRCLE : om = get_magic7x7_circle_matrix(); break;
        case OD_MAGIC4X4_45 : om = get_magic4x4_45_matrix(); break;
        case OD_MAGIC6X6_45 : om = get_magic6x6_45_matrix(); break;
        case OD_MAGIC8X8_45 : om = get_magic8x8_45_matrix(); break;
        case OD_MAGIC4X4 : om = get_magic4x4_matrix(); break;
        case OD_MAGIC6X6 : om = get_magic6x6_matrix(); break;
        case OD_MAGIC8X8 : om = get_magic8x8_matrix(); break;
        case OD_VARIABLE_2X2 : om = get_variable_2x2_matrix(step); break;
        case OD_VARIABLE_4X4 : om = get_variable_4x4_matrix(step); break;
        case OD_INTERLEAVED_GRADIENT_NOISE : om = get_interleaved_gradient_noise(step, param.x, param.y, param.z); break;
        case OD_MATRIX_FROM_IMAGE : om = get_matrix_from_image(noise); break; // TODO::Dicky
        default: break;
    }
    ordered_dither(img, om, sigma, out);
    OrderedDitherMatrix_free(om);
}