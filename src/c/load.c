#define _GNU_SOURCE
#include "load.h"
#include "graph.h"
#include <pebble.h>
#include <string.h>

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
static char s_x0[8];
static char s_x1[8];

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

static void graph_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GraphStyle st;
  memset(&st, 0, sizeof(st));
  st.x0 = s_x0;
  st.x1 = s_x1;
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
    st.line = GColorGreen;
    st.has_fill = false;
    graph_draw_series(ctx, b, s_ctl, s_n, minv, maxv, &st);
    st.line = GColorOrange;
    graph_draw_series(ctx, b, s_atl, s_n, minv, maxv, &st);
  } else {
    int maxv = -100000;
    int minv = 100000;
    for (int i = 0; i < s_n; i++) {
      if (s_tsb_series[i] > maxv) maxv = s_tsb_series[i];
      if (s_tsb_series[i] < minv) minv = s_tsb_series[i];
    }
    if (maxv <= minv) maxv = minv + 1;
    st.line = form_color(s_tsb_now);
    st.has_fill = true;
    st.fill = GColorFromRGBA(70, 70, 70, 80);
    st.zero_line = true;
    graph_draw_series(ctx, b, s_tsb_series, s_n, minv, maxv, &st);
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

void load_show(int ctl, int atl, int tsb, char *series, const char *x0, const char *x1) {
  s_ctl_now = ctl;
  s_atl_now = atl;
  s_tsb_now = tsb;
  snprintf(s_x0, sizeof(s_x0), "%s", x0 ? x0 : "");
  snprintf(s_x1, sizeof(s_x1), "%s", x1 ? x1 : "");
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
