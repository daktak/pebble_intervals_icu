#ifndef GRAPH_H
#define GRAPH_H

#include <pebble.h>

typedef struct {
  bool has_fill;
  GColor line;
  GColor fill;
  const char *x0;
  const char *x1;
  bool zero_line;
} GraphStyle;

void graph_draw_series(GContext *ctx, GRect bounds, const int *series, int n,
                       int minv, int maxv, const GraphStyle *style);

#endif