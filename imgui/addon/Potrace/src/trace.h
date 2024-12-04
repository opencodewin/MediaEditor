#ifndef TRACE_H
#define TRACE_H

#include "potracelib.h"
#include "progress.h"
#include "curve.h"

int process_path(path_t *plist, const potrace_param_t *param, progress_t *progress);

#endif /* TRACE_H */
