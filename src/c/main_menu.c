#include "main_menu.h"
#include "comm.h"
#include "ui.h"

#ifdef PBL_PLATFORM_EMERY
  #define STATS_FONT FONT_KEY_GOTHIC_24
  #define STATS_ROW_H 28
#else
  #define STATS_FONT FONT_KEY_GOTHIC_14
  #define STATS_ROW_H 18
#endif
#define STATS_PAD 6
#define HEADER_H (STATS_ROW_H * 4 + STATS_PAD * 2)

#define ST_IDLE 0
#define ST_LOADING 1
#define ST_DONE 2
#define ST_ERROR 3

static Window *s_window;
static MenuLayer *s_menu;
static Layer *s_stats_layer;
static char s_l1[4][8];
static char s_v1[4][16];
static char s_l2[4][8];
static char s_v2[4][16];
static char s_status[40];
static GColor s_status_color;
static int s_stats_state = ST_IDLE;

static uint16_t get_sections(MenuLayer *m, void *ctx) { return 1; }
static uint16_t get_rows(MenuLayer *m, uint16_t section, void *ctx) { return 2; }
static int16_t get_cell_height(MenuLayer *m, MenuIndex *i, void *ctx) {
  GRect mb = layer_get_bounds(menu_layer_get_layer(m));
  return mb.size.h / 2 - 4;
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

static void window_appear(Window *window) {
  if (s_menu) menu_layer_reload_data(s_menu);
  bool key = comm_has_api_key();
  bool id = comm_has_athlete_id();
  APP_LOG(APP_LOG_LEVEL_INFO, "main: appear key=%d id=%d state=%d", key, id, s_stats_state);
  if (key && id) {
    snprintf(s_status, sizeof(s_status), "Loading stats...");
    s_status_color = GColorBlack;
    s_stats_state = ST_LOADING;
    layer_mark_dirty(s_stats_layer);
    comm_send_cmd(CMD_STATS);
    APP_LOG(APP_LOG_LEVEL_INFO, "main: requesting STATS");
  }
}

static void window_load(Window *window) {
  window_set_background_color(window, GColorWhite);
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  for (int i = 0; i < 4; i++) {
    s_l1[i][0] = '\0';
    s_v1[i][0] = '\0';
    s_l2[i][0] = '\0';
    s_v2[i][0] = '\0';
  }
  if (!comm_has_api_key()) {
    snprintf(s_status, sizeof(s_status), "Set your API Key");
    s_status_color = GColorRed;
    s_stats_state = ST_IDLE;
  } else if (!comm_has_athlete_id()) {
    snprintf(s_status, sizeof(s_status), "Set Athlete ID");
    s_status_color = GColorRed;
    s_stats_state = ST_IDLE;
  } else {
    snprintf(s_status, sizeof(s_status), "Loading stats...");
    s_status_color = GColorBlack;
    s_stats_state = ST_LOADING;
  }

  s_stats_layer = layer_create(GRect(0, 0, b.size.w, HEADER_H));
  layer_set_update_proc(s_stats_layer, stats_layer_update);
  layer_add_child(root, s_stats_layer);

  GRect mb = GRect(0, HEADER_H, b.size.w, b.size.h - HEADER_H);
  s_menu = menu_layer_create(mb);
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
  layer_destroy(s_stats_layer);
  s_stats_layer = NULL;
}

static void copy_token(char *dst, int max, const char **pp) {
  int i = 0;
  while (**pp == ' ' || **pp == '\t') (*pp)++;
  while (**pp && **pp != ' ' && **pp != '\t' && **pp != '\n' && i < max - 1) {
    dst[i++] = **pp;
    (*pp)++;
  }
  dst[i] = '\0';
}

void main_menu_set_stats(const char *stats) {
  if (!stats) return;
  const char *p = stats;
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

void main_menu_stats_failed(void) {
  s_stats_state = ST_ERROR;
  snprintf(s_status, sizeof(s_status), "Stats failed");
  s_status_color = GColorRed;
  if (s_stats_layer) layer_mark_dirty(s_stats_layer);
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
