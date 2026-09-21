#include "season.h"
#include "glance.h"
#include "comm.h"
#include <pebble.h>

void season_show(void) {
  glance_show(CMD_SEASON, "Loading...");
}

void season_set_data(const char *payload) {
  glance_set_data(payload);
}