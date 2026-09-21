#include "stats.h"
#include "glance.h"
#include "comm.h"
#include <pebble.h>

void stats_show(void) {
  glance_show(CMD_STATS, "Loading...");
}

void stats_set_data(const char *payload) {
  glance_set_data(payload);
}