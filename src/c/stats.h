#ifndef STATS_H
#define STATS_H

#include <pebble.h>

void stats_show(void);
void stats_set_data(const char *payload);
void stats_error(const char *msg);
bool stats_is_loading(void);

#endif