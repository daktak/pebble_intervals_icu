#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include <pebble.h>

Window *main_menu_window_create(void);
void main_menu_reload(void);
void main_menu_set_stats(const char *stats);
void main_menu_stats_failed(void);
bool main_menu_is_loading(void);

#endif
