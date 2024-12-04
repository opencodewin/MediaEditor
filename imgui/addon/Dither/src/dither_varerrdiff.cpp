#define MODULE_API_EXPORTS
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include "libdither.h"
#include "dither_varerrdiff_data.h"


void variable_error_diffusion_dither(const ImGui::ImMat& img, const VD_TYPE type, bool serpentine, ImGui::ImMat& out) {
    /* Variable Error Diffusion, implementing Ostromoukhov's and Zhou Fang's approach */
    srand((uint32_t)time(NULL));
    // dither matrix
    const int m_offset_x[2][3] = {{1, -1, 0}, {-1, 1, 0}};
    const int m_offset_y[2][3] = {{0, 1, 1}, {0, 1, 1}};

    double* buffer = (double*)calloc(img.w * img.h, sizeof(double));
    const long* divs;
    const long* coefs;
    if(type == VD_OSTROMOUKHOV) {
        coefs = ostro_coefs;
        divs = ostro_divs;
    } else {  // zhoufang
        coefs = zhoufang_coef;
        divs = zhoufang_divs;
        #pragma omp parallel for num_threads(OMP_THREADS)
        for(int y = 0; y < img.h; y++) {
            for (int x = 0; x < img.w; x++)
            {
                size_t addr = y * img.w + x;
                buffer[addr] = img.at<uint8_t>(x, y) / 255.f;
            }
        }
    }
    // serpentine direction setup
    int direction = 0; // FORWARD
    int direction_toggle = 1;
    if(serpentine) direction_toggle = 2;
    // start dithering
    for(int y = 0; y < img.h; y++) {
        int start, end, step;
        // get direction
        if (direction == 0) {
            start = 0;
            end = img.w;
            step = 1;
        } else {
            start = img.w - 1;
            end = -1;
            step = -1;
        }
        for (int x = start; x != end; x += step) {
            double err;
            int coef_offs;
            size_t addr = y * img.w + x;
            double px = img.at<uint8_t>(x, y) / 255.f;
            // dither function
            if(type == VD_OSTROMOUKHOV) {   // ostro
                err = buffer[addr] + px;
                if (err > 0.5) {
                    out.at<uint8_t>(x, y) = 0xFF;
                    err -= 1.0;
                }
            } else {  // zhoufang
                err = buffer[addr];
                if (px >= 0.5)
                    px = 1.0 - px;
                double threshold = (128.0 + (rand() % 128) * (rand_scale[(int)(px * 128.0)] / 100.0)) / 256.0;
                if (err >= threshold) {
                    out.at<uint8_t>(x, y) = 0xFF;
                    err = buffer[addr] - 1.0;
                }
            }
            coef_offs = (int) (px * 255.0 + 0.5);
            // distribute the error
            err /= (double)divs[coef_offs];
            #pragma omp parallel for num_threads(OMP_THREADS)
            for(int i = 0; i < 3; i++) {
                int xx = x + m_offset_x[direction][i];
                if(-1 < xx && xx < img.w) {
                    int yy = y + m_offset_y[direction][i];
                    if(yy < img.h) {
                        buffer[yy * img.w + xx] += err * (double)coefs[coef_offs * 3 + i];
                    }
                }
            }
        }
        direction = (y + 1) % direction_toggle;
    }
    free(buffer);
}
