#define _GNU_SOURCE
#include "activities.h"
#include <pebble.h>

#define MAX_ACT 32
#define NAME_LEN 48

static char s_dates[MAX_ACT][12];
static char s_types[MAX_ACT][16];
static char s_names[MAX_ACT][NAME_LEN];
static int s_loads[MAX_ACT];
static int s_count = 0;

static Window *s_window = NULL;
static MenuLayer *s_menu = NULL;
static Window *s_detail = NULL;

static uint16_t get_sections(MenuLayer *m, void *ctx) { return 1; }
static uint16_t get_rows(MenuLayer *m, uint16_t section, void *ctx) { return s_count; }

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *i, void *data) {
  static char sub[32];
  snprintf(sub, sizeof(sub), "L%d  %s", s_loads[i->row], s_types[i->row]);
  menu_cell_basic_draw(ctx, cell, s_names[i->row], sub, NULL);
}

static void detail_load(Window *window) {
  char *buf = (char *)window_get_user_data(window);
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  TextLayer *tl = text_layer_create(GRect(5, 5, b.size.w - 10, b.size.h - 10));
  text_layer_set_font(tl, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(tl, buf);
  text_layer_set_text_alignment(tl, GTextAlignmentCenter);
  text_layer_set_overflow_mode(tl, GTextOverflowModeWordWrap);
  window_set_user_data(window, tl);
  layer_add_child(root, text_layer_get_layer(tl));
}

static void detail_unload(Window *window) {
  TextLayer *tl = (TextLayer *)window_get_user_data(window);
  if (tl) text_layer_destroy(tl);
}

static void select_click(MenuLayer *m, MenuIndex *i, void *ctx) {
  static char buf[128];
  snprintf(buf, sizeof(buf), "%s\n%s\nLoad: %d", s_names[i->row], s_types[i->row], s_loads[i->row]);
  if (s_detail) window_destroy(s_detail);
  s_detail = window_create();
  window_set_user_data(s_detail, buf);
  window_set_window_handlers(s_detail, (WindowHandlers){
    .load = detail_load,
    .unload = detail_unload,
  });
  window_stack_push(s_detail, true);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  s_menu = menu_layer_create(b);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = get_sections,
    .get_num_rows = get_rows,
    .draw_row = draw_row,
    .select_click = select_click,
  });
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu);
}

void activities_show(char *payload) {
  s_count = 0;
  char *save;
  char *row = strtok_r(payload, "\n", &save);
  while (row && s_count < MAX_ACT) {
    char *s2;
    char *date = strtok_r(row, "|", &s2);
    char *type = strtok_r(NULL, "|", &s2);
    char *name = strtok_r(NULL, "|", &s2);
    char *load = strtok_r(NULL, "|", &s2);
    if (date) snprintf(s_dates[s_count], 12, "%s", date);
    if (type) snprintf(s_types[s_count], 16, "%s", type);
    if (name) snprintf(s_names[s_count], NAME_LEN, "%s", name);
    s_loads[s_count] = load ? atoi(load) : 0;
    s_count++;
    row = strtok_r(NULL, "\n", &save);
  }

  if (s_window) window_destroy(s_window);
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
