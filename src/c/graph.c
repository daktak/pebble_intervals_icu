#include "graph.h"
#include <pebble.h>
#include <string.h>

#define LABEL_FONT FONT_KEY_GOTHIC_14
#define LEFT_MARGIN 26
#define TOP_MARGIN 9
#define BOTTOM_MARGIN 24
#define RIGHT_MARGIN 2

static GColor line_color_of(GColor c) {
  return PBL_IF_COLOR_ELSE(c, GColorBlack);
}

static int pixel_x(GRect plot, int n, int i) {
  return plot.origin.x + (plot.size.w * i) / (n - 1);
}

static int pixel_y(GRect plot, int v, int minv, int maxv) {
  if (maxv <= minv) maxv = minv + 1;
  int h = plot.size.h;
  return plot.origin.y + h - (h * (v - minv)) / (maxv - minv);
}

static void draw_thick_line(GContext *ctx, GRect plot, const int *series, int n,
                            int minv, int maxv, GColor color) {
  if (n < 2) return;
  static const GPoint offs[3] = { {0, 0}, {1, 0}, {0, 1} };
  graphics_context_set_stroke_color(ctx, line_color_of(color));
  for (int o = 0; o < 3; o++) {
    GPoint prev = GPointZero;
    for (int i = 0; i < n; i++) {
      GPoint p = GPoint(pixel_x(plot, n, i) + offs[o].x,
                        pixel_y(plot, series[i], minv, maxv) + offs[o].y);
      if (i > 0) graphics_draw_line(ctx, prev, p);
      prev = p;
    }
  }
}

static void fill_area(GContext *ctx, GRect plot, const int *series, int n,
                      int minv, int maxv, GColor fill, bool to_zero) {
  if (n < 2) return;
  GColor fc = PBL_IF_COLOR_ELSE(fill, GColorLightGray);
  graphics_context_set_fill_color(ctx, fc);
  int base = plot.origin.y + plot.size.h;
  if (to_zero) {
    base = pixel_y(plot, 0, minv, maxv);
    if (base < plot.origin.y) base = plot.origin.y;
    if (base > plot.origin.y + plot.size.h) base = plot.origin.y + plot.size.h;
  }
  for (int x = plot.origin.x; x < plot.origin.x + plot.size.w; x++) {
    double t = (double)(x - plot.origin.x) / (double)(plot.size.w - 1);
    double fpos = t * (n - 1);
    int i0 = (int)fpos;
    int i1 = i0 + 1;
    if (i0 >= n - 1) i0 = n - 1;
    if (i1 > n - 1) i1 = n - 1;
    double frac = fpos - i0;
    int v = (int)(series[i0] + (series[i1] - series[i0]) * frac);
    int yv = pixel_y(plot, v, minv, maxv);
    int top = yv < base ? yv : base;
    int bot = yv < base ? base : yv;
    if (top < plot.origin.y) top = plot.origin.y;
    if (bot > plot.origin.y + plot.size.h) bot = plot.origin.y + plot.size.h;
    int hgt = bot - top + 1;
    if (hgt >= 1) graphics_fill_rect(ctx, GRect(x, top, 1, hgt), 0, GCornerNone);
  }
}

static void draw_grid(GContext *ctx, GRect plot, int minv, int maxv) {
  int mid = minv + (maxv - minv) / 2;
  int vals[3] = { maxv, mid, minv };
  char lab[3][6];
  for (int k = 0; k < 3; k++) {
    int y = pixel_y(plot, vals[k], minv, maxv);
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_draw_line(ctx, GPoint(plot.origin.x, y), GPoint(plot.origin.x + plot.size.w - 1, y));
    snprintf(lab[k], sizeof(lab[k]), "%d", vals[k]);
    GRect r = GRect(0, y - 6, plot.origin.x - 2, 12);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, lab[k], fonts_get_system_font(LABEL_FONT), r,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  }
}

static void draw_zero_line(GContext *ctx, GRect plot, int minv, int maxv) {
  int y = pixel_y(plot, 0, minv, maxv);
  if (y < plot.origin.y || y > plot.origin.y + plot.size.h) return;
  graphics_context_set_stroke_color(ctx, GColorBlack);
  int x = plot.origin.x;
  while (x < plot.origin.x + plot.size.w) {
    int len = 3;
    if (x + len > plot.origin.x + plot.size.w) len = plot.origin.x + plot.size.w - x;
    graphics_draw_line(ctx, GPoint(x, y), GPoint(x + len - 1, y));
    x += 6;
  }
}

static void draw_xaxis(GContext *ctx, GRect plot, const char *x0, const char *x1) {
  int y = plot.origin.y + plot.size.h;
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(plot.origin.x, y), GPoint(plot.origin.x + plot.size.w - 1, y));
  if (x0 && x0[0]) {
    graphics_context_set_text_color(ctx, GColorBlack);
    GRect r = GRect(plot.origin.x - 2, y + 2, 70, 12);
    graphics_draw_text(ctx, x0, fonts_get_system_font(LABEL_FONT), r,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
  if (x1 && x1[0]) {
    graphics_context_set_text_color(ctx, GColorBlack);
    GRect r = GRect(plot.origin.x + plot.size.w - 70, y + 2, 70, 12);
    graphics_draw_text(ctx, x1, fonts_get_system_font(LABEL_FONT), r,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  }
}

void graph_draw_series(GContext *ctx, GRect bounds, const int *series, int n,
                       int minv, int maxv, const GraphStyle *style) {
  if (n < 2) return;
  GRect plot = GRect(bounds.origin.x + LEFT_MARGIN,
                     bounds.origin.y + TOP_MARGIN,
                     bounds.size.w - LEFT_MARGIN - RIGHT_MARGIN,
                     bounds.size.h - TOP_MARGIN - BOTTOM_MARGIN);
  if (plot.size.w < 4 || plot.size.h < 4) return;

  if (style && style->has_fill) {
    fill_area(ctx, plot, series, n, minv, maxv, style->fill, style->zero_line);
  }
  draw_grid(ctx, plot, minv, maxv);
  if (style && style->zero_line) {
    draw_zero_line(ctx, plot, minv, maxv);
  }
  draw_thick_line(ctx, plot, series, n, minv, maxv, style ? style->line : GColorBlack);
  draw_xaxis(ctx, plot, style ? style->x0 : NULL, style ? style->x1 : NULL);
}