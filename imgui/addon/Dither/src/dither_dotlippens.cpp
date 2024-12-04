#define MODULE_API_EXPORTS
#include <stdlib.h>
#include <string.h>
#include "libdither.h"
#include "dither_dotlippens_data.h"
#include "dither_dotdiff_data.h"

struct DotLippensCoefficients {
    int* buffer;  // buffer for flat matrix array
    int  width;
    int  height;
};

#define DotClassMatrix DotLippensCoefficients

static int* create_dot_lippens_cm() {
    int cm[4][16][16];
    for(size_t i = 0; i < 16; i++) {
        for(size_t j = 0; j < 16; j++) {
            cm[3][i][j] = ocm[i][j];
            cm[2][i][j] = ocm[15 - i][15 - j];
            cm[1][i][j] = ocm[15 - j][15 - i];
            cm[0][i][j] = ocm[j][i];
        }
    }
    int* final_cm = (int*)calloc(128 * 128, sizeof(int));
    for(size_t i = 0; i < 128; i += 16)
        for(size_t j = 0; j < 128; j += 16)
            for(size_t m = 0; m < 16; m++)
                for(size_t n = 0; n < 16; n++)
                    final_cm[(i + m) * 128 + (j + n)] = cm[order[(int)((float)i / 16.0)][(int)((float)j / 16.0)]][m][n];
    return final_cm;
}

static DotLippensCoefficients* DotLippensCoefficients_new(int width, int height, const int* coefficients) {
    DotLippensCoefficients* self = (DotLippensCoefficients*)calloc(1, sizeof(DotLippensCoefficients));
    self->buffer = (int*)calloc(width * height, sizeof(int));
    memcpy(self->buffer, coefficients, width * height * sizeof(int));
    self->height = height;
    self->width = width;
    return self;
}

static void DotLippensCoefficients_free(DotLippensCoefficients* self) {
    if(self) {
        free(self->buffer);
        free(self);
        self = NULL;
    }
}

static DotClassMatrix* get_dotlippens_class_matrix() { return DotLippensCoefficients_new(128, 128, dotlippens_class_matrix); }
static DotClassMatrix* get_guoliu_16x16_class_matrix() { return DotLippensCoefficients_new(16, 16, guoliu_16x16_class_matrix); }
static DotClassMatrix* get_mese_16x16_class_matrix() { return DotLippensCoefficients_new(16, 16, mese_16x16_class_matrix); }
static DotClassMatrix* get_knuth_class_matrix() { return DotLippensCoefficients_new(8, 8, knuth_class_matrix); }
static DotLippensCoefficients* get_dotlippens_coefficients1() { return DotLippensCoefficients_new(5, 5, dotlippens1_coe); }
static DotLippensCoefficients* get_dotlippens_coefficients2() { return DotLippensCoefficients_new(5, 5, dotlippens2_coe); }
static DotLippensCoefficients* get_dotlippens_coefficients3() { return DotLippensCoefficients_new(5, 5, dotlippens3_coe); }

static void dotlippens_dither(const ImGui::ImMat& img, const DotClassMatrix* class_matrix, const DotLippensCoefficients* coefficients, ImGui::ImMat& out) {
    /* Lippens and Philips Dot Dithering
     * class_matix: same class matrix as used by regular (Knuth's) dot ditherer
     * coefficients: Lippens and Philips coefficients */
    double coefficients_sum = 0.0;
    for(int i = 0; i < coefficients->width * coefficients->height; i++)
        coefficients_sum += (double)coefficients->buffer[i];
    coefficients_sum /= 2.0;

    int* image_cm = (int*)calloc(img.w * img.h, sizeof(int));
    double* image = (double*)calloc(img.w * img.h, sizeof(double));

    for(int y = 0; y < img.h; y++) {
        for(int x = 0; x < img.w; x++) {
            size_t addr = y * img.w + x;
            image_cm[addr] = class_matrix->buffer[(y % class_matrix->height) * class_matrix->width + (x % class_matrix->width)];
            image[addr] = img.at<uint8_t>(x, y) / 255.0; // make a copy of the image as we can't modify the original
        }
    }
    int half_size = (int)(((float)coefficients->width - 1.0) / 2.0);
    int n = 0;
    while(n != 256) {
        #pragma omp parallel for num_threads(OMP_THREADS)
        for(int y = 0; y < img.h; y++) {
            for (int x = 0; x < img.w; x++) {
                size_t addr = y * img.w + x;
                if(image_cm[addr] == n) {
                    double err = image[addr];
                    if(err > 0.5) {
                        err -= 1.0;
                        out.at<uint8_t>(x, y) = 0xFF; //out[addr] = 0xff;
                    }
                    for(int cmy = -half_size; cmy <= half_size; cmy++) {
                        for(int cmx = -half_size; cmx <= half_size; cmx++) {
                            int imy = y + cmy;
                            int imx = x + cmx;
                            addr = imy * img.w + imx;
                            if(imy >= 0 && imy < img.h && imx >= 0 && imx < img.w)
                                if (image_cm[addr] > cmx)
                                    image[addr] += err * (double)coefficients->buffer[(cmy + half_size) * coefficients->width + (cmx + half_size)] / coefficients_sum;
                        }
                    }
                }
            }
        }
        n++;
    }
    free(image_cm);
    free(image);
}

void dotlippens_dither(const ImGui::ImMat& img, const LP_TYPE type, ImGui::ImMat& out)
{
    DotClassMatrix* cm = nullptr;
    DotLippensCoefficients* coe = nullptr;
    switch (type)
    {
        case LP_V1:
            cm = get_dotlippens_class_matrix();
            coe = get_dotlippens_coefficients1();
            break;
        case LP_V2:
            cm = get_dotlippens_class_matrix();
            coe = get_dotlippens_coefficients2();
            break;
        case LP_V3:
            cm = get_dotlippens_class_matrix();
            coe = get_dotlippens_coefficients3();
            break;
        case LP_GUO_LIU_16X16:
            cm = get_guoliu_16x16_class_matrix();
            coe = get_dotlippens_coefficients1();
            break;
        case LP_MESE_AND_VAIDYANATHAN_16X16:
            cm = get_mese_16x16_class_matrix();
            coe = get_dotlippens_coefficients1();
            break;
        case LP_KNUTH:
            cm = get_knuth_class_matrix();
            coe = get_dotlippens_coefficients1();
            break;
        default: break;
    }
    dotlippens_dither(img, cm, coe, out);
    DotLippensCoefficients_free(coe);
    DotLippensCoefficients_free(cm);
}