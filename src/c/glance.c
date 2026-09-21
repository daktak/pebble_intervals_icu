#include "glance.h"
#include "comm.h"
#include <pebble.h>
#include <string.h>

#ifdef PBL_PLATFORM_EMERY
  #define GLANCE_FONT FONT_KEY_GOTHIC_24
  #define GLANCE_ROW_H 28
#else
  #define GLANCE_FONT FONT_KEY_GOTHIC_14
  #define GLANCE_ROW_H 18
#endif
#define GLANCE_PAD 6
#define GLANCE_MAX_ROWS 5

#define ST_IDLE 0
#define ST_LOADING 1
#define ST_DONE 2
#define ST_ERROR 3

static Window *s_window = NULL;
static Layer *s_layer = NULL;
static char s_l1[GLANCE_MAX_ROWS][8];
static char s_v1[GLANCE_MAX_ROWS][16];
static char s_l2[GLANCE_MAX_ROWS][8];
static char s_v2[GLANCE_MAX_ROWS][16];
static char s_status[40];
static GColor s_status_color;
static int s_state = ST_IDLE;
static int s_rows = 0;

static void copy_token(char *dst, int max, const char **pp) {
  int i = 0;
  while (**pp == ' ' || **pp == '\t') (*pp)++;
  while (**pp && **pp != ' ' && **pp != '\t' && **pp != '\n' && i < max - 1) {
    dst[i++] = **pp;
    (*pp)++;
  }
  dst[i] = '\0';
}

static void glance_layer_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int w = b.size.w;
  GFont font = fonts_get_system_font(GLANCE_FONT);
  if (s_state == ST_DONE) {
    graphics_context_set_text_color(ctx, GColorBlack);
    int y = GLANCE_PAD;
    int leftX = GLANCE_PAD;
    int rightW = w - GLANCE_PAD * 2;
    for (int i = 0; i < s_rows; i++) {
      char left[32];
      snprintf(left, sizeof(left), "%s %s", s_l1[i], s_v1[i]);
      char right[32];
      snprintf(right, sizeof(right), "%s %s", s_l2[i], s_v2[i]);
      GRect lr = GRect(leftX, y, w - leftX, GLANCE_ROW_H);
      graphics_draw_text(ctx, left, font, lr, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      GRect rr = GRect(GLANCE_PAD, y, rightW, GLANCE_ROW_H);
      graphics_draw_text(ctx, right, font, rr, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
      y += GLANCE_ROW_H;
    }
  } else {
    graphics_context_set_text_color(ctx, s_status_color);
    GRect r = GRect(GLANCE_PAD, GLANCE_PAD, w - GLANCE_PAD * 2, GLANCE_ROW_H * 2);
    graphics_draw_text(ctx, s_status, font, r, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  }
}

static void window_load(Window *window) {
  window_set_background_color(window, GColorWhite);
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  s_layer = layer_create(b);
  layer_set_update_proc(s_layer, glance_layer_update);
  layer_add_child(root, s_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_layer);
  s_layer = NULL;
  s_window = NULL;
}

void glance_show(Cmd cmd, const char *loading_msg) {
  if (s_window) window_destroy(s_window);
  for (int i = 0; i < GLANCE_MAX_ROWS; i++) {
    s_l1[i][0] = '\0';
    s_v1[i][0] = '\0';
    s_l2[i][0] = '\0';
    s_v2[i][0] = '\0';
  }
  s_rows = 0;
  snprintf(s_status, sizeof(s_status), "%s", loading_msg ? loading_msg : "Loading...");
  s_status_color = GColorBlack;
  s_state = ST_LOADING;

  comm_send_cmd(cmd);
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

void glance_set_data(const char *payload) {
  if (!payload) return;
  const char *p = payload;
  int row = 0;
  while (row < GLANCE_MAX_ROWS && *p) {
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
  s_rows = row;
  s_state = ST_DONE;
  if (s_layer) layer_mark_dirty(s_layer);
}

void glance_error(const char *msg) {
  s_state = ST_ERROR;
  snprintf(s_status, sizeof(s_status), "%s", msg ? msg : "Request failed");
  s_status_color = GColorRed;
  if (s_layer) layer_mark_dirty(s_layer);
}

bool glance_is_loading(void) {
  return s_state == ST_LOADING;
}