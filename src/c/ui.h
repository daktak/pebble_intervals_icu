#ifndef UI_H
#define UI_H

#include <pebble.h>

void ui_show_loading(const char *msg);
void ui_show_error(const char *msg);
void ui_dismiss_overlay(void);

#endif
