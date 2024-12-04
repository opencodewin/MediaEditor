#ifndef RENDER_H
#define RENDER_H

#include "greymap.h"

struct render_s
{
  greymap_t *gm;
  double x0, y0, x1, y1;
  int x0i, y0i, x1i, y1i;
  double a0, a1;
  int *incrow_buf;
};
typedef struct render_s render_t;

render_t *render_new(greymap_t *gm);
void render_free(render_t *rm);
void render_close(render_t *rm);
void render_moveto(render_t *rm, double x, double y);
void render_lineto(render_t *rm, double x, double y);
void render_dot(render_t *rm, double x, double y);
void render_curveto(render_t *rm, double x2, double y2, double x3, double y3, double x4, double y4);

#endif /* RENDER_H */
