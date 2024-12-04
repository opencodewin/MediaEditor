#define MODULE_API_EXPORTS
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include "libdither.h"


#define MIN(a,b) (((a)<(b))?(a):(b))

void grid_dither(const ImGui::ImMat& img, int w, int h, int min_pixels, bool alt_algorithm, ImGui::ImMat& out) {
    srand((uint32_t)time(NULL));
    size_t dimensions = (size_t)(img.w * img.h);
    out.fill((int8_t)0xFF);
    int grid_width = w;
    int grid_height = h;
    int grid_area = grid_width * grid_height;
    int max_pixels = grid_area;
    float maxn = (float)(pow(max_pixels, 2.0)) / ((float)grid_area / 4.0);
    for(int y = 0; y < img.h; y += grid_height) {
        for(int x = 0; x < img.w; x += grid_width) {
            float sum_intensity = 0.0;
            int samplecount = 0;
            for (int yy = 0; yy < grid_height; yy++) {
                for (int xx = 0; xx < grid_width; xx++, samplecount++) {
                    if (y + yy < img.h && x + xx < img.w)
                        sum_intensity += img.at<uint8_t>(x + xx, y + yy) / 255.0;
                }
            }

            float avg_intensity = sum_intensity / (float)samplecount;
            float n = pow((1.0 - avg_intensity) * max_pixels, 2.0) / ((float)samplecount / 4.0);
            if(n < min_pixels)
                n = 0.0;
            if(alt_algorithm) {
                int* o = (int*)calloc(grid_area, sizeof(int));
                int limit = (int)round((n * (float)grid_area) / maxn);
                int c = 0;
                for(int i = 0; i < grid_area; i++) {
                    while(true) {
                        int xr = rand() % (grid_width);
                        int yr = rand() % (grid_height);
                        if(o[yr * grid_width + xr] == 0) {
                            if(x + xr < img.w && y + yr < img.h)
                                out.at<uint8_t>(x + xr, y + yr) = 0;
                            o[yr * grid_width + xr] = 1;
                            c++;
                            break;
                        }
                    }
                    if(c > limit)
                        break;
                }
                free(o);
            } else {
                for (int i = 0; i < (int) n; i++) {
                    int xx = x + (rand() % (MIN(x + grid_width, img.w) - x));
                    int yy = y + (rand() % (MIN(y + grid_height, img.h) - y));
                    if (xx < img.w && yy < img.h)
                        out.at<uint8_t>(xx, yy) = 0;
                }
            }
        }
    }
}
