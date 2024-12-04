#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "potracelib.h"
#include "backend.h"
#include "bitmap_io.h"

#include <sys/time.h>
static uint64_t get_current_time_usec()
{
#ifdef _WIN32
  LARGE_INTEGER freq;
  LARGE_INTEGER pc;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&pc);
  return pc.QuadPart * 1000000.0 / freq.QuadPart;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec);
#endif
}

#define UNDEF ((double)(1e30)) /* a value to represent "undefined" */
#define TRY(x) \
  if (x)       \
  goto try_error

/* ---------------------------------------------------------------------- */
/* some data structures for option processing */

struct pageformat_s
{
  const char *name;
  int w, h;
};
typedef struct pageformat_s pageformat_t;

/* dimensions of the various page formats, in postscript points */
static pageformat_t pageformat[] = {
    {"a4", 595, 842},
    {"a3", 842, 1191},
    {"a5", 421, 595},
    {"b5", 516, 729},
    {"letter", 612, 792},
    {"legal", 612, 1008},
    {"tabloid", 792, 1224},
    {"statement", 396, 612},
    {"executive", 540, 720},
    {"folio", 612, 936},
    {"quarto", 610, 780},
    {"10x14", 720, 1008},
    {NULL, 0, 0},
};

struct turnpolicy_s
{
  const char *name;
  int n;
};
typedef struct turnpolicy_s turnpolicy_t;

/* names of turn policies */
static turnpolicy_t turnpolicy[] = {
    {"black", POTRACE_TURNPOLICY_BLACK},
    {"white", POTRACE_TURNPOLICY_WHITE},
    {"left", POTRACE_TURNPOLICY_LEFT},
    {"right", POTRACE_TURNPOLICY_RIGHT},
    {"minority", POTRACE_TURNPOLICY_MINORITY},
    {"majority", POTRACE_TURNPOLICY_MAJORITY},
    {"random", POTRACE_TURNPOLICY_RANDOM},
    {NULL, 0},
};

/* ---------------------------------------------------------------------- */
/* some info functions */

static void license(FILE *f)
{
  fprintf(f,
          "This program is free software; you can redistribute it and/or modify\n"
          "it under the terms of the GNU General Public License as published by\n"
          "the Free Software Foundation; either version 2 of the License, or\n"
          "(at your option) any later version.\n"
          "\n"
          "This program is distributed in the hope that it will be useful,\n"
          "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
          "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
          "GNU General Public License for more details.\n"
          "\n"
          "You should have received a copy of the GNU General Public License\n"
          "along with this program; if not, write to the Free Software Foundation\n"
          "Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.\n");
}

static void show_defaults(FILE *f)
{
  fprintf(f, "Default unit: " DEFAULT_DIM_NAME "\n");
  fprintf(f, "Default page size: " DEFAULT_PAPERFORMAT "\n");
}

static void usage(FILE *f)
{
  int j;

  fprintf(f, "Usage: " POTRACE " [options] [filename...]\n");
  fprintf(f, "General options:\n");
  fprintf(f, " -h, --help                 - print this help message and exit\n");
  fprintf(f, " -v, --version              - print version info and exit\n");
  fprintf(f, " -l, --license              - print license info and exit\n");
  fprintf(f, "File selection:\n");
  fprintf(f, " <filename>                 - an input file\n");
  fprintf(f, " -o, --output <filename>    - write all output to this file\n");
  fprintf(f, " --                         - end of options; 0 or more input filenames follow\n");
  fprintf(f, "Backend selection:\n");
  fprintf(f, " -b, --backend <name>       - select backend by name\n");
  fprintf(f, " -b svg, -s, --svg          - SVG backend (scalable vector graphics)\n");
  fprintf(f, " -b pgm, -g, --pgm          - PGM backend (portable greymap)\n");
  fprintf(f, " -b dxf                     - DXF backend (drawing interchange format)\n");
  fprintf(f, " -b geojson                 - GeoJSON backend\n");
  fprintf(f, " -b gimppath                - Gimppath backend (GNU Gimp)\n");
  fprintf(f, "Algorithm options:\n");
  fprintf(f, " -z, --turnpolicy <policy>  - how to resolve ambiguities in path decomposition\n");
  fprintf(f, " -t, --turdsize <n>         - suppress speckles of up to this size (default 2)\n");
  fprintf(f, " -a, --alphamax <n>         - corner threshold parameter (default 1)\n");
  fprintf(f, " -n, --longcurve            - turn off curve optimization\n");
  fprintf(f, " -O, --opttolerance <n>     - curve optimization tolerance (default 0.2)\n");
  fprintf(f, " -u, --unit <n>             - quantize output to 1/unit pixels (default 10)\n");
  fprintf(f, " -d, --debug <n>            - produce debugging output of type n (n=1,2,3)\n");
  fprintf(f, "Scaling and placement options:\n");
  fprintf(f, " -P, --pagesize <format>    - page size (default is " DEFAULT_PAPERFORMAT ")\n");
  fprintf(f, " -W, --width <dim>          - width of output image\n");
  fprintf(f, " -H, --height <dim>         - height of output image\n");
  fprintf(f, " -r, --resolution <n>[x<n>] - resolution (in dpi) (dimension-based backends)\n");
  fprintf(f, " -x, --scale <n>[x<n>]      - scaling factor (pixel-based backends)\n");
  fprintf(f, " -S, --stretch <n>          - yresolution/xresolution\n");
  fprintf(f, " -A, --rotate <angle>       - rotate counterclockwise by angle\n");
  fprintf(f, " -M, --margin <dim>         - margin\n");
  fprintf(f, " -L, --leftmargin <dim>     - left margin\n");
  fprintf(f, " -R, --rightmargin <dim>    - right margin\n");
  fprintf(f, " -T, --topmargin <dim>      - top margin\n");
  fprintf(f, " -B, --bottommargin <dim>   - bottom margin\n");
  fprintf(f, " --tight                    - remove whitespace around the input image\n");
  fprintf(f, "Color options, supported by some backends:\n");
  fprintf(f, " -C, --color #rrggbb        - set foreground color (default black)\n");
  fprintf(f, " --fillcolor #rrggbb        - set fill color (default transparent)\n");
  fprintf(f, " --opaque                   - make white shapes opaque\n");
  fprintf(f, "SVG options:\n");
  fprintf(f, " --group                    - group related paths together\n");
  fprintf(f, " --flat                     - whole image as a single path\n");
  fprintf(f, "PGM options:\n");
  fprintf(f, " -G, --gamma <n>            - gamma value for anti-aliasing (default 2.2)\n");
  fprintf(f, "Frontend options:\n");
  fprintf(f, " -k, --blacklevel <n>       - black/white cutoff in input file (default 0.5)\n");
  fprintf(f, " -i, --invert               - invert bitmap\n");
  fprintf(f, "\n");
  fprintf(f, "Dimensions can have optional units, e.g. 6.5in, 15cm, 100pt.\n");
  fprintf(f, "Default is " DEFAULT_DIM_NAME " (or pixels for pgm, dxf, and gimppath backends).\n");
  fprintf(f, "Possible input file formats are: pnm (pbm, pgm, ppm), bmp.\n");
  j = fprintf(f, "Backends are: ");
  backend_list(f, j, 78);
  fprintf(f, ".\n");
}

/* ---------------------------------------------------------------------- */
/* auxiliary functions for parameter parsing */

/* parse a dimension of the kind "1.5in", "7cm", etc. Return result in
   postscript points (=1/72 in). If endptr!=NULL, store pointer to
   next character in *endptr in the manner of strtod(3). */
static dim_t parse_dimension(char *s, char **endptr)
{
  char *p;
  dim_t res;

  res.x = strtod(s, &p);
  res.d = 0;
  if (p != s)
  {
    if (!strncasecmp(p, "in", 2))
    {
      res.d = DIM_IN;
      p += 2;
    }
    else if (!strncasecmp(p, "cm", 2))
    {
      res.d = DIM_CM;
      p += 2;
    }
    else if (!strncasecmp(p, "mm", 2))
    {
      res.d = DIM_MM;
      p += 2;
    }
    else if (!strncasecmp(p, "pt", 2))
    {
      res.d = DIM_PT;
      p += 2;
    }
  }
  if (endptr != NULL)
  {
    *endptr = p;
  }
  return res;
}

/* parse a pair of dimensions, such as "8.5x11in", "30mmx4cm" */
static void parse_dimensions(char *s, char **endptr, dim_t *dxp, dim_t *dyp)
{
  char *p, *q;
  dim_t dx, dy;

  dx = parse_dimension(s, &p);
  if (p == s)
  {
    goto fail;
  }
  if (*p != 'x')
  {
    goto fail;
  }
  p++;
  dy = parse_dimension(p, &q);
  if (q == p)
  {
    goto fail;
  }
  if (dx.d && !dy.d)
  {
    dy.d = dx.d;
  }
  else if (!dx.d && dy.d)
  {
    dx.d = dy.d;
  }
  *dxp = dx;
  *dyp = dy;
  if (endptr != NULL)
  {
    *endptr = q;
  }
  return;

fail:
  dx.x = dx.d = dy.x = dy.d = 0;
  *dxp = dx;
  *dyp = dy;
  if (endptr != NULL)
  {
    *endptr = s;
  }
  return;
}

static int parse_color(char *s)
{
  int i, d;
  int col = 0;

  if (s[0] != '#' || strlen(s) != 7)
  {
    return -1;
  }
  for (i = 0; i < 6; i++)
  {
    d = s[6 - i];
    if (d >= '0' && d <= '9')
    {
      col |= (d - '0') << (4 * i);
    }
    else if (d >= 'a' && d <= 'f')
    {
      col |= (d - 'a' + 10) << (4 * i);
    }
    else if (d >= 'A' && d <= 'F')
    {
      col |= (d - 'A' + 10) << (4 * i);
    }
    else
    {
      return -1;
    }
  }
  return col;
}

/* codes for options that don't have short form */
enum
{
  OPT_TIGHT = 300,
  OPT_FILLCOLOR,
  OPT_OPAQUE,
  OPT_GROUP,
  OPT_FLAT,
};

static struct option longopts[] = {
    {"help", 0, 0, 'h'},
    {"version", 0, 0, 'v'},
    {"show-defaults", 0, 0, 'V'}, /* undocumented option for compatibility */
    {"license", 0, 0, 'l'},
    {"width", 1, 0, 'W'},
    {"height", 1, 0, 'H'},
    {"resolution", 1, 0, 'r'},
    {"scale", 1, 0, 'x'},
    {"stretch", 1, 0, 'S'},
    {"margin", 1, 0, 'M'},
    {"leftmargin", 1, 0, 'L'},
    {"rightmargin", 1, 0, 'R'},
    {"topmargin", 1, 0, 'T'},
    {"bottommargin", 1, 0, 'B'},
    {"tight", 0, 0, OPT_TIGHT},
    {"rotate", 1, 0, 'A'},
    {"pagesize", 1, 0, 'P'},
    {"turdsize", 1, 0, 't'},
    {"unit", 1, 0, 'u'},
    {"postscript", 0, 0, 'p'},
    {"svg", 0, 0, 's'},
    {"pgm", 0, 0, 'g'},
    {"backend", 1, 0, 'b'},
    {"debug", 1, 0, 'd'},
    {"color", 1, 0, 'C'},
    {"fillcolor", 1, 0, OPT_FILLCOLOR},
    {"turnpolicy", 1, 0, 'z'},
    {"gamma", 1, 0, 'G'},
    {"longcurve", 0, 0, 'n'},
    {"alphamax", 1, 0, 'a'},
    {"opttolerance", 1, 0, 'O'},
    {"output", 1, 0, 'o'},
    {"blacklevel", 1, 0, 'k'},
    {"invert", 0, 0, 'i'},
    {"opaque", 0, 0, OPT_OPAQUE},
    {"group", 0, 0, OPT_GROUP},
    {"flat", 0, 0, OPT_FLAT},

    {0, 0, 0, 0}};

/* ---------------------------------------------------------------------- */
/* calculations with bitmap dimensions, positioning etc */

/* ---------------------------------------------------------------------- */
/* auxiliary functions for file handling */

/* open a file for reading. Return stdin if filename is NULL or "-" */
static FILE *my_fopen_read(const char *filename)
{
  if (filename == NULL || strcmp(filename, "-") == 0)
  {
    return stdin;
  }
  return fopen(filename, "rb");
}

/* open a file for writing. Return stdout if filename is NULL or "-" */
static FILE *my_fopen_write(const char *filename)
{
  if (filename == NULL || strcmp(filename, "-") == 0)
  {
    return stdout;
  }
  return fopen(filename, "wb");
}

/* close a file, but do nothing is filename is NULL or "-" */
static void my_fclose(FILE *f, const char *filename)
{
  if (filename == NULL || strcmp(filename, "-") == 0)
  {
    return;
  }
  fclose(f);
}

/* make output filename from input filename. Return an allocated value. */
static char *make_outfilename(const char *infile, const char *ext)
{
  char *outfile;
  char *p;

  if (strcmp(infile, "-") == 0)
  {
    return strdup("-");
  }

  outfile = (char *)malloc(strlen(infile) + strlen(ext) + 5);
  if (!outfile)
  {
    return NULL;
  }
  strcpy(outfile, infile);
  p = strrchr(outfile, '.');
  if (p)
  {
    *p = 0;
  }
  strcat(outfile, ext);

  /* check that input and output filenames are different */
  if (strcmp(infile, outfile) == 0)
  {
    strcpy(outfile, infile);
    strcat(outfile, "-out");
  }

  return outfile;
}

static const char *shortopts = "hvVlW:H:r:x:S:M:L:R:T:B:A:P:t:u:sgb:d:C:z:G:na:O:o:k:i";

static void dopts(int ac, char *av[])
{
  int c;
  char *p;
  int i, j, r;
  dim_t dim, dimx, dimy;
  int matches, bestmatch;

  /* defaults */
  init_info();
  backend_lookup("svg", &info.backend);

  while ((c = getopt_long(ac, av, shortopts, longopts, NULL)) != -1)
  {
    switch (c)
    {
    case 'h':
      fprintf(stdout, "" POTRACE " " VERSION ". Transforms bitmaps into vector graphics.\n\n");
      usage(stdout);
      exit(0);
      break;
    case 'v':
    case 'V':
      fprintf(stdout, "" POTRACE " " VERSION ". Copyright (C) 2001-2019 Peter Selinger.\n");
      fprintf(stdout, "Library version: %s\n", potrace_version());
      show_defaults(stdout);
      exit(0);
      break;
    case 'l':
      fprintf(stdout, "" POTRACE " " VERSION ". Copyright (C) 2001-2019 Peter Selinger.\n\n");
      license(stdout);
      exit(0);
      break;
    case 'W':
      info.width_d = parse_dimension(optarg, &p);
      if (*p)
      {
        fprintf(stderr, "" POTRACE ": invalid dimension -- %s\n", optarg);
        exit(1);
      }
      break;
    case 'H':
      info.height_d = parse_dimension(optarg, &p);
      if (*p)
      {
        fprintf(stderr, "" POTRACE ": invalid dimension -- %s\n", optarg);
        exit(1);
      }
      break;
    case 'r':
      parse_dimensions(optarg, &p, &dimx, &dimy);
      if (*p == 0 && dimx.d == 0 && dimy.d == 0 && dimx.x != 0.0 && dimy.x != 0.0)
      {
        info.rx = dimx.x;
        info.ry = dimy.x;
        break;
      }
      dim = parse_dimension(optarg, &p);
      if (*p == 0 && dim.d == 0 && dim.x != 0.0)
      {
        info.rx = info.ry = dim.x;
        break;
      }
      fprintf(stderr, "" POTRACE ": invalid resolution -- %s\n", optarg);
      exit(1);
      break;
    case 'x':
      parse_dimensions(optarg, &p, &dimx, &dimy);
      if (*p == 0 && dimx.d == 0 && dimy.d == 0)
      {
        info.sx = dimx.x;
        info.sy = dimy.x;
        break;
      }
      dim = parse_dimension(optarg, &p);
      if (*p == 0 && dim.d == 0)
      {
        info.sx = info.sy = dim.x;
        break;
      }
      fprintf(stderr, "" POTRACE ": invalid scaling factor -- %s\n", optarg);
      exit(1);
      break;
    case 'S':
      info.stretch = atof(optarg);
      break;
    case 'M':
      info.lmar_d = parse_dimension(optarg, &p);
      if (*p)
      {
        fprintf(stderr, "" POTRACE ": invalid dimension -- %s\n", optarg);
        exit(1);
      }
      info.rmar_d = info.tmar_d = info.bmar_d = info.lmar_d;
      break;
    case 'L':
      info.lmar_d = parse_dimension(optarg, &p);
      if (*p)
      {
        fprintf(stderr, "" POTRACE ": invalid dimension -- %s\n", optarg);
        exit(1);
      }
      break;
    case 'R':
      info.rmar_d = parse_dimension(optarg, &p);
      if (*p)
      {
        fprintf(stderr, "" POTRACE ": invalid dimension -- %s\n", optarg);
        exit(1);
      }
      break;
    case 'T':
      info.tmar_d = parse_dimension(optarg, &p);
      if (*p)
      {
        fprintf(stderr, "" POTRACE ": invalid dimension -- %s\n", optarg);
        exit(1);
      }
      break;
    case 'B':
      info.bmar_d = parse_dimension(optarg, &p);
      if (*p)
      {
        fprintf(stderr, "" POTRACE ": invalid dimension -- %s\n", optarg);
        exit(1);
      }
      break;
    case OPT_TIGHT:
      info.tight = 1;
      break;
    case 'A':
      info.angle = strtod(optarg, &p);
      if (*p)
      {
        fprintf(stderr, "" POTRACE ": invalid angle -- %s\n", optarg);
        exit(1);
      }
      if (info.angle <= -180 || info.angle > 180)
      {
        info.angle -= 360 * ceil(info.angle / 360 - 0.5);
      }
      break;
    case 'P':
      matches = 0;
      bestmatch = 0;
      for (i = 0; pageformat[i].name != NULL; i++)
      {
        if (strcasecmp(pageformat[i].name, optarg) == 0)
        {
          matches = 1;
          bestmatch = i;
          break;
        }
        else if (strncasecmp(pageformat[i].name, optarg, strlen(optarg)) == 0)
        {
          /* don't allow partial match on "10x14" */
          if (optarg[0] != '1')
          {
            matches++;
            bestmatch = i;
          }
        }
      }
      if (matches == 1)
      {
        info.paperwidth = pageformat[bestmatch].w;
        info.paperheight = pageformat[bestmatch].h;
        break;
      }
      parse_dimensions(optarg, &p, &dimx, &dimy);
      if (*p == 0)
      {
        info.paperwidth = (int)round(double_of_dim(dimx, DEFAULT_DIM));
        info.paperheight = (int)round(double_of_dim(dimy, DEFAULT_DIM));
        break;
      }
      if (matches == 0)
      {
        fprintf(stderr, "" POTRACE ": unrecognized page format -- %s\n", optarg);
      }
      else
      {
        fprintf(stderr, "" POTRACE ": ambiguous page format -- %s\n", optarg);
      }
      j = fprintf(stderr, "Use one of: ");
      for (i = 0; pageformat[i].name != NULL; i++)
      {
        if (j + strlen(pageformat[i].name) > 75)
        {
          fprintf(stderr, "\n");
          j = 0;
        }
        j += fprintf(stderr, "%s, ", pageformat[i].name);
      }
      fprintf(stderr, "or specify <dim>x<dim>.\n");
      exit(1);
      break;
    case 't':
      info.param->turdsize = atoi(optarg);
      break;
    case 'u':
      info.unit = strtod(optarg, &p);
      if (*p)
      {
        fprintf(stderr, "" POTRACE ": invalid unit -- %s\n", optarg);
        exit(1);
      }
      break;
    case 's':
      backend_lookup("svg", &info.backend);
      break;
    case 'g':
      backend_lookup("pgm", &info.backend);
      break;
    case 'b':
      r = backend_lookup(optarg, &info.backend);
      if (r == 1 || r == 2)
      {
        if (r == 1)
        {
          fprintf(stderr, "" POTRACE ": unrecognized backend -- %s\n", optarg);
        }
        else
        {
          fprintf(stderr, "" POTRACE ": ambiguous backend -- %s\n", optarg);
        }
        j = fprintf(stderr, "Use one of: ");
        backend_list(stderr, j, 70);
        fprintf(stderr, ".\n");
        exit(1);
      }
      break;
    case 'd':
      info.debug = atoi(optarg);
      break;
    case 'C':
      info.color = parse_color(optarg);
      if (info.color == -1)
      {
        fprintf(stderr, "" POTRACE ": invalid color -- %s\n", optarg);
        exit(1);
      }
      break;
    case OPT_FILLCOLOR:
      info.fillcolor = parse_color(optarg);
      if (info.fillcolor == -1)
      {
        fprintf(stderr, "" POTRACE ": invalid color -- %s\n", optarg);
        exit(1);
      }
      info.opaque = 1;
      break;
    case 'z':
      matches = 0;
      bestmatch = 0;
      for (i = 0; turnpolicy[i].name != NULL; i++)
      {
        if (strcasecmp(turnpolicy[i].name, optarg) == 0)
        {
          matches = 1;
          bestmatch = i;
          break;
        }
        else if (strncasecmp(turnpolicy[i].name, optarg, strlen(optarg)) == 0)
        {
          matches++;
          bestmatch = i;
        }
      }
      if (matches == 1)
      {
        info.param->turnpolicy = turnpolicy[bestmatch].n;
        break;
      }
      if (matches == 0)
      {
        fprintf(stderr, "" POTRACE ": unrecognized turnpolicy -- %s\n", optarg);
      }
      else
      {
        fprintf(stderr, "" POTRACE ": ambiguous turnpolicy -- %s\n", optarg);
      }
      j = fprintf(stderr, "Use one of: ");
      for (i = 0; turnpolicy[i].name != NULL; i++)
      {
        if (j + strlen(turnpolicy[i].name) > 75)
        {
          fprintf(stderr, "\n");
          j = 0;
        }
        j += fprintf(stderr, "%s%s", turnpolicy[i].name, turnpolicy[i + 1].name ? ", " : "");
      }
      fprintf(stderr, ".\n");
      exit(1);
      break;
    case 'G':
      info.gamma = atof(optarg);
      break;
    case 'n':
      info.param->opticurve = 0;
      break;
    case 'a':
      info.param->alphamax = strtod(optarg, &p);
      if (*p)
      {
        fprintf(stderr, "" POTRACE ": invalid alphamax -- %s\n", optarg);
        exit(1);
      }
      break;
    case 'O':
      info.param->opttolerance = strtod(optarg, &p);
      if (*p)
      {
        fprintf(stderr, "" POTRACE ": invalid opttolerance -- %s\n", optarg);
        exit(1);
      }
      break;
    case 'o':
      free(info.outfile);
      info.outfile = strdup(optarg);
      if (!info.outfile)
      {
        fprintf(stderr, "" POTRACE ": %s\n", strerror(errno));
        exit(2);
      }
      break;
    case 'k':
      info.blacklevel = strtod(optarg, &p);
      if (*p)
      {
        fprintf(stderr, "" POTRACE ": invalid blacklevel -- %s\n", optarg);
        exit(1);
      }
      break;
    case 'i':
      info.invert = 1;
      break;
    case OPT_OPAQUE:
      info.opaque = 1;
      break;
    case OPT_GROUP:
      info.grouping = 2;
      break;
    case OPT_FLAT:
      info.grouping = 0;
      break;
    case '?':
      fprintf(stderr, "Try --help for more info\n");
      exit(1);
      break;
    default:
      fprintf(stderr, "" POTRACE ": Unimplemented option -- %c\n", c);
      exit(1);
    }
  }
  info.infiles = &av[optind];
  info.infilecount = ac - optind;
  info.some_infiles = info.infilecount ? 1 : 0;

  /* if "--" was used, even an empty list of filenames is considered
     "some" filenames. */
  if (strcmp(av[optind - 1], "--") == 0)
  {
    info.some_infiles = 1;
  }
}

/* Process one or more bitmaps from fin, and write the results to fout
   using the page_f function of the appropriate backend. */

static void process_file(backend_t *b, const char *infile, const char *outfile, FILE *fin, FILE *fout)
{
  int r;
  potrace_bitmap_t *bm = NULL;
  imginfo_t imginfo;
  int eof_flag = 0; /* to indicate premature eof */
  int count;        /* number of bitmaps successfully processed, this file */
  potrace_state_t *st;

  //for (count = 0;; count++)
  {
    /* read a bitmap */
    r = bm_read(fin, info.blacklevel, &bm);
    switch (r)
    {
    case -1: /* system error */
      fprintf(stderr, "" POTRACE ": %s: %s\n", infile, strerror(errno));
      exit(2);
    case -2: /* corrupt file format */
      exit(2);
    case -3: /* empty file */
      if (count > 0)
      { /* end of file */
        return;
      }
      fprintf(stderr, "" POTRACE ": %s: empty file\n", infile);
      exit(2);
    case -4: /* wrong magic */
      if (count > 0)
      {
        fprintf(stderr, "" POTRACE ": %s: warning: junk at end of file\n", infile);
        return;
      }
      fprintf(stderr, "" POTRACE ": %s: file format not recognized\n", infile);
      fprintf(stderr, "Possible input file formats are: pnm (pbm, pgm, ppm), bmp.\n");
      exit(2);
    case 1: /* unexpected end of file */
      fprintf(stderr, "" POTRACE ": warning: %s: premature end of file\n", infile);
      eof_flag = 1;
      break;
    }

    /* prepare progress bar, if requested */
    info.param->progress.callback = NULL;

    if (info.invert)
    {
      bm_invert(bm);
    }
    int64_t t0 = get_current_time_usec();
    /* process the image */
    st = potrace_trace(info.param, bm);
    if (!st || st->status != POTRACE_STATUS_OK)
    {
      fprintf(stderr, "" POTRACE ": %s: %s\n", infile, strerror(errno));
      exit(2);
    }
    int64_t t1 = get_current_time_usec();

    /* calculate image dimensions */
    imginfo.pixwidth = bm->w;
    imginfo.pixheight = bm->h;
    imginfo.channels = 1;
    bm_free(bm);

    int64_t t2 = get_current_time_usec();
    calc_dimensions(&imginfo, st->plist);

    r = b->page_f(fout, st->plist, &imginfo);
    if (r)
    {
      fprintf(stderr, "" POTRACE ": %s: %s\n", outfile, strerror(errno));
      exit(2);
    }
    int64_t t3 = get_current_time_usec();

    potrace_state_free(st);

    fprintf(stderr, "" POTRACE ": tracetime:%fms: save time:%fms\n", (t1 - t0) / 1000.0, (t3 - t2) / 1000.0);

    if (eof_flag || !b->multi)
    {
      return;
    }
  }
  /* not reached */
}

int main(int ac, char *av[])
{
  backend_t *b; /* backend info */
  FILE *fin, *fout;
  int i;
  char *outfile;

  /* process options */
  dopts(ac, av);

  b = info.backend;
  if (b == NULL)
  {
    fprintf(stderr, "" POTRACE ": internal error: selected backend not found\n");
    exit(1);
  }

  /* fix some parameters */
  /* if backend cannot handle opticurve, disable it */
  if (b->opticurve == 0)
  {
    info.param->opticurve = 0;
  }

  /* there are several ways to call us:
     potrace                     -- stdin to stdout
     potrace -o outfile          -- stdin to outfile
     potrace file...             -- encode each file and generate outfile names
     potrace -o outfile file...  -- concatenate files and write to outfile

     The latter form is only allowed one file for single-page
     backends.  For multi-page backends, each file must contain 0 or
     more complete bitmaps.
  */

  if (!info.some_infiles)
  { /* read from stdin */

    fout = my_fopen_write(info.outfile);
    if (!fout)
    {
      fprintf(stderr, "" POTRACE ": %s: %s\n", info.outfile ? info.outfile : "stdout", strerror(errno));
      exit(2);
    }
    if (b->init_f)
    {
      TRY(b->init_f(fout));
    }
    process_file(b, "stdin", info.outfile ? info.outfile : "stdout", stdin, fout);
    if (b->term_f)
    {
      TRY(b->term_f(fout));
    }
    my_fclose(fout, info.outfile);
    free(info.outfile);
    potrace_param_free(info.param);
    return 0;
  }
  else if (!info.outfile)
  { /* infiles -> multiple outfiles */

    for (i = 0; i < info.infilecount; i++)
    {
      outfile = make_outfilename(info.infiles[i], b->ext);
      if (!outfile)
      {
        fprintf(stderr, "" POTRACE ": %s\n", strerror(errno));
        exit(2);
      }
      fin = my_fopen_read(info.infiles[i]);
      if (!fin)
      {
        fprintf(stderr, "" POTRACE ": %s: %s\n", info.infiles[i], strerror(errno));
        exit(2);
      }
      fout = my_fopen_write(outfile);
      if (!fout)
      {
        fprintf(stderr, "" POTRACE ": %s: %s\n", outfile, strerror(errno));
        exit(2);
      }
      if (b->init_f)
      {
        TRY(b->init_f(fout));
      }
      process_file(b, info.infiles[i], outfile, fin, fout);
      if (b->term_f)
      {
        TRY(b->term_f(fout));
      }
      my_fclose(fin, info.infiles[i]);
      my_fclose(fout, outfile);
      free(outfile);
    }
    potrace_param_free(info.param);
    return 0;
  }
  else
  { /* infiles to single outfile */

    if (!b->multi && info.infilecount >= 2)
    {
      fprintf(stderr, "" POTRACE ": cannot use multiple input files with -o in %s mode\n", b->name);
      exit(1);
    }
    if (info.infilecount == 0)
    {
      fprintf(stderr, "" POTRACE ": cannot use empty list of input files with -o\n");
      exit(1);
    }

    fout = my_fopen_write(info.outfile);
    if (!fout)
    {
      fprintf(stderr, "" POTRACE ": %s: %s\n", info.outfile, strerror(errno));
      exit(2);
    }
    if (b->init_f)
    {
      TRY(b->init_f(fout));
    }
    for (i = 0; i < info.infilecount; i++)
    {
      fin = my_fopen_read(info.infiles[i]);
      if (!fin)
      {
        fprintf(stderr, "" POTRACE ": %s: %s\n", info.infiles[i], strerror(errno));
        exit(2);
      }
      process_file(b, info.infiles[i], info.outfile, fin, fout);
      my_fclose(fin, info.infiles[i]);
    }
    if (b->term_f)
    {
      TRY(b->term_f(fout));
    }
    my_fclose(fout, info.outfile);
    free(info.outfile);
    potrace_param_free(info.param);
    return 0;
  }

  /* not reached */

try_error:
  fprintf(stderr, "" POTRACE ": %s\n", strerror(errno));
  exit(2);
}
