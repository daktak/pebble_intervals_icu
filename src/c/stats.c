#define _GNU_SOURCE
#include "stats.h"
#include "comm.h"
#include <pebble.h>
#include <string.h>

#ifdef PBL_PLATFORM_EMERY
  #define STATS_FONT FONT_KEY_GOTHIC_24
  #define STATS_ROW_H 28
#else
  #define STATS_FONT FONT_KEY_GOTHIC_14
  #define STATS_ROW_H 18
#endif
#define STATS_PAD 6

#define ST_IDLE 0
#define ST_LOADING 1
#define ST_DONE 2
#define ST_ERROR 3

static Window *s_window = NULL;
static Layer *s_stats_layer = NULL;
static char s_l1[4][8];
static char s_v1[4][16];
static char s_l2[4][8];
static char s_v2[4][16];
static char s_status[40];
static GColor s_status_color;
static int s_stats_state = ST_IDLE;

static void copy_token(char *dst, int max, const char **pp) {
  int i = 0;
  while (**pp == ' ' || **pp == '\t') (*pp)++;
  while (**pp && **pp != ' ' && **pp != '\t' && **pp != '\n' && i < max - 1) {
    dst[i++] = **pp;
    (*pp)++;
  }
  dst[i] = '\0';
}

static void stats_layer_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int w = b.size.w;
  GFont font = fonts_get_system_font(STATS_FONT);
  if (s_stats_state == ST_DONE) {
    graphics_context_set_text_color(ctx, GColorBlack);
    int y = STATS_PAD;
    int leftX = STATS_PAD;
    int rightW = w - STATS_PAD * 2;
    for (int i = 0; i < 4; i++) {
      char left[32];
      snprintf(left, sizeof(left), "%s %s", s_l1[i], s_v1[i]);
      char right[32];
      snprintf(right, sizeof(right), "%s %s", s_l2[i], s_v2[i]);
      GRect lr = GRect(leftX, y, w - leftX, STATS_ROW_H);
      graphics_draw_text(ctx, left, font, lr, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      GRect rr = GRect(STATS_PAD, y, rightW, STATS_ROW_H);
      graphics_draw_text(ctx, right, font, rr, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
      y += STATS_ROW_H;
    }
  } else {
    graphics_context_set_text_color(ctx, s_status_color);
    GRect r = GRect(STATS_PAD, STATS_PAD, w - STATS_PAD * 2, STATS_ROW_H * 2);
    graphics_draw_text(ctx, s_status, font, r, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  }
}

static void window_load(Window *window) {
  window_set_background_color(window, GColorWhite);
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  s_stats_layer = layer_create(b);
  layer_set_update_proc(s_stats_layer, stats_layer_update);
  layer_add_child(root, s_stats_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_stats_layer);
  s_stats_layer = NULL;
  s_window = NULL;
}

void stats_show(void) {
  if (s_window) window_destroy(s_window);
  for (int i = 0; i < 4; i++) {
    s_l1[i][0] = '\0';
    s_v1[i][0] = '\0';
    s_l2[i][0] = '\0';
    s_v2[i][0] = '\0';
  }
  snprintf(s_status, sizeof(s_status), "Loading...");
  s_status_color = GColorBlack;
  s_stats_state = ST_LOADING;
  comm_send_cmd(CMD_STATS);
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

void stats_set_data(const char *payload) {
  if (!payload) return;
  const char *p = payload;
  int row = 0;
  while (row < 4 && *p) {
    char line[64];
    int li = 0;
    while (*p && *p != '\n' && li < (int)sizeof(line) - 1) line[li++] = *p++;
    line[li] = '\0';
    if (*p == '\n') p++;
    const char *q = line;
    copy_token(s_l1[row], sizeof(s_l1[row]), &q);
    copy_token(s_v1[row], sizeof(s_v1[row]), &q);
    copy_token(s_l2[row], sizeof(s_l2[row]), &q);
    copy_token(s_v2[row], sizeof(s_v2[row]), &q);
    row++;
  }
  s_stats_state = ST_DONE;
  if (s_stats_layer) layer_mark_dirty(s_stats_layer);
}

void stats_error(const char *msg) {
  s_stats_state = ST_ERROR;
  snprintf(s_status, sizeof(s_status), "%s", msg ? msg : "Stats failed");
  s_status_color = GColorRed;
  if (s_stats_layer) layer_mark_dirty(s_stats_layer);
}

bool stats_is_loading(void) {
  return s_stats_state == ST_LOADING;
}