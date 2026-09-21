#include "today.h"
#include "glance.h"
#include "comm.h"
#include <pebble.h>

void today_show(void) {
  glance_show(CMD_TODAY, "Loading...");
}

void today_set_data(const char *payload) {
  glance_set_data(payload);
}