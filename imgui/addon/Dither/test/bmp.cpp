#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "bmp.h"

const int HEADER_SIZE = 54;

void set_scanline_padding(Bmp* self) {
    self->scanline_padding = 4 - ((self->width * self->bpp) % 4);
    if(self->scanline_padding == 4)
        self->scanline_padding = 0;
}

Bmp* bmp_rgb24(int width, int height) {
    Bmp* self = (Bmp*)calloc(1, sizeof(Bmp));
    self->width = width;
    self->height = height;
    self->bpp = 3; // 3 bytes per pixel for r, g, b (no alpha)
    set_scanline_padding(self);
    int32_t img_size = (self->width * self->bpp + self->scanline_padding) * self->height;
    self->buffer = (uint8_t*)calloc(img_size, sizeof(uint8_t));
    return self;
}

Bmp* bmp_load(char* filename) {
    Bmp* self = (Bmp*)calloc(1, sizeof(Bmp));
    struct stat sb;
    if (stat(filename, &sb) == -1) {
        fprintf(stderr, "ERROR: cannot open file %s\n", filename);
        return NULL;
    }
    size_t bytes_read = 0;
    FILE* fp = fopen(filename, "rb");
    uint8_t *header = (uint8_t *) calloc(HEADER_SIZE, sizeof(uint8_t));
    bytes_read = fread(header, sizeof(uint8_t), HEADER_SIZE, fp);
    if (bytes_read != HEADER_SIZE) {
        free(header);
        fprintf(stderr, "ERROR: could not read BMP header\n");
        return NULL;
    }
    if (*(uint16_t*)(header) != 0x4d42) {
        free(header);
        fprintf(stderr, "ERROR: file %s is not a BMP file\n", filename);
        return NULL;
    }
    if (*(uint32_t*)(header + 30) != 0) {
        free(header);
        fprintf(stderr, "ERROR: wrong format. BMP must be uncompressed\n");
        return NULL;
    }
    if (*(uint32_t*)(header + 10) != 54) {
        free(header);
        fprintf(stderr, "ERROR: wrong BMP format.\n"); // might have extra information after header, before image data
        return NULL;
    }
    self->height = *(int32_t*)(header + 22);
    self->width = *(int32_t*)(header + 18);
    self->bpp = *(uint16_t*)(header + 28) / 8;
    set_scanline_padding(self);
    free(header);
    self->buffer = (uint8_t *)calloc(sb.st_size - HEADER_SIZE, sizeof(uint8_t));
    bytes_read = fread(self->buffer, sizeof(uint8_t), sb.st_size - HEADER_SIZE, fp);
    if (bytes_read != sb.st_size - HEADER_SIZE) {
        free(self->buffer);
        fprintf(stderr, "ERROR: could not read BMP data\n");
        return NULL;
    }
    fclose(fp);
    return self;
}

bool bmp_save(Bmp* self, char* filename) {
    int32_t img_size = ((self->width * self->bpp) + self->scanline_padding) * self->height;
    uint8_t* header = (uint8_t*)calloc(HEADER_SIZE, sizeof(uint8_t));
    *(uint16_t*)(header) = 0x4d42;
    *(uint32_t*)(header + 2) = img_size + HEADER_SIZE;
    *(uint32_t*)(header + 10) = HEADER_SIZE;
    *(uint32_t*)(header + 14) = 40;
    *(int32_t*)(header + 18) = self->width;
    *(int32_t*)(header + 22) = self->height;
    *(uint16_t*)(header + 26) = 1;
    *(int32_t*)(header + 28) = self->bpp * 8;
    size_t bytes_written = 0;
    FILE* fp = fopen(filename, "wb");
    bytes_written = fwrite(header, sizeof(uint8_t), HEADER_SIZE, fp);
    free(header);
    if (bytes_written != HEADER_SIZE) {
        fprintf(stderr, "ERROR: could not write BMP file\n");
        return false;
    }
    bytes_written = fwrite(self->buffer, sizeof(uint8_t), img_size, fp);
    if (bytes_written != img_size) {
        fprintf(stderr, "ERROR: could not write BMP file\n");
        return false;
    }
    fclose(fp);
    return true;
}

void bmp_free(Bmp* self) {
    if(self) {
        free(self->buffer);
        free(self);
        self = NULL;
    }
}

void bmp_getpixel(Bmp* self, int x, int y, Pixel* p) {
    size_t offset = (self->width * self->bpp + self->scanline_padding) * y + (x * self->bpp);
    p->r = self->buffer[offset];
    p->g = self->buffer[offset + 1];
    p->b = self->buffer[offset + 2];
}

void bmp_setpixel(Bmp* self, int x, int y, Pixel p) {
    size_t offset = (self->width * self->bpp + self->scanline_padding) * y + (x * self->bpp);
    self->buffer[offset] = p.r;
    self->buffer[offset + 1] = p.g;
    self->buffer[offset + 2] = p.b;
}
