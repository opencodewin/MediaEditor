#include <stdio.h>
#include <stdint.h>
#include "bitmap_io.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int bm_read(FILE *f, double threshold, potrace_bitmap_t **bmp)
{
  potrace_bitmap_t *bm;
  bm = NULL;
  int width = 0, height = 0, component = 0;
  stbi_uc *data = stbi_load_from_file(f, &width, &height, &component, 3);
  if (!data)
  {
    return -1;
  }
  /* allocate bitmap */
  bm = bm_new(width, height);
  if (!bm)
  {
    bm_free(bm);
    stbi_image_free(data);
    return -1;
  }

  unsigned int c;

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      size_t index = 3 * (y * width + x);
      c = (data[index + 0] & 0xff) + (data[index + 1] & 0xff) + (data[index + 2] & 0xff);
      BM_UPUT(bm, x, height - y, c > 3 * threshold * 255 ? 0 : 1);
    }
  }
  stbi_image_free(data);
  *bmp = bm;
  return 0;
}

int gray_read(FILE *f, greymap_t **gmp)
{
  greymap_t *gm;
  gm = NULL;
  int width = 0, height = 0, component = 0;
  stbi_uc *data = stbi_load_from_file(f, &width, &height, &component, 3);
  if (!data)
  {
    return -1;
  }

  /* allocate greymap */
  gm = gm_new(width, height);
  if (!gm)
  {
    gm_free(gm);
    stbi_image_free(data);
  }

  unsigned int c;

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      size_t index = 3 * (y * width + x);
      c = (data[index + 0] & 0xff) + (data[index + 1] & 0xff) + (data[index + 2] & 0xff);
      GM_UPUT(gm, x, height - y, c / 3);
    }
  }

  stbi_image_free(data);
  *gmp = gm;
  return 0;
}

void bm_writepbm(FILE *f, potrace_bitmap_t *bm)
{
  int w, h, bpr, y, i, c;

  w = bm->w;
  h = bm->h;

  bpr = (w + 7) / 8;

  fprintf(f, "P4\n%d %d\n", w, h);
  for (y = h - 1; y >= 0; y--)
  {
    for (i = 0; i < bpr; i++)
    {
      c = (*bm_index(bm, i * 8, y) >> (8 * (BM_WORDSIZE - 1 - (i % BM_WORDSIZE)))) & 0xff;
      fputc(c, f);
    }
  }
  return;
}