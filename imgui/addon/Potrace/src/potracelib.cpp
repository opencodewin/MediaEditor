#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "potracelib.h"
#include "bitmap.h"
#include "backend.h"
#include "curve.h"
#include "decompose.h"
#include "trace.h"
#include "progress.h"

struct info_s info;

/* default parameters */
static const potrace_param_t param_default = {
    2,                           /* turdsize */
    POTRACE_TURNPOLICY_MINORITY, /* turnpolicy */
    1.0,                         /* alphamax */
    1,                           /* opticurve */
    0.2,                         /* opttolerance */
    {
        NULL,     /* callback function */
        NULL,     /* callback data */
        0.0, 1.0, /* progress range */
        0.0,      /* granularity */
    },
};

/* Return a fresh copy of the set of default parameters, or NULL on
   failure with errno set. */
potrace_param_t *potrace_param_default(void)
{
  potrace_param_t *p;

  p = (potrace_param_t *)malloc(sizeof(potrace_param_t));
  if (!p)
  {
    return NULL;
  }
  memcpy(p, &param_default, sizeof(potrace_param_t));
  return p;
}

/* On success, returns a Potrace state st with st->status ==
   POTRACE_STATUS_OK. On failure, returns NULL if no Potrace state
   could be created (with errno set), or returns an incomplete Potrace
   state (with st->status == POTRACE_STATUS_INCOMPLETE, and with errno
   set). Complete or incomplete Potrace state can be freed with
   potrace_state_free(). */
potrace_state_t *potrace_trace(const potrace_param_t *param, const potrace_bitmap_t *bm)
{
  int r;
  path_t *plist = NULL;
  potrace_state_t *st;
  progress_t prog;
  progress_t subprog;

  /* prepare private progress bar state */
  prog.callback = param->progress.callback;
  prog.data = param->progress.data;
  prog.min = param->progress.min;
  prog.max = param->progress.max;
  prog.epsilon = param->progress.epsilon;
  prog.d_prev = param->progress.min;

  /* allocate state object */
  st = (potrace_state_t *)malloc(sizeof(potrace_state_t));
  if (!st)
  {
    return NULL;
  }

  progress_subrange_start(0.0, 0.1, &prog, &subprog);

  /* process the image */
  r = bm_to_pathlist(bm, &plist, param, &subprog);
  if (r)
  {
    free(st);
    return NULL;
  }

  st->status = POTRACE_STATUS_OK;
  st->plist = plist;
  st->priv = NULL; /* private state currently unused */

  progress_subrange_end(&prog, &subprog);

  progress_subrange_start(0.1, 1.0, &prog, &subprog);

  /* partial success. */
  r = process_path(plist, param, &subprog);
  if (r)
  {
    st->status = POTRACE_STATUS_INCOMPLETE;
  }

  progress_subrange_end(&prog, &subprog);

  return st;
}

/* free a Potrace state, without disturbing errno. */
void potrace_state_free(potrace_state_t *st)
{
  pathlist_free(st->plist);
  free(st);
}

/* free a parameter list, without disturbing errno. */
void potrace_param_free(potrace_param_t *p)
{
  free(p);
}

const char *potrace_version(void)
{
  return "potracelib " VERSION "";
}

/* determine the dimensions of the output based on command line and
   image dimensions, and optionally, based on the actual image outline. */
void calc_dimensions(imginfo_t *imginfo, potrace_path_t *plist, int* width, int* height)
{
  double dim_def;
  double maxwidth, maxheight, sc;
  int default_scaling = 0;

  /* we take care of a special case: if one of the image dimensions is
     0, we change it to 1. Such an image is empty anyway, so there
     will be 0 paths in it. Changing the dimensions avoids division by
     0 error in calculating scaling factors, bounding boxes and
     such. This doesn't quite do the right thing in all cases, but it
     is better than causing overflow errors or "nan" output in
     backends.  Human users don't tend to process images of size 0
     anyway; they might occur in some pipelines. */
  if (imginfo->pixwidth == 0)
  {
    imginfo->pixwidth = 1;
  }
  if (imginfo->pixheight == 0)
  {
    imginfo->pixheight = 1;
  }

  /* set the default dimension for width, height, margins */
  if (info.backend->pixel)
  {
    dim_def = DIM_PT;
  }
  else
  {
    dim_def = DIM_MM;
    info.width_d.x = imginfo->pixwidth;
    info.height_d.x = imginfo->pixheight;
  }

  /* apply default dimension to width, height, margins */
  imginfo->width = info.width_d.x == UNDEF ? UNDEF : double_of_dim(info.width_d, dim_def);
  imginfo->height = info.height_d.x == UNDEF ? UNDEF : double_of_dim(info.height_d, dim_def);
  imginfo->lmar = info.lmar_d.x == UNDEF ? UNDEF : double_of_dim(info.lmar_d, dim_def);
  imginfo->rmar = info.rmar_d.x == UNDEF ? UNDEF : double_of_dim(info.rmar_d, dim_def);
  imginfo->tmar = info.tmar_d.x == UNDEF ? UNDEF : double_of_dim(info.tmar_d, dim_def);
  imginfo->bmar = info.bmar_d.x == UNDEF ? UNDEF : double_of_dim(info.bmar_d, dim_def);

  /* start with a standard rectangle */
  trans_from_rect(&imginfo->trans, imginfo->pixwidth, imginfo->pixheight);

  /* if info.tight is set, tighten the bounding box */
  if (info.tight)
  {
    trans_tighten(&imginfo->trans, plist);
  }

  /* sx/rx is just an alternate way to specify width; sy/ry is just an
     alternate way to specify height. */
  if (info.backend->pixel)
  {
    if (imginfo->width == UNDEF && info.sx != UNDEF)
    {
      imginfo->width = imginfo->trans.bb[0] * info.sx;
    }
    if (imginfo->height == UNDEF && info.sy != UNDEF)
    {
      imginfo->height = imginfo->trans.bb[1] * info.sy;
    }
  }
  else
  {
    if (imginfo->width == UNDEF && info.rx != UNDEF)
    {
      imginfo->width = imginfo->trans.bb[0] / info.rx * 72;
    }
    if (imginfo->height == UNDEF && info.ry != UNDEF)
    {
      imginfo->height = imginfo->trans.bb[1] / info.ry * 72;
    }
  }

  /* if one of width/height is specified, use stretch to determine the
     other */
  if (imginfo->width == UNDEF && imginfo->height != UNDEF)
  {
    imginfo->width = imginfo->height / imginfo->trans.bb[1] * imginfo->trans.bb[0] / info.stretch;
  }
  else if (imginfo->width != UNDEF && imginfo->height == UNDEF)
  {
    imginfo->height = imginfo->width / imginfo->trans.bb[0] * imginfo->trans.bb[1] * info.stretch;
  }

  /* if width and height are still variable, tenatively use the
     default scaling factor of 72dpi (for dimension-based backends) or
     1 (for pixel-based backends). For fixed-size backends, this will
     be adjusted later to fit the page. */
  if (imginfo->width == UNDEF && imginfo->height == UNDEF)
  {
    imginfo->width = imginfo->trans.bb[0];
    imginfo->height = imginfo->trans.bb[1] * info.stretch;
    default_scaling = 1;
  }

  /* apply scaling */
  trans_scale_to_size(&imginfo->trans, imginfo->width, imginfo->height);

  /* apply rotation, and tighten the bounding box again, if necessary */
  if (info.angle != 0.0)
  {
    trans_rotate(&imginfo->trans, info.angle);
    if (info.tight)
    {
      trans_tighten(&imginfo->trans, plist);
    }
  }

  /* for fixed-size backends, if default scaling was in effect,
     further adjust the scaling to be the "best fit" for the given
     page size and margins. */
  if (default_scaling && info.backend->fixed)
  {

    /* try to squeeze it between margins */
    maxwidth = UNDEF;
    maxheight = UNDEF;

    if (imginfo->lmar != UNDEF && imginfo->rmar != UNDEF)
    {
      maxwidth = info.paperwidth - imginfo->lmar - imginfo->rmar;
    }
    if (imginfo->bmar != UNDEF && imginfo->tmar != UNDEF)
    {
      maxheight = info.paperheight - imginfo->bmar - imginfo->tmar;
    }
    if (maxwidth == UNDEF && maxheight == UNDEF)
    {
      maxwidth = max(info.paperwidth - 144, info.paperwidth * 0.75);
      maxheight = max(info.paperheight - 144, info.paperheight * 0.75);
    }

    if (maxwidth == UNDEF)
    {
      sc = maxheight / imginfo->trans.bb[1];
    }
    else if (maxheight == UNDEF)
    {
      sc = maxwidth / imginfo->trans.bb[0];
    }
    else
    {
      sc = min(maxwidth / imginfo->trans.bb[0], maxheight / imginfo->trans.bb[1]);
    }

    /* re-scale coordinate system */
    imginfo->width *= sc;
    imginfo->height *= sc;
    trans_rescale(&imginfo->trans, sc);
  }

  /* adjust margins */
  if (info.backend->fixed)
  {
    if (imginfo->lmar == UNDEF && imginfo->rmar == UNDEF)
    {
      imginfo->lmar = (info.paperwidth - imginfo->trans.bb[0]) / 2;
    }
    else if (imginfo->lmar == UNDEF)
    {
      imginfo->lmar = (info.paperwidth - imginfo->trans.bb[0] - imginfo->rmar);
    }
    else if (imginfo->lmar != UNDEF && imginfo->rmar != UNDEF)
    {
      imginfo->lmar += (info.paperwidth - imginfo->trans.bb[0] - imginfo->lmar - imginfo->rmar) / 2;
    }
    if (imginfo->bmar == UNDEF && imginfo->tmar == UNDEF)
    {
      imginfo->bmar = (info.paperheight - imginfo->trans.bb[1]) / 2;
    }
    else if (imginfo->bmar == UNDEF)
    {
      imginfo->bmar = (info.paperheight - imginfo->trans.bb[1] - imginfo->tmar);
    }
    else if (imginfo->bmar != UNDEF && imginfo->tmar != UNDEF)
    {
      imginfo->bmar += (info.paperheight - imginfo->trans.bb[1] - imginfo->bmar - imginfo->tmar) / 2;
    }
  }
  else
  {
    if (imginfo->lmar == UNDEF)
    {
      imginfo->lmar = 0;
    }
    if (imginfo->rmar == UNDEF)
    {
      imginfo->rmar = 0;
    }
    if (imginfo->bmar == UNDEF)
    {
      imginfo->bmar = 0;
    }
    if (imginfo->tmar == UNDEF)
    {
      imginfo->tmar = 0;
    }
  }

  trans_t t;
  t.bb[0] = imginfo->trans.bb[0] + imginfo->lmar + imginfo->rmar;
  t.bb[1] = imginfo->trans.bb[1] + imginfo->tmar + imginfo->bmar;
  if (width) *width = (int)ceil(t.bb[0]);
  if (height) *height = (int)ceil(t.bb[1]);
}

/* scale greymap by factor s, using linear interpolation. If
   bilevel=0, return a pointer to a greymap_t. If bilevel=1, return a
   pointer to a potrace_bitmap_t and use cutoff threshold c (0=black,
   1=white).  On error, return NULL with errno set. */

void *interpolate_linear(greymap_t *gm, int s, int bilevel, double c)
{
  int p00, p01, p10, p11;
  int i, j, x, y;
  double xx, yy, av;
  double c1 = 0;
  int w, h;
  double p0, p1;
  greymap_t *gm_out = NULL;
  potrace_bitmap_t *bm_out = NULL;

  w = gm->w;
  h = gm->h;

  /* allocate output bitmap/greymap */
  if (bilevel)
  {
    bm_out = bm_new(w * s, h * s);
    if (!bm_out)
    {
      return NULL;
    }
    c1 = c * 255;
  }
  else
  {
    gm_out = gm_new(w * s, h * s);
    if (!gm_out)
    {
      return NULL;
    }
  }

  /* interpolate */
  for (i = 0; i < w; i++)
  {
    for (j = 0; j < h; j++)
    {
      p00 = GM_BGET(gm, i, j);
      p01 = GM_BGET(gm, i, j + 1);
      p10 = GM_BGET(gm, i + 1, j);
      p11 = GM_BGET(gm, i + 1, j + 1);

      if (bilevel)
      {
        /* treat two special cases which are very common */
        if (p00 < c1 && p01 < c1 && p10 < c1 && p11 < c1)
        {
          for (x = 0; x < s; x++)
          {
            for (y = 0; y < s; y++)
            {
              BM_UPUT(bm_out, i * s + x, j * s + y, 1);
            }
          }
          continue;
        }
        if (p00 >= c1 && p01 >= c1 && p10 >= c1 && p11 >= c1)
        {
          continue;
        }
      }

      /* the general case */
      for (x = 0; x < s; x++)
      {
        xx = x / (double)s;
        p0 = p00 * (1 - xx) + p10 * xx;
        p1 = p01 * (1 - xx) + p11 * xx;
        for (y = 0; y < s; y++)
        {
          yy = y / (double)s;
          av = p0 * (1 - yy) + p1 * yy;
          if (bilevel)
          {
            BM_UPUT(bm_out, i * s + x, j * s + y, av < c1);
          }
          else
          {
            GM_UPUT(gm_out, i * s + x, j * s + y, av);
          }
        }
      }
    }
  }
  if (bilevel)
  {
    return (void *)bm_out;
  }
  else
  {
    return (void *)gm_out;
  }
}

/* same as interpolate_linear, except use cubic interpolation (slower
   and better). */

#define SAFE_CALLOC(var, n, typ)                     \
  if ((var = (typ *)calloc(n, sizeof(typ))) == NULL) \
  goto calloc_error

/* we need this typedef so that the SAFE_CALLOC macro will work */
typedef double double4[4];

void *interpolate_cubic(greymap_t *gm, int s, int bilevel, double c)
{
  int w, h;
  double4 *poly = NULL;   /* poly[s][4]: fixed interpolation polynomials */
  double p[4];            /* four current points */
  double4 *window = NULL; /* window[s][4]: current state */
  double t, v;
  int k, l, i, j, x, y;
  double c1 = 0;
  greymap_t *gm_out = NULL;
  potrace_bitmap_t *bm_out = NULL;

  SAFE_CALLOC(poly, s, double4);
  SAFE_CALLOC(window, s, double4);

  w = gm->w;
  h = gm->h;

  /* allocate output bitmap/greymap */
  if (bilevel)
  {
    bm_out = bm_new(w * s, h * s);
    if (!bm_out)
    {
      goto calloc_error;
    }
    c1 = c * 255;
  }
  else
  {
    gm_out = gm_new(w * s, h * s);
    if (!gm_out)
    {
      goto calloc_error;
    }
  }

  /* pre-calculate interpolation polynomials */
  for (k = 0; k < s; k++)
  {
    t = k / (double)s;
    poly[k][0] = 0.5 * t * (t - 1) * (1 - t);
    poly[k][1] = -(t + 1) * (t - 1) * (1 - t) + 0.5 * (t - 1) * (t - 2) * t;
    poly[k][2] = 0.5 * (t + 1) * t * (1 - t) - t * (t - 2) * t;
    poly[k][3] = 0.5 * t * (t - 1) * t;
  }

  /* interpolate */
  for (y = 0; y < h; y++)
  {
    x = 0;
    for (i = 0; i < 4; i++)
    {
      for (j = 0; j < 4; j++)
      {
        p[j] = GM_BGET(gm, x + i - 1, y + j - 1);
      }
      for (k = 0; k < s; k++)
      {
        window[k][i] = 0.0;
        for (j = 0; j < 4; j++)
        {
          window[k][i] += poly[k][j] * p[j];
        }
      }
    }
    while (1)
    {
      for (l = 0; l < s; l++)
      {
        for (k = 0; k < s; k++)
        {
          v = 0.0;
          for (i = 0; i < 4; i++)
          {
            v += window[k][i] * poly[l][i];
          }
          if (bilevel)
          {
            BM_PUT(bm_out, x * s + l, y * s + k, v < c1);
          }
          else
          {
            GM_PUT(gm_out, x * s + l, y * s + k, v);
          }
        }
      }
      x++;
      if (x >= w)
      {
        break;
      }
      for (i = 0; i < 3; i++)
      {
        for (k = 0; k < s; k++)
        {
          window[k][i] = window[k][i + 1];
        }
      }
      i = 3;
      for (j = 0; j < 4; j++)
      {
        p[j] = GM_BGET(gm, x + i - 1, y + j - 1);
      }
      for (k = 0; k < s; k++)
      {
        window[k][i] = 0.0;
        for (j = 0; j < 4; j++)
        {
          window[k][i] += poly[k][j] * p[j];
        }
      }
    }
  }

  free(poly);
  free(window);

  if (bilevel)
  {
    return (void *)bm_out;
  }
  else
  {
    return (void *)gm_out;
  }

calloc_error:
  free(poly);
  free(window);
  return NULL;
}

void init_info()
{
  info.debug = 0;
  info.width_d.x = UNDEF;
  info.height_d.x = UNDEF;
  info.rx = UNDEF;
  info.ry = UNDEF;
  info.sx = UNDEF;
  info.sy = UNDEF;
  info.stretch = 1;
  info.lmar_d.x = UNDEF;
  info.rmar_d.x = UNDEF;
  info.tmar_d.x = UNDEF;
  info.bmar_d.x = UNDEF;
  info.angle = 0;
  info.paperwidth = DEFAULT_PAPERWIDTH;
  info.paperheight = DEFAULT_PAPERHEIGHT;
  info.tight = 0;
  info.unit = 10;
  info.color = 0x000000;
  info.gamma = 2.2;
  info.param = potrace_param_default();
  info.outfile = NULL;
  info.blacklevel = 0.5;
  info.invert = 0;
  info.opaque = 0;
  info.grouping = 1;
  info.fillcolor = 0xffffff;
  info.lambda_high = 0;                     /* highpass filter radius */
  info.lambda_low = 0;                      /* lowpass filter radius */
  info.scale = 2;                           /* scaling factor */
  info.linear = 0;                          /* linear scaling? */
  info.bilevel = 1;                         /* convert to bilevel? */
  info.level = 0.45;                        /* cutoff grey level */
  info.draw_dot = 0;
}

static backend_t backend[] = {
    {"svg", ".svg", 0, 0, 0, NULL, page_svg, NULL, 1},
    {"dxf", ".dxf", 0, 1, 0, NULL, page_dxf, NULL, 1},
    {"geojson", ".json", 0, 1, 0, NULL, page_geojson, NULL, 1},
    {"pgm", ".pgm", 0, 1, 1, NULL, page_pgm, NULL, 1},
    {"gimppath", ".svg", 0, 1, 0, NULL, page_gimp, NULL, 1},
    {"mem", ".mem", 0, 1, 1, NULL, page_mem, NULL, 1},
    {NULL, NULL, 0, 0, 0, NULL, NULL, NULL, 0},
};

/* look up a backend by name. If found, return 0 and set *bp. If not
   found leave *bp unchanged and return 1, or 2 on ambiguous
   prefix. */
int backend_lookup(const char *name, backend_t **bp)
{
  int i;
  int m = 0; /* prefix matches */
  backend_t *b = NULL;

  for (i = 0; backend[i].name; i++)
  {
    if (strcasecmp(backend[i].name, name) == 0)
    {
      *bp = &backend[i];
      return 0;
    }
    else if (strncasecmp(backend[i].name, name, strlen(name)) == 0)
    {
      m++;
      b = &backend[i];
    }
  }
  /* if there was no exact match, and exactly one prefix match, use that */
  if (m == 1)
  {
    *bp = b;
    return 0;
  }
  else if (m)
  {
    return 2;
  }
  else
  {
    return 1;
  }
}

/* list all available backends by name, in a comma separated list.
   Assume the cursor starts in column j, and break lines at length
   linelen. Do not output any trailing punctuation. Return the column
   the cursor is in. */
int backend_list(FILE *fout, int j, int linelen)
{
  int i;

  for (i = 0; backend[i].name; i++)
  {
    if (j + (int)strlen(backend[i].name) > linelen)
    {
      fprintf(fout, "\n");
      j = 0;
    }
    j += fprintf(fout, "%s", backend[i].name);
    if (backend[i + 1].name)
    {
      j += fprintf(fout, ", ");
    }
  }
  return j;
}
