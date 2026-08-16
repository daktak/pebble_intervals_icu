#define _GNU_SOURCE
#include "activities.h"
#include "comm.h"
#include <pebble.h>
#include <string.h>

#define MAX_ACT 32
#define NAME_LEN 48
#define MAX_DET 24
#define DET_LBL 16
#define DET_VAL 24
#define ROW_H 22

static char s_dates[MAX_ACT][12];
static char s_types[MAX_ACT][16];
static char s_names[MAX_ACT][NAME_LEN];
static char s_dows[MAX_ACT][4];
static int s_loads[MAX_ACT];
static int s_count = 0;

static Window *s_window = NULL;
static MenuLayer *s_menu = NULL;
static Window *s_detail = NULL;
static TextLayer *s_detail_title = NULL;
static ScrollLayer *s_detail_scroll = NULL;
static Layer *s_detail_content = NULL;

static char s_dlbl[MAX_DET][DET_LBL];
static char s_dval[MAX_DET][DET_VAL];
static int s_dcount = 0;
static int s_detail_idx = -1;
static bool s_detail_loading = false;

static int s_zone_secs[8];
static int s_zone_count = 0;
static bool s_has_zones = false;
#define BAR_H 16
#define BAR_TOP 4
#define BAR_GAP 4
#define BAR_AREA (BAR_TOP + BAR_H + BAR_GAP)

static uint16_t get_sections(MenuLayer *m, void *ctx) { return 1; }
static uint16_t get_rows(MenuLayer *m, uint16_t section, void *ctx) { return s_count == 0 ? 1 : s_count; }

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *i, void *data) {
  if (s_count == 0) {
    menu_cell_basic_draw(ctx, cell, "No activities", NULL, NULL);
    return;
  }
  static char sub[32];
  static char title[NAME_LEN];
  const char *nm = s_names[i->row][0] ? s_names[i->row]
    : (s_types[i->row][0] ? s_types[i->row] : s_dates[i->row]);
  snprintf(title, sizeof(title), "%.28s", nm);
  snprintf(sub, sizeof(sub), "%s  L%d  %.10s", s_dows[i->row], s_loads[i->row], s_types[i->row]);
  menu_cell_basic_draw(ctx, cell, title, sub, NULL);
}

static GColor zone_color(int i) {
  GColor c = GColorBlack;
  switch (i) {
#ifdef PBL_COLOR
    case 0: c = GColorCeleste; break;
    case 1: c = GColorIslamicGreen; break;
    case 2: c = GColorChromeYellow; break;
    case 3: c = GColorOrange; break;
    case 4: c = GColorRed; break;
    case 5: c = GColorPurple; break;
    case 6: c = GColorBlack; break;
    default: c = GColorDarkGray; break;
#else
    case 0: c = GColorFromRGB(230, 230, 230); break;
    case 1: c = GColorFromRGB(190, 190, 190); break;
    case 2: c = GColorFromRGB(150, 150, 150); break;
    case 3: c = GColorFromRGB(110, 110, 110); break;
    case 4: c = GColorFromRGB(70, 70, 70); break;
    case 5: c = GColorFromRGB(35, 35, 35); break;
    case 6: c = GColorBlack; break;
    default: c = GColorFromRGB(120, 120, 120); break;
#endif
  }
  return c;
}

static int detail_offset(void) {
  return (s_has_zones && s_zone_count > 0) ? BAR_AREA : 0;
}

static void draw_zone_segment(GContext *ctx, int x, int y, int w, int h, int i) {
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, zone_color(i));
  graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone);
#else
  int spacing;
  switch (i) {
    case 0: spacing = 12; break;
    case 1: spacing = 8;  break;
    case 2: spacing = 5;  break;
    case 3: spacing = 4;  break;
    case 4: spacing = 3;  break;
    case 5: spacing = 2;  break;
    default: spacing = 0;  break;
  }
  if (spacing == 0) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone);
  } else {
    graphics_context_set_stroke_color(ctx, GColorBlack);
    for (int yy = y; yy < y + h; yy += spacing)
      graphics_draw_line(ctx, GPoint(x, yy), GPoint(x + w - 1, yy));
  }
#endif
}

static void detail_content_draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int w = b.size.w;
  GFont f = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  graphics_context_set_text_color(ctx, GColorBlack);
  if (s_detail_loading) {
    graphics_draw_text(ctx, "Loading...", f, GRect(4, 4, w - 8, ROW_H), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    return;
  }
  if (s_has_zones && s_zone_count > 0) {
    int barW = w - 8;
    int total = 0;
    for (int k = 0; k < s_zone_count; k++) total += s_zone_secs[k];
    if (total > 0) {
      int x = 4;
      for (int k = 0; k < s_zone_count; k++) {
        int segW = (s_zone_secs[k] * barW) / total;
        if (k == s_zone_count - 1) segW = (4 + barW) - x;
        graphics_context_set_fill_color(ctx, zone_color(k));
        draw_zone_segment(ctx, x, BAR_TOP, segW > 0 ? segW - 1 : 0, BAR_H, k);
        x += segW;
      }
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_draw_rect(ctx, GRect(4, BAR_TOP, barW, BAR_H));
    }
  }
  int y = detail_offset();
  for (int i = 0; i < s_dcount; i++) {
    GRect r = GRect(4, y, w - 8, ROW_H);
    graphics_draw_text(ctx, s_dlbl[i], f, r, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, s_dval[i], f, r, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
    y += ROW_H;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "detail: draw rows=%d", s_dcount);
}

static void detail_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "detail: load");
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  window_set_background_color(window, GColorWhite);
  int titleH = 24;
  s_detail_title = text_layer_create(GRect(4, 0, b.size.w - 8, titleH));
  text_layer_set_background_color(s_detail_title, GColorWhite);
  text_layer_set_text_color(s_detail_title, GColorBlack);
  text_layer_set_font(s_detail_title, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text(s_detail_title, (s_detail_idx >= 0 && s_detail_idx < s_count && s_names[s_detail_idx][0]) ? s_names[s_detail_idx] : "Activity");
  text_layer_set_text_alignment(s_detail_title, GTextAlignmentLeft);
  text_layer_set_overflow_mode(s_detail_title, GTextOverflowModeTrailingEllipsis);
  layer_add_child(root, text_layer_get_layer(s_detail_title));

  int visH = b.size.h - titleH;
  int contentH = detail_offset() + s_dcount * ROW_H;
  if (contentH < visH) contentH = visH;

  s_detail_scroll = scroll_layer_create(GRect(0, titleH, b.size.w, visH));
  s_detail_content = layer_create(GRect(0, 0, b.size.w, MAX_DET * ROW_H));
  layer_set_update_proc(s_detail_content, detail_content_draw);
  scroll_layer_add_child(s_detail_scroll, s_detail_content);
  scroll_layer_set_content_size(s_detail_scroll, GSize(b.size.w, contentH));
  scroll_layer_set_click_config_onto_window(s_detail_scroll, window);
  layer_add_child(root, scroll_layer_get_layer(s_detail_scroll));
}

static void detail_unload(Window *window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "detail: unload");
  if (s_detail_title) text_layer_destroy(s_detail_title);
  s_detail_title = NULL;
  if (s_detail_content) layer_destroy(s_detail_content);
  s_detail_content = NULL;
  if (s_detail_scroll) scroll_layer_destroy(s_detail_scroll);
  s_detail_scroll = NULL;
  s_detail = NULL;
}

static void select_click(MenuLayer *m, MenuIndex *i, void *ctx) {
  if (s_count == 0) return;
  if (s_detail) return;
  APP_LOG(APP_LOG_LEVEL_INFO, "detail: select row=%d", i->row);
  s_detail_idx = i->row;
  s_detail_loading = true;
  s_dcount = 0;
  s_has_zones = false;
  s_zone_count = 0;
  s_detail = window_create();
  window_set_window_handlers(s_detail, (WindowHandlers){
    .load = detail_load,
    .unload = detail_unload,
  });
  window_stack_push(s_detail, false);
  comm_send_activity_detail(s_detail_idx);
}

static void window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "activities: window_load");
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
  APP_LOG(APP_LOG_LEVEL_INFO, "activities: window_unload");
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  s_window = NULL;
}

void activities_show(char *payload) {
  APP_LOG(APP_LOG_LEVEL_INFO, "activities: show");
  s_count = 0;
  char *save;
  char *row = strtok_r(payload, "\n", &save);
  while (row && s_count < MAX_ACT) {
    char *s2;
    char *date = strtok_r(row, "|", &s2);
    char *type = strtok_r(NULL, "|", &s2);
    char *name = strtok_r(NULL, "|", &s2);
    char *load = strtok_r(NULL, "|", &s2);
    char *dow = strtok_r(NULL, "|", &s2);
    if (date) snprintf(s_dates[s_count], 12, "%s", date);
    if (type) snprintf(s_types[s_count], 16, "%s", type);
    if (name) snprintf(s_names[s_count], NAME_LEN, "%s", name);
    if (dow) snprintf(s_dows[s_count], 4, "%s", dow);
    s_loads[s_count] = load ? atoi(load) : 0;
    s_count++;
    row = strtok_r(NULL, "\n", &save);
  }

  if (s_detail) {
    window_destroy(s_detail);
    s_detail = NULL;
  }
  if (s_window) window_destroy(s_window);
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

void activities_set_detail(const char *payload) {
  if (!payload) return;
  s_dcount = 0;
  s_has_zones = false;
  s_zone_count = 0;
  const char *p = payload;
  while (s_dcount < MAX_DET && *p) {
    char line[96];
    int li = 0;
    while (*p && *p != '\n' && li < (int)sizeof(line) - 1) line[li++] = *p++;
    line[li] = '\0';
    if (*p == '\n') p++;
    char *sep = strchr(line, '|');
    if (sep) {
      *sep = '\0';
      if (strncmp(line, "@@ZONES@@", 10) == 0) {
        char *zs = sep + 1;
        char *zp;
        char *tok = strtok_r(zs, ",", &zp);
        int cnt = 0;
        while (tok && cnt < 8) {
          s_zone_secs[cnt] = atoi(tok);
          cnt++;
          tok = strtok_r(NULL, ",", &zp);
        }
        s_zone_count = cnt;
        int tot = 0;
        for (int k = 0; k < cnt; k++) tot += s_zone_secs[k];
        s_has_zones = (cnt > 0 && tot > 0);
        continue;
      }
      snprintf(s_dlbl[s_dcount], sizeof(s_dlbl[s_dcount]), "%s", line);
      snprintf(s_dval[s_dcount], sizeof(s_dval[s_dcount]), "%s", sep + 1);
    } else {
      snprintf(s_dlbl[s_dcount], sizeof(s_dlbl[s_dcount]), "%s", line);
      s_dval[s_dcount][0] = '\0';
    }
    s_dcount++;
  }
  s_detail_loading = false;
  APP_LOG(APP_LOG_LEVEL_INFO, "detail: set_detail rows=%d zones=%d", s_dcount, s_zone_count);
  if (s_detail_scroll && s_detail_content) {
    GRect sb = layer_get_bounds(scroll_layer_get_layer(s_detail_scroll));
    int contentH = detail_offset() + s_dcount * ROW_H;
    if (contentH < sb.size.h) contentH = sb.size.h;
    scroll_layer_set_content_size(s_detail_scroll, GSize(sb.size.w, contentH));
    layer_mark_dirty(scroll_layer_get_layer(s_detail_scroll));
  }
}
