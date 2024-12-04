#ifndef BACKEND_H
#define BACKEND_H

#include <stdio.h>
#include "potracelib.h"
#include "auxiliary.h"
#include "trans.h"

/* structure to hold a dimensioned value */
struct dim_s
{
  double x; /* value */
  double d; /* dimension (in pt), or 0 if not given */
};
typedef struct dim_s dim_t;

#define DIM_IN (72)
#define DIM_CM (72 / 2.54)
#define DIM_MM (72 / 25.4)
#define DIM_PT (1)

/* set some configurable defaults */

#ifdef USE_METRIC
#define DEFAULT_DIM DIM_CM
#define DEFAULT_DIM_NAME "centimeters"
#else
#define DEFAULT_DIM DIM_IN
#define DEFAULT_DIM_NAME "inches"
#endif

#ifdef USE_A4
#define DEFAULT_PAPERWIDTH 595
#define DEFAULT_PAPERHEIGHT 842
#define DEFAULT_PAPERFORMAT "a4"
#else
#define DEFAULT_PAPERWIDTH 612
#define DEFAULT_PAPERHEIGHT 792
#define DEFAULT_PAPERFORMAT "letter"
#endif

static inline double double_of_dim(dim_t d, double def)
{
  if (d.d)
  {
    return d.x * d.d;
  }
  else
  {
    return d.x * def;
  }
}

struct backend_s;

/* structure to hold command line options */
struct info_s
{
  struct backend_s *backend;            /* type of backend (eps,ps,pgm etc) */
  potrace_param_t *param;               /* tracing parameters, see potracelib.h */
  int debug;                            /* type of output (0-2) (for BACKEND_PS/EPS only) */
  dim_t width_d;                        /* desired width of image */
  dim_t height_d;                       /* desired height of image */
  double rx;                            /* desired x resolution (in dpi) */
  double ry;                            /* desired y resolution (in dpi) */
  double sx;                            /* desired x scaling factor */
  double sy;                            /* desired y scaling factor */
  double stretch;                       /* ry/rx, if not otherwise determined */
  dim_t lmar_d, rmar_d, tmar_d, bmar_d; /* margins */
  double angle;                         /* rotate by this many degrees */
  int paperwidth, paperheight;          /* paper size for ps backend (in pt) */
  int tight;                            /* should bounding box follow actual vector outline? */
  double unit;                          /* granularity of output grid */
  int color;                            /* rgb color code 0xrrggbb: line color */
  int fillcolor;                        /* rgb color code 0xrrggbb: fill color */
  double gamma;                         /* gamma value for pgm backend */
  char *outfile;                        /* output filename, if given */
  char **infiles;                       /* array of input filenames */
  int infilecount;                      /* number of input filenames */
  int some_infiles;                     /* do we process a list of input filenames? */
  double blacklevel;                    /* 0 to 1: black/white cutoff in input file */
  int invert;                           /* invert bitmap? */
  bool invert_background;               /* default is white */
  int opaque;                           /* paint white shapes opaquely? */
  int grouping;                         /* 0=flat; 1=connected components; 2=hierarchical */
  // mkimg
  double lambda_high;                   /* highpass filter radius */
  double lambda_low;                    /* lowpass filter radius */
  int scale;      /* scaling factor */
  int linear;     /* linear scaling? */
  int bilevel;    /* convert to bilevel? */
  double level;   /* cutoff grey level */
  // debug
  int draw_dot;   /* draw dot for debug*/
};
typedef struct info_s info_t;

extern info_t info;

/* structure to hold per-image information, set e.g. by calc_dimensions */
struct imginfo_s
{
  int pixwidth;                  /* width of input pixmap */
  int pixheight;                 /* height of input pixmap */
  double width;                  /* desired width of image (in pt or pixels) */
  double height;                 /* desired height of image (in pt or pixels) */
  int channels;                  /* desired channels of image(1 = gray, 3 = rgb, 4 = rgba) */
  double lmar, rmar, tmar, bmar; /* requested margins (in pt) */
  bool invert;                   /* invert background */
  trans_t trans;                 /* specify relative position of a tilted rectangle */
};
typedef struct imginfo_s imginfo_t;

/* backends and their characteristics */
struct backend_s
{
  const char *name;          /* name of this backend */
  const char *ext;           /* file extension */
  int fixed;                 /* fixed page size backend? */
  int pixel;                 /* pixel-based backend? */
  int multi;                 /* multi-page backend? */
  int (*init_f)(void *fout); /* initialization function */
  int (*page_f)(void *fout, potrace_path_t *plist, imginfo_t *imginfo);
  /* per-bitmap function */
  int (*term_f)(void *fout); /* finalization function */
  int opticurve;             /* opticurve capable (true Bezier curves?) */
};
typedef struct backend_s backend_t;

int page_svg(void *out, potrace_path_t *plist, imginfo_t *imginfo);
int page_gimp(void *out, potrace_path_t *plist, imginfo_t *imginfo);
int page_pgm(void *out, potrace_path_t *plist, imginfo_t *imginfo);
int page_geojson(void *out, potrace_path_t *plist, imginfo_t *imginfo);
int page_dxf(void *out, potrace_path_t *plist, imginfo_t *imginfo);
int page_mem(void *out, potrace_path_t *plist, imginfo_t *imginfo);

void init_info();
int backend_lookup(const char *name, backend_t **bp);
int backend_list(FILE *fout, int j, int linelen);
void calc_dimensions(imginfo_t *imginfo, potrace_path_t *plist, int* width = nullptr, int* height = nullptr);

#endif /* BACKEND_H */
