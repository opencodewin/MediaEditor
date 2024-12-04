#define MODULE_API_EXPORTS
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "libdither.h"
#include "dither_kallebach_data.h"

void kallebach_dither(const ImGui::ImMat& img, bool random, ImGui::ImMat& out) {
    /* Kacker and Allebach dithering.
     * The algorithm alternates between different dither arrays. The arrays can be
     * chosen at random (parameter: random = true) or in order (parameter: random = false)
     * */
    srand((uint32_t)time(NULL));
    const int dither_array_size = 32;
    const int dither_array_count = 4;
    int height_map_m = (int)ceil((double)img.h / (double)dither_array_size);
    int width_map_m = (int)ceil((double)img.w / (double)dither_array_size);
    short* map = (short*)calloc((width_map_m + 1) * (height_map_m + 1), sizeof(short));
    int current_index = 0;
    for(int i = 0; i < img.h; i += dither_array_size) {
        for(int j = 0; j < img.w; j += dither_array_size) {
            int left_index = map[(int)((double)i / (double)dither_array_size + 1) * (width_map_m + 1) + (int)((double)j / (double)dither_array_size)];
            int upper_index = map[(int)((double)i / (double)dither_array_size) * (width_map_m + 1) + (int)((double)j / (double)dither_array_size + 1)];
            while(1) {
                if(random) {
                    current_index = rand() % (dither_array_count); // choose a dither array by random
                } else {
                    current_index++;  // go through dither arrays in order
                    if (current_index == dither_array_count)
                        current_index = 0;
                }
                if(current_index != left_index && current_index != upper_index) {
                    #pragma omp parallel for num_threads(OMP_THREADS)
                    for(int m = 0; m < dither_array_size; m++) {
                        for(int n = 0; n < dither_array_size; n++) {
                            int im = i + m;
                            int jn = j + n;
                            if(im >=0 && im < img.h && jn >=0 && jn < img.w) {
                                //size_t addr = im * img->width + jn;
                                if(img.at<uint8_t>(jn, im) > dither_arrays[current_index][m][n])
                                    out.at<uint8_t>(jn, im) = 0xFF;// out[addr] = 0xff;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
    free(map);
}
