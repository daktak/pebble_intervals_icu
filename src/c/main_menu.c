#include "main_menu.h"
#include "comm.h"
#include "ui.h"

static Window *s_window;
static MenuLayer *s_menu;

static uint16_t get_sections(MenuLayer *m, void *ctx) { return 1; }
static uint16_t get_rows(MenuLayer *m, uint16_t section, void *ctx) { return 2; }

static int16_t get_header_height(MenuLayer *m, uint16_t section, void *ctx) {
  return 30;
}

static void draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  GRect b = layer_get_bounds(cell);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_text_color(ctx, comm_has_api_key() ? GColorWhite : GColorRed);
  graphics_draw_text(ctx,
    comm_has_api_key() ? "Intervals.icu" : "Set your API Key",
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
    GRect(b.origin.x + 4, b.origin.y, b.size.w - 8, b.size.h),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentLeft,
    NULL);
}

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
}

static void window_load(Window *window) {
  window_set_background_color(window, GColorBlack);
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  s_menu = menu_layer_create(b);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = get_sections,
    .get_num_rows = get_rows,
    .get_header_height = get_header_height,
    .draw_header = draw_header,
    .draw_row = draw_row,
    .select_click = select_click,
  });
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu);
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
