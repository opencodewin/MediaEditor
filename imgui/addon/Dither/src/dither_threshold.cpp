#define MODULE_API_EXPORTS
#include <stdlib.h>
#include "libdither.h"
#include "random.h"

static double gamma_decode(double c) {
    /* converts a sRGB input (in the range 0.0-1.0) to linear color space */
    if(c <= 0.04045)
        return c / 12.02;
    else
        return pow(((c + 0.055) / 1.055), 2.4);
}

static double gamma_encode(double c) {
    /* converts a linear color input (in the range 0.0-1.0) to sRGB color space */
    if(c <= 0.0031308)
        return 12.92 * c;
    else
        return (1.055 * pow(c, (1.0 / 2.4))) - 0.055;
}

double auto_threshold(const ImGui::ImMat& img) {
    /* automatically determines the best threshold value for the image.
     * use output of this function as threshold parameter for the threshold dither function */
    double avg = 0.0;
    double min = 1.0;
    double max = 0.0;
    size_t imgsize = img.w * img.h;
    for(int y = 0; y < img.h; y++)
    {
        for (int x = 0; x < img.w; x++)
        {
            double c = gamma_encode(img.at<uint8_t>(x, y) / 255.0);
            avg += c;
            if(c < min) min = c;
            if(c > max) max = c;
        }
    }
    avg /= (double)(img.w * img.h);
    double v = (1.0 - (max - min)) * 0.5;
    if(avg < gamma_decode(0.5))
        v = -v;
    return gamma_decode(avg + v);
}

void threshold_dither(const ImGui::ImMat&  img, double threshold, double noise, ImGui::ImMat& out) {
    /* Threshold dithering
     * threshold: threshold to dither a pixel black. From 0.0 to 1.0. Suggested value: 0.5.
     * noise: amount of noise / randomness in pixel placement
     * */
    threshold = (0.5 * noise + threshold * (1.0 - noise));
    #pragma omp parallel for num_threads(OMP_THREADS)
    for(int y = 0; y < img.h; y++) {
        for(int x = 0; x < img.w; x++) {
            double px = img.at<uint8_t>(x, y) / 255.0;
            if(noise > 0)
                px += (rand_float() - 0.5) * noise;
            if(px > threshold)
                out.at<uint8_t>(x, y) = 0xFF;
        }
    }
}
