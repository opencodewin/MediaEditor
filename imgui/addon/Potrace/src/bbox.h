#ifndef BBOX_H
#define BBOX_H

#include "potracelib.h"

/* an interval [min, max] */
struct interval_s
{
  double min, max;
};
typedef struct interval_s interval_t;

void path_limits(potrace_path_t *path, potrace_dpoint_t dir, interval_t *i);

#endif /* BBOX_H */
