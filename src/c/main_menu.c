#include "main_menu.h"
#include "comm.h"
#include "season.h"
#include "stats.h"
#include "today.h"
#include "ui.h"

#define MENU_NUM_ROWS 6
#define MENU_CELL_HEIGHT 36

static Window *s_window;
static MenuLayer *s_menu;

static uint16_t get_sections(MenuLayer *m, void *ctx) { return 1; }
static uint16_t get_rows(MenuLayer *m, uint16_t section, void *ctx) { return MENU_NUM_ROWS; }

static int16_t get_cell_height(MenuLayer *m, MenuIndex *i, void *ctx) {
  return PBL_IF_ROUND_ELSE(
    menu_layer_is_index_selected(m, i) ?
      MENU_CELL_ROUND_FOCUSED_SHORT_CELL_HEIGHT : MENU_CELL_ROUND_UNFOCUSED_TALL_CELL_HEIGHT,
    MENU_CELL_HEIGHT);
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *i, void *data) {
  const char *titles[MENU_NUM_ROWS] = {
    "Today", "Weekly Stats", "Activities", "Training Load", "Trends", "Season Bests"
  };
  const char *subs[MENU_NUM_ROWS] = { NULL, NULL, NULL, NULL, NULL, NULL };
  menu_cell_basic_draw(ctx, cell, titles[i->row], subs[i->row], NULL);
}

static void select_click(MenuLayer *m, MenuIndex *i, void *ctx) {
  switch (i->row) {
    case 0:
      today_show();
      break;
    case 1:
      stats_show();
      break;
    case 2:
      ui_dismiss_overlay();
      ui_show_loading("Loading...");
      comm_send_cmd(CMD_WEEK);
      break;
    case 3:
      ui_dismiss_overlay();
      ui_show_loading("Loading...");
      comm_send_cmd(CMD_LOAD);
      break;
    case 4:
      ui_dismiss_overlay();
      ui_show_loading("Loading...");
      comm_send_cmd(CMD_TRENDS);
      break;
    case 5:
      season_show();
      break;
  }
}

static void window_load(Window *window) {
  window_set_background_color(window, GColorWhite);
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  s_menu = menu_layer_create(b);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = get_sections,
    .get_num_rows = get_rows,
    .get_cell_height = get_cell_height,
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
}

Window *main_menu_window_create(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  return s_window;
}