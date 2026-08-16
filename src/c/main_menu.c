#include "main_menu.h"
#include "comm.h"
#include "ui.h"

#define HEADER_H 68
#define STATS_LEN 96

#define ST_IDLE 0
#define ST_LOADING 1
#define ST_DONE 2
#define ST_ERROR 3

static Window *s_window;
static MenuLayer *s_menu;
static TextLayer *s_header_tl;
static char s_stats_buf[STATS_LEN];
static int s_stats_state = ST_IDLE;

static uint16_t get_sections(MenuLayer *m, void *ctx) { return 1; }
static uint16_t get_rows(MenuLayer *m, uint16_t section, void *ctx) { return 2; }

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *i, void *data) {
  const char *titles[2] = { "Week Activities", "Training Load" };
  menu_cell_basic_draw(ctx, cell, titles[i->row], NULL, NULL);
}

static void select_click(MenuLayer *m, MenuIndex *i, void *ctx) {
  switch (i->row) {
    case 0:
      ui_dismiss_overlay();
      ui_show_loading("Loading...");
      comm_send_cmd(CMD_WEEK);
      break;
    case 1:
      ui_dismiss_overlay();
      ui_show_loading("Loading...");
      comm_send_cmd(CMD_LOAD);
      break;
  }
}

static void window_appear(Window *window) {
  if (s_menu) menu_layer_reload_data(s_menu);
  bool key = comm_has_api_key();
  bool id = comm_has_athlete_id();
  APP_LOG(APP_LOG_LEVEL_INFO, "main: appear key=%d id=%d state=%d", key, id, s_stats_state);
  if (key && id) {
    if (s_header_tl) {
      text_layer_set_text_color(s_header_tl, GColorBlack);
      text_layer_set_text(s_header_tl, "Loading stats...");
    }
    s_stats_state = ST_LOADING;
    comm_send_cmd(CMD_STATS);
    APP_LOG(APP_LOG_LEVEL_INFO, "main: requesting STATS");
  }
}

static void window_load(Window *window) {
  window_set_background_color(window, GColorWhite);
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_header_tl = text_layer_create(GRect(0, 0, b.size.w, HEADER_H));
  text_layer_set_background_color(s_header_tl, GColorWhite);
  if (!comm_has_api_key()) {
    text_layer_set_text_color(s_header_tl, GColorRed);
    text_layer_set_text(s_header_tl, "Set your API Key");
  } else if (!comm_has_athlete_id()) {
    text_layer_set_text_color(s_header_tl, GColorRed);
    text_layer_set_text(s_header_tl, "Set Athlete ID");
  } else {
    text_layer_set_text_color(s_header_tl, GColorBlack);
    text_layer_set_text(s_header_tl, "Loading stats...");
    s_stats_state = ST_LOADING;
  }
  text_layer_set_font(s_header_tl, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_header_tl, GTextAlignmentLeft);
  text_layer_set_overflow_mode(s_header_tl, GTextOverflowModeWordWrap);
  layer_add_child(root, text_layer_get_layer(s_header_tl));

  GRect mb = GRect(0, HEADER_H, b.size.w, b.size.h - HEADER_H);
  s_menu = menu_layer_create(mb);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = get_sections,
    .get_num_rows = get_rows,
    .draw_row = draw_row,
    .select_click = select_click,
  });
  menu_layer_set_normal_colors(s_menu, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu, GColorBlack, GColorWhite);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  if (s_header_tl) text_layer_destroy(s_header_tl);
  s_header_tl = NULL;
}

void main_menu_set_stats(const char *stats) {
  if (!stats) return;
  snprintf(s_stats_buf, sizeof(s_stats_buf), "%s", stats);
  s_stats_state = ST_DONE;
  if (s_header_tl) {
    text_layer_set_text_color(s_header_tl, GColorBlack);
    text_layer_set_text(s_header_tl, s_stats_buf);
  }
}

void main_menu_stats_failed(void) {
  s_stats_state = ST_ERROR;
  if (s_header_tl) {
    text_layer_set_text_color(s_header_tl, GColorRed);
    text_layer_set_text(s_header_tl, "Stats failed");
  }
}

bool main_menu_is_loading(void) {
  return s_stats_state == ST_LOADING;
}

void main_menu_reload(void) {
  if (s_menu) menu_layer_reload_data(s_menu);
}

Window *main_menu_window_create(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
    .appear = window_appear,
  });
  return s_window;
}
