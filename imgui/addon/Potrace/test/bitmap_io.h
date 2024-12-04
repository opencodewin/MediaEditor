#ifndef BITMAP_IO_H
#define BITMAP_IO_H

#include <stdio.h>
#include "bitmap.h"
#include "greymap.h"

/* Note that bitmaps are stored bottom to top, i.e., the first
   scanline is the bottom-most one */

int bm_read(FILE *f, double blacklevel, potrace_bitmap_t **bmp);
int gray_read(FILE *f, greymap_t **gmp);
void bm_writepbm(FILE *f, potrace_bitmap_t *bm);
int bm_print(FILE *f, potrace_bitmap_t *bm);

#endif /* BITMAP_IO_H */
