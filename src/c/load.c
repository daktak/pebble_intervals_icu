#define _GNU_SOURCE
#include "load.h"
#include <pebble.h>

#define MAX_PTS 64

static int s_ctl[MAX_PTS];
static int s_atl[MAX_PTS];
static int s_n = 0;
static int s_ctl_now = 0;
static int s_atl_now = 0;
static int s_tsb_now = 0;

static Window *s_window = NULL;
static TextLayer *s_info = NULL;
static Layer *s_graph = NULL;

static void draw_series(Layer *layer, GContext *ctx, int *series, int n, int minv, int maxv, GColor color) {
  GRect b = layer_get_bounds(layer);
  if (n < 2) return;
  int pad = 4;
  int w = b.size.w - pad * 2;
  int h = b.size.h - pad * 2;
  int prev_x = 0, prev_y = 0;
  for (int i = 0; i < n; i++) {
    int x = pad + (w * i) / (n - 1);
    int y = pad + h - (h * (series[i] - minv)) / (maxv - minv);
    if (i > 0) {
      graphics_context_set_stroke_color(ctx, color);
      graphics_draw_line(ctx, GPoint(prev_x, prev_y), GPoint(x, y));
    }
    prev_x = x;
    prev_y = y;
  }
}

static void graph_update(Layer *layer, GContext *ctx) {
  int maxv = 0;
  int minv = 100000;
  for (int i = 0; i < s_n; i++) {
    if (s_ctl[i] > maxv) maxv = s_ctl[i];
    if (s_atl[i] > maxv) maxv = s_atl[i];
    if (s_ctl[i] < minv) minv = s_ctl[i];
    if (s_atl[i] < minv) minv = s_atl[i];
  }
  if (maxv <= minv) maxv = minv + 1;
  draw_series(layer, ctx, s_ctl, s_n, minv, maxv, GColorGreen);
  draw_series(layer, ctx, s_atl, s_n, minv, maxv, GColorOrange);
}

static void parse_series(char *series) {
  s_n = 0;
  static char buf[512];
  snprintf(buf, sizeof(buf), "%s", series);
  char *save;
  char *part = strtok_r(buf, ";", &save);
  while (part) {
    char *colon = strchr(part, ':');
    if (colon) {
      *colon = '\0';
      char *vals = colon + 1;
      int *target = NULL;
      if (strcmp(part, "ctl") == 0) target = s_ctl;
      else if (strcmp(part, "atl") == 0) target = s_atl;
      if (target) {
        int idx = 0;
        char *save2;
        char *v = strtok_r(vals, ",", &save2);
        while (v && idx < MAX_PTS) {
          target[idx++] = atoi(v);
          v = strtok_r(NULL, ",", &save2);
        }
        if (idx > s_n) s_n = idx;
      }
    }
    part = strtok_r(NULL, ";", &save);
  }
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_info = text_layer_create(GRect(2, 4, b.size.w - 4, 72));
  text_layer_set_font(s_info, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_info, GTextAlignmentLeft);
  static char info_buf[48];
  snprintf(info_buf, sizeof(info_buf), "Fit %d\nLoad %d\nForm %+d", s_ctl_now, s_atl_now, s_tsb_now);
  text_layer_set_text(s_info, info_buf);
  layer_add_child(root, text_layer_get_layer(s_info));

  s_graph = layer_create(GRect(0, 56, b.size.w, b.size.h - 56));
  layer_set_update_proc(s_graph, graph_update);
  layer_add_child(root, s_graph);
}

static void window_unload(Window *window) {
  text_layer_destroy(s_info);
  layer_destroy(s_graph);
}

void load_show(int ctl, int atl, int tsb, char *series) {
  s_ctl_now = ctl;
  s_atl_now = atl;
  s_tsb_now = tsb;
  parse_series(series);

  if (s_window) window_destroy(s_window);
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
