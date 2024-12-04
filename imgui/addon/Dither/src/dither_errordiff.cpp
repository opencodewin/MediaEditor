#define MODULE_API_EXPORTS
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "libdither.h"
#include "random.h"
#include "dither_errordiff_data.h"

struct ErrorDiffusionMatrix {
    double divisor;
    int* buffer;  // buffer for flat matrix array
    int  width;
    int  height;
};

/* ***** ERROR DIFFUSION MATRIX OBJECT STRUCT ***** */
static ErrorDiffusionMatrix* ErrorDiffusionMatrix_new(int width, int height, double divisor, const int* matrix) {
    ErrorDiffusionMatrix* self = (ErrorDiffusionMatrix*)calloc(1, sizeof(ErrorDiffusionMatrix));
    self->buffer = (int*)calloc(width * height, sizeof(int));
    memcpy(self->buffer, matrix, width * height * sizeof(int));
    self->width = width;
    self->height = height;
    self->divisor = divisor;
    return self;
}

static void ErrorDiffusionMatrix_free(ErrorDiffusionMatrix* self) {
    if(self) {
        free(self->buffer);
        free(self);
        self = NULL;
    }
}

/* ***** BUILT-IN DIFFUSION MATRICES ***** */

static ErrorDiffusionMatrix* get_xot_matrix() { return ErrorDiffusionMatrix_new(14, 10, 355, matrix_xot); }
static ErrorDiffusionMatrix* get_diagonal_matrix() { return ErrorDiffusionMatrix_new(3, 2, 16, matrix_diagonal); }
static ErrorDiffusionMatrix* get_floyd_steinberg_matrix() { return ErrorDiffusionMatrix_new(3, 2, 16, matrix_floyd_steinberg); }
static ErrorDiffusionMatrix* get_shiaufan3_matrix() { return ErrorDiffusionMatrix_new(4, 2, 8, matrix_shiaufan_3); }
static ErrorDiffusionMatrix* get_shiaufan2_matrix() { return ErrorDiffusionMatrix_new(4, 2, 16, matrix_shiaufan_2); }
static ErrorDiffusionMatrix* get_shiaufan1_matrix() { return ErrorDiffusionMatrix_new(5, 2, 16, matrix_shiaufan_1); }
static ErrorDiffusionMatrix* get_stucki_matrix() { return ErrorDiffusionMatrix_new(5, 3, 42, matrix_stucki); }
static ErrorDiffusionMatrix* get_diffusion_1d_matrix() { return ErrorDiffusionMatrix_new(2, 1, 1, matrix_diffusion_1d); }
static ErrorDiffusionMatrix* get_diffusion_2d_matrix() { return ErrorDiffusionMatrix_new(2, 2, 2, matrix_diffusion_2d); }
static ErrorDiffusionMatrix* get_fake_floyd_steinberg_matrix() { return ErrorDiffusionMatrix_new(2, 2, 8, matrix_fake_floyd_steinberg); }
static ErrorDiffusionMatrix* get_jarvis_judice_ninke_matrix() { return ErrorDiffusionMatrix_new(5, 3, 48, matrix_jarvis_judice_ninke); }
static ErrorDiffusionMatrix* get_atkinson_matrix() { return ErrorDiffusionMatrix_new(4, 3, 8, matrix_atkinson); }
static ErrorDiffusionMatrix* get_burkes_matrix() { return ErrorDiffusionMatrix_new(5, 2, 32, matrix_burkes); }
static ErrorDiffusionMatrix* get_sierra_3_matrix() { return ErrorDiffusionMatrix_new(5, 3, 32, matrix_sierra_3); }
static ErrorDiffusionMatrix* get_sierra_2row_matrix() { return ErrorDiffusionMatrix_new(5, 2, 16, matrix_sierra_2row); }
static ErrorDiffusionMatrix* get_sierra_lite_matrix() { return ErrorDiffusionMatrix_new(3, 2, 4, matrix_sierra_lite); }
static ErrorDiffusionMatrix* get_steve_pigeon_matrix() { return ErrorDiffusionMatrix_new(5, 3, 14, matrix_steve_pigeon); }
static ErrorDiffusionMatrix* get_robert_kist_matrix() { return ErrorDiffusionMatrix_new(5, 3, 220, matrix_robert_kist); }
static ErrorDiffusionMatrix* get_stevenson_arce_matrix() { return ErrorDiffusionMatrix_new(7, 4, 200, matrix_stevenson_arce); }

/* ***** ERROR DIFFUSION DITHER FUNCTION ***** */

static void error_diffusion_dither(const ImGui::ImMat& img,
                                const ErrorDiffusionMatrix* m,
                                bool serpentine,
                                float sigma,
                                ImGui::ImMat& out)
{
    /* Error Diffusion dithering
     * img: source image to be dithered
     * serpentine:
     * sigma: jitter
     * setPixel: callback function to set a pixel
     */
    // prepare the matrix...
    int i = 0;
    int j = 0;
    float *m_weights = NULL;
    int *m_offset_x = NULL;
    int *m_offset_y = NULL;
    int matrix_length = 0;
    for(int y = 0; y < m->height; y++) {
        for(int x = 0; x < m->width; x++) {
            int value = m->buffer[y * m->width + x];
            if(value == -1) {
                matrix_length = (m->width * m->height - i - 1);
                m_weights = (float *)calloc((size_t)matrix_length * 2, sizeof(float));
                m_offset_x = (int *)calloc((size_t)matrix_length * 2, sizeof(int));
                m_offset_y = (int *)calloc((size_t)matrix_length, sizeof(int));
            } else if(value > 0) {
                m_weights[j] = (float)value;
                m_weights[j + matrix_length] = (float)value;
                m_offset_x[j] = (x - i);
                m_offset_x[j + matrix_length] = -(x - i);
                m_offset_y[j] = y;
                j++;
            } if(matrix_length == 0) {
                i++;
            }
        }
    }
    // do the error diffusion...
    float* buffer = (float* )calloc((size_t)(img.w * img.h), sizeof(float));
    #pragma omp parallel for num_threads(OMP_THREADS)
    for(int y = 0; y < img.h; y++) {
        for (int x = 0; x < img.w; x++)
        {
            size_t addr = y * img.w + x;
            buffer[addr] = img.at<uint8_t>(x, y) / 255.f;
        }
    }

    int direction = 0; // FORWARD
    int direction_toggle = 1;
    if(serpentine) direction_toggle = 2;

    float threshold = 0.5;
    for(int y = 0; y < img.h; y++) {
        int start, end, step;
        if(direction == 0) {
            start = 0;
            end = img.w;
            step = 1;
        } else {
            start = img.w - 1;
            end = -1;
            step = -1;
        }
        for (int x = start; x != end; x += step) {
            size_t addr = y * img.w + x;
            float err = buffer[addr];
            if(sigma > 0.0)
                threshold = box_muller(sigma, 0.5);
            if(err > threshold) {
                out.at<uint8_t>(x, y) = 0xFF;
                err -= 1.0;
            }
            err /= m->divisor;
            #pragma omp parallel for num_threads(OMP_THREADS)
            for(int g = 0; g < matrix_length; g++) {
                int xx = x + m_offset_x[g + matrix_length * direction];
                if(-1 < xx && xx < img.w) {
                    int yy = y + m_offset_y[g];
                    if(yy < img.h) {
                        buffer[yy * img.w + xx] += err * m_weights[g + matrix_length * direction];
                    }
                }
            }
        }
        direction = (y + 1) % direction_toggle;
    }
    free(buffer);
    free(m_weights);
    free(m_offset_x);
    free(m_offset_y);
}

void error_diffusion_dither(const ImGui::ImMat& img, const ED_TYPE type, bool serpentine, float sigma, ImGui::ImMat& out)
{
    ErrorDiffusionMatrix* em = nullptr;
    switch (type)
    {
        case ED_XOT : em = get_xot_matrix(); break;
        case ED_DIAGONAL : em = get_diagonal_matrix(); break;
        case ED_FLOYD_STEINBERG : em = get_floyd_steinberg_matrix(); break;
        case ED_SHIAUFAN3 : em = get_shiaufan3_matrix(); break;
        case ED_SHIAUFAN2 : em = get_shiaufan2_matrix(); break;
        case ED_SHIAUFAN1 : em = get_shiaufan1_matrix(); break;
        case ED_STUCKI : em = get_stucki_matrix(); break;
        case ED_DIFFUSION_1D : em = get_diffusion_1d_matrix(); break;
        case ED_DIFFUSION_2D : em = get_diffusion_2d_matrix(); break;
        case ED_FAKE_FLOYD_STEINBERG : em = get_fake_floyd_steinberg_matrix(); break;
        case ED_JARVIS_JUDICE_NINKE : em = get_jarvis_judice_ninke_matrix(); break;
        case ED_ATKINSON : em = get_atkinson_matrix(); break;
        case ED_BURKES : em = get_burkes_matrix(); break;
        case ED_SIERRA_3 : em = get_sierra_3_matrix(); break;
        case ED_SIERRA_2ROW : em = get_sierra_2row_matrix(); break;
        case ED_SIERRA_LITE : em = get_sierra_lite_matrix(); break;
        case ED_STEVE_PIGEON : em = get_steve_pigeon_matrix(); break;
        case ED_ROBERT_KIST : em = get_robert_kist_matrix(); break;
        case ED_STEVENSON_ARCE : em = get_stevenson_arce_matrix(); break;
        default: break;
    }
    error_diffusion_dither(img, em, serpentine, sigma, out);
    ErrorDiffusionMatrix_free(em);
}