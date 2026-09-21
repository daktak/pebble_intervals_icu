#define _GNU_SOURCE
#include "trend.h"
#include "graph.h"
#include <pebble.h>
#include <string.h>

#define MAX_PTS 64
#define MODE_SLEEP 0
#define MODE_HRV 1
#define MODE_RHR 2
#define MODE_COUNT 3

static int s_series[MODE_COUNT][MAX_PTS];
static int s_n = 0;
static int s_mode = MODE_SLEEP;
static char s_x0[8];
static char s_x1[8];

static Window *s_window = NULL;
static TextLayer *s_info = NULL;
static Layer *s_graph = NULL;

static int *series_for(int mode) {
  return s_series[mode];
}

static const char *mode_name(int mode) {
  switch (mode) {
    case MODE_HRV: return "HRV";
    case MODE_RHR: return "Rest HR";
    case MODE_SLEEP:
    default: return "Sleep";
  }
}

static GColor mode_color(int mode) {
#ifdef PBL_COLOR
  switch (mode) {
    case MODE_HRV: return GColorCyan;
    case MODE_RHR: return GColorGreen;
    case MODE_SLEEP:
    default: return GColorPurple;
  }
#else
  return GColorBlack;
#endif
}

static GColor mode_fill(int mode) {
#ifdef PBL_COLOR
  switch (mode) {
    case MODE_HRV: return GColorFromRGBA(0, 170, 190, 80);
    case MODE_RHR: return GColorFromRGBA(0, 140, 60, 80);
    case MODE_SLEEP:
    default: return GColorFromRGBA(140, 60, 180, 80);
  }
#else
  return GColorLightGray;
#endif
}

static int mode_last(void) {
  int *s = series_for(s_mode);
  return s_n > 0 ? s[s_n - 1] : 0;
}

static void update_info(void) {
  if (!s_info) return;
  static char buf[64];
  snprintf(buf, sizeof(buf), "%s\nlast %d\nUP/DOWN: switch", mode_name(s_mode), mode_last());
  text_layer_set_text(s_info, buf);
}

static void graph_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int *s = series_for(s_mode);
  if (s_n < 2) {
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, "No data", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      GRect(0, b.size.h / 2 - 20, b.size.w, 40),
      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    return;
  }
  int minv, maxv;
  if (s_mode == MODE_SLEEP) {
    minv = 0;
    maxv = 100;
  } else {
    minv = 100000;
    maxv = -100000;
    for (int i = 0; i < s_n; i++) {
      if (s[i] > maxv) maxv = s[i];
      if (s[i] < minv) minv = s[i];
    }
  }
  if (maxv <= minv) maxv = minv + 1;
  GraphStyle st;
  memset(&st, 0, sizeof(st));
  st.line = mode_color(s_mode);
  st.has_fill = true;
  st.fill = mode_fill(s_mode);
  st.x0 = s_x0;
  st.x1 = s_x1;
  graph_draw_series(ctx, b, s, s_n, minv, maxv, &st);
}

static void next_mode(void) {
  s_mode = (s_mode + 1) % MODE_COUNT;
  update_info();
  if (s_graph) layer_mark_dirty(s_graph);
}

static void up_click(ClickRecognizerRef rec, void *ctx) {
  next_mode();
}

static void down_click(ClickRecognizerRef rec, void *ctx) {
  next_mode();
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
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
      if (strcmp(part, "rhr") == 0) target = s_series[MODE_RHR];
      else if (strcmp(part, "hrv") == 0) target = s_series[MODE_HRV];
      else if (strcmp(part, "sleep") == 0) target = s_series[MODE_SLEEP];
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

void trend_show(char *series, const char *x0, const char *x1) {
  s_mode = MODE_SLEEP;
  snprintf(s_x0, sizeof(s_x0), "%s", x0 ? x0 : "");
  snprintf(s_x1, sizeof(s_x1), "%s", x1 ? x1 : "");
  parse_series(series);

  if (s_window) window_destroy(s_window);
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}