#include "ui.h"

static Window *s_overlay = NULL;
static TextLayer *s_overlay_text = NULL;
static char s_overlay_msg[64];
static bool s_overlay_error = false;

static void overlay_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  s_overlay_text = text_layer_create(GRect(8, b.size.h / 2 - 24, b.size.w - 16, 60));
  text_layer_set_text_alignment(s_overlay_text, GTextAlignmentCenter);
  text_layer_set_font(s_overlay_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_color(s_overlay_text, GColorWhite);
  text_layer_set_background_color(s_overlay_text, GColorClear);
  text_layer_set_text(s_overlay_text, s_overlay_msg);
  layer_add_child(root, text_layer_get_layer(s_overlay_text));
}

static void overlay_unload(Window *window) {
  text_layer_destroy(s_overlay_text);
  s_overlay_text = NULL;
  s_overlay = NULL;
}

static void present_overlay(void) {
  ui_dismiss_overlay();
  s_overlay = window_create();
  window_set_background_color(s_overlay, s_overlay_error ? GColorRed : GColorBlack);
  window_set_window_handlers(s_overlay, (WindowHandlers){
    .load = overlay_load,
    .unload = overlay_unload,
  });
  window_stack_push(s_overlay, false);
}

void ui_show_loading(const char *msg) {
  snprintf(s_overlay_msg, sizeof(s_overlay_msg), "%s", msg);
  s_overlay_error = false;
  present_overlay();
}

void ui_show_error(const char *msg) {
  snprintf(s_overlay_msg, sizeof(s_overlay_msg), "%s", msg);
  s_overlay_error = true;
  present_overlay();
}

void ui_dismiss_overlay(void) {
  if (s_overlay) {
    APP_LOG(APP_LOG_LEVEL_INFO, "overlay: dismissing");
    window_destroy(s_overlay);
    s_overlay = NULL;
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "overlay: dismiss no-op (null)");
  }
}
