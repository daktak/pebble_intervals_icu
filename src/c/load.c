#define _GNU_SOURCE
#include "load.h"
#include <pebble.h>

#define MAX_PTS 64
#define MODE_FITNESS 0
#define MODE_FORM 1

static int s_ctl[MAX_PTS];
static int s_atl[MAX_PTS];
static int s_tsb_series[MAX_PTS];
static int s_n = 0;
static int s_ctl_now = 0;
static int s_atl_now = 0;
static int s_tsb_now = 0;
static int s_mode = MODE_FITNESS;

static Window *s_window = NULL;
static TextLayer *s_info = NULL;
static Layer *s_graph = NULL;

static const char *form_zone(int tsb) {
  if (tsb <= -30) return "High Risk";
  if (tsb <= -15) return "Transition";
  if (tsb <= 0) return "Optimal";
  if (tsb <= 15) return "Fresh";
  return "Grey Zone";
}

static GColor form_color(int tsb) {
  if (tsb <= -30) return GColorRed;
  if (tsb <= -15) return GColorOrange;
  if (tsb <= 0) return GColorGreen;
  if (tsb <= 15) return GColorCyan;
  return GColorLightGray;
}

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
  if (s_mode == MODE_FITNESS) {
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
  } else {
    int maxv = -100000;
    int minv = 100000;
    for (int i = 0; i < s_n; i++) {
      if (s_tsb_series[i] > maxv) maxv = s_tsb_series[i];
      if (s_tsb_series[i] < minv) minv = s_tsb_series[i];
    }
    if (maxv <= minv) maxv = minv + 1;
    GRect b = layer_get_bounds(layer);
    int pad = 4;
    int h = b.size.h - pad * 2;
    int zeroy = pad + h - (h * (0 - minv)) / (maxv - minv);
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_draw_line(ctx, GPoint(pad, zeroy), GPoint(b.size.w - pad, zeroy));
    draw_series(layer, ctx, s_tsb_series, s_n, minv, maxv, form_color(s_tsb_now));
  }
}

static void update_info(void) {
  if (!s_info) return;
  static char buf[64];
  if (s_mode == MODE_FITNESS) {
    snprintf(buf, sizeof(buf), "Fit %d\nFatigue %d\nForm %+d\nDOWN: form", s_ctl_now, s_atl_now, s_tsb_now);
  } else {
    snprintf(buf, sizeof(buf), "Form %+d\n%s\nUP: fitness", s_tsb_now, form_zone(s_tsb_now));
  }
  text_layer_set_text(s_info, buf);
}

static void parse_series(char *series) {
  s_n = 0;
  static char buf[1024];
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
      else if (strcmp(part, "tsb") == 0) target = s_tsb_series;
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
  if (s_n > MAX_PTS) s_n = MAX_PTS;
}

static void down_click(ClickRecognizerRef rec, void *ctx) {
  if (s_mode == MODE_FITNESS) {
    s_mode = MODE_FORM;
    update_info();
    if (s_graph) layer_mark_dirty(s_graph);
  }
}

static void up_click(ClickRecognizerRef rec, void *ctx) {
  if (s_mode == MODE_FORM) {
    s_mode = MODE_FITNESS;
    update_info();
    if (s_graph) layer_mark_dirty(s_graph);
  }
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_info = text_layer_create(GRect(2, 4, b.size.w - 4, 80));
  text_layer_set_font(s_info, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_info, GTextAlignmentLeft);
  layer_add_child(root, text_layer_get_layer(s_info));
  update_info();

  s_graph = layer_create(GRect(0, 84, b.size.w, b.size.h - 84));
  layer_set_update_proc(s_graph, graph_update);
  layer_add_child(root, s_graph);

  window_set_click_config_provider(s_window, click_config);
}

static void window_unload(Window *window) {
  text_layer_destroy(s_info);
  s_info = NULL;
  layer_destroy(s_graph);
  s_graph = NULL;
  s_window = NULL;
}

void load_show(int ctl, int atl, int tsb, char *series) {
  s_ctl_now = ctl;
  s_atl_now = atl;
  s_tsb_now = tsb;
  s_mode = MODE_FITNESS;
  parse_series(series);

  if (s_window) window_destroy(s_window);
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
