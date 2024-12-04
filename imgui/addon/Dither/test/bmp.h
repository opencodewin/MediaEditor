#pragma once
#ifndef BMP_H
#define BMP_H

#include <stdint.h>

struct Bmp {
    int32_t height;
    int32_t width;
    uint16_t bpp;
    uint8_t *buffer;
    int scanline_padding;
};
typedef struct Bmp Bmp;

struct Pixel {
    uint8_t r, g, b;
};
typedef struct Pixel Pixel;

Bmp* bmp_rgb24(int width, int height);
Bmp* bmp_load(char* filename);
bool bmp_save(Bmp* self, char* filename);
void bmp_free(Bmp* self);
void bmp_setpixel(Bmp* self, int x, int y, Pixel p);
void bmp_getpixel(Bmp* self, int x, int y, Pixel* p);

#endif  // BMP_H
