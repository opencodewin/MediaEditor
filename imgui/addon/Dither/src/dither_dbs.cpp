#define MODULE_API_EXPORTS
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "libdither.h"

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

struct Matrix {
    double* buffer;  // buffer for flat matrix array
    int width;
    int height;
};

static Matrix* Matrix_new(int width, int height) {
    Matrix* self = (Matrix*)calloc(1, sizeof(Matrix));
    self->buffer = (double*)calloc(width * height, sizeof(double));
    self->width = width;
    self->height = height;
    return self;
}

static void Matrix_free(Matrix* self) {
    if(self) {
        free(self->buffer);
        free(self);
        self = NULL;
    }
}

static void conv2d(const Matrix* matrix, const Matrix* kernel, Matrix** out) {
    /* Same function as scipy.correlate2d(matrix, kernel, mode='full') or xcorr2 in Matlab */
    int kw = kernel->width;      // kernel width
    int kh = kernel->height;     // kernel height
    int mw = matrix->width;      // matrix width
    int mh = matrix->height;     // matrix height
    int ow = mw + kw - 1;        // out width
    int oh = mh + kh - 1;        // out height
    *out = Matrix_new(ow, oh);
    #pragma omp parallel for num_threads(OMP_THREADS)
    for(int oy = 0; oy < oh; oy++) {
        for(int ox = 0; ox < ow; ox++) {
            for(int ky = 0; ky < kh; ky++) {
                for(int kx = 0; kx < kw; kx++) {
                    int mx = ox + kx - kw + 1;
                    int my = oy + ky - kh + 1;
                    if(my >=0 && my < mh && mx >=0 && mx < mw)
                        (*out)->buffer[oy * ow + ox] += kernel->buffer[ky * kw + kx] * matrix->buffer[my * mw + mx];
                }
            }
        }
    }
}

static void get_cep(const ImGui::ImMat& img, int width, int height, int v, Matrix** cpp, Matrix** cep) {
    const int fs = 7;
    double c = 0.0;
    double d = (fs - 1.0) / 6.0;
    int glen = (int)((fs - 1.0) / 2.0);
    Matrix* gf = Matrix_new(fs, fs);
    for(int k = -glen; k <= glen; k++) {
        for(int l = -glen; l <= glen; l++) {
            // different functions for matrix generation; higher numbers produce more coarse images
            switch(v) {
                case 0:
                    c = k * k + l * l;
                    break;
                case 1:
                    c = (k * k + l * l) / (2.0 * d * d);
                    break;
                case 2:
                    c = (k * k + l * l) / 2.25;
                    break;
                case 4:
                    c = (k * k + l * l) / 4.5;
                    break;
                case 5:
                    c = (k * k + l * l) / 8.0;
                    break;
                case 6:
                    c = (k * k + l * l) / 16.0;
                    break;
            }
            if(v != 3) {
                gf->buffer[(l + glen) * fs + (k + glen)] = exp(-c) / (2.0 * M_PI * d * d);
            } else {
                c = (k * k + l * l) / 1.5;
                gf->buffer[(l + glen) * fs + (k + glen)] = (2.0 * exp(-c) + exp(-(k * k + l * l) / 8.0)) / (2.0 * M_PI * d * d);
            }
        }
    }
    // auto-correlation of gaussian filter
    conv2d(gf, gf, cpp);
    // initial error and cross-correlation between error and Gaussian
    Matrix* err = Matrix_new(width, height);
    for(int y = 0, i = 0; y < height; y++) {
        for(int x = 0; x < width; x++, i++) {
            err->buffer[i] = 0.0 - img.at<uint8_t>(x, y) / 255.0; //img->buffer[i];
        }
    }
    conv2d(err, *cpp, cep);
    Matrix_free(gf);
    Matrix_free(err);
}

void dbs_dither(const ImGui::ImMat& img, int v, ImGui::ImMat& out) {
    /*
     * DBS dithering. Ported and adapted from Sankar Srinivasan's DBS ditherer (https://github.com/SankarSrin)
     * parameter v: 0 - 6. choose between 7 functions for matrix generation. The higher the number the coarser the output dither.
     */
    Matrix* cep = NULL;
    Matrix* cpp = NULL;
    const int half_cpp_size = 6;
    get_cep(img, img.w, img.h, v, &cpp, &cep);
    int8_t* dst = (int8_t*)calloc(img.w * img.h, sizeof(int8_t));
    while(1) {
        int count_b = 0;
        #pragma omp parallel for num_threads(OMP_THREADS)
        for(int i = 0; i < img.h; i++) {
            for(int j = 0; j < img.w; j++) {
                int8_t a0c = 0, a1c = 0, cpx = 0, cpy = 0;
                double eps_min = 0.0;
                for(int8_t y = -1; y <= 1; y++) {
                    if(i + y < 0 || i + y >= img.h)
                        continue;
                    for(int8_t x = -1; x <= 1; x++) {
                        int8_t a1 = 0, a0 = 0;
                        double eps = 0.0;
                        if(j + x < 0 || j + x >= img.w)
                            continue;
                        size_t addr = i * img.w + j;
                        if(y == 0 && x == 0) {
                            a1 = 0;
                            a0 = dst[addr] == 1? -1 : 1;
                        } else {
                            if(dst[(i + y) * img.w + (j + x)] != dst[addr]) {
                                a0 = dst[addr] == 1? -1 : 1;
                                a1 = (int8_t)-a0;
                            } else {
                                a0 = 0;
                                a1 = 0;
                            }
                        }
                        eps = (a0 * a0 + a1 * a1) *
                                cpp->buffer[half_cpp_size * cpp->width + half_cpp_size] + 2 * a0 * a1 *
                                cpp->buffer[(half_cpp_size + y) * cpp->width + (half_cpp_size + x)] + 2 * a0 *
                                cep->buffer[(half_cpp_size + i) * cep->width + (half_cpp_size + j)] + 2 * a1 *
                                cep->buffer[(half_cpp_size + i + y) * cep->width + (half_cpp_size + j + x)];
                        if(eps_min > eps) {
                            eps_min = eps;
                            a0c = a0;
                            a1c = a1;
                            cpx = x;
                            cpy = y;
                        }
                    }
                }
                if(eps_min < 0) {
                    for(int y = -half_cpp_size; y <= half_cpp_size; y++)
                        for(int x = -half_cpp_size; x <= half_cpp_size; x++)
                            cep->buffer[(half_cpp_size + i + y) * cep->width + (half_cpp_size + j + x)] +=
                                    (cpp->buffer[(half_cpp_size + y) * cpp->width + (half_cpp_size + x)] * a0c);
                    for(int y = -half_cpp_size; y <= half_cpp_size; y++)
                        for(int x = -half_cpp_size; x <= half_cpp_size; x++)
                            cep->buffer[(half_cpp_size + i + y + cpy) * cep->width + (half_cpp_size + j + x + cpx)] +=
                                    (cpp->buffer[(half_cpp_size + y) * cpp->width + (half_cpp_size + x)] * a1c);
                    dst[i * img.w + j] = (int8_t)(dst[i * img.w + j] + a0c);
                    dst[(i + cpy) * img.w + (j + cpx)] = (int8_t)(dst[(i + cpy) * img.w + (j + cpx)] + a1c);
                    count_b++;
                }
            }
        }
        if(count_b == 0)
            break;
    }
    #pragma omp parallel for num_threads(OMP_THREADS)
    for (int y = 0; y < img.h; y++)
    {
        for (int x = 0; x < img.w; x++)
        {
            size_t addr = y * img.w + x;
            if(dst[addr] == 1) out.at<uint8_t>(x, y) = 0xFF;
        }
    }
    free(dst);
    Matrix_free(cpp);
    Matrix_free(cep);
}
