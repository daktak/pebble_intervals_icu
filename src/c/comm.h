#ifndef COMM_H
#define COMM_H

#include <pebble.h>

typedef enum {
  CMD_OPEN_CONFIG = 1,
  CMD_WEEK = 2,
  CMD_LOAD = 3,
  CMD_STATS = 4,
  CMD_ACTIVITY_DETAIL = 5
} Cmd;

void comm_init(void);
void comm_send_cmd(Cmd cmd);
void comm_send_activity_detail(int idx);
bool comm_has_api_key(void);
bool comm_has_athlete_id(void);
void main_menu_set_stats(const char *stats);
void activities_set_detail(const char *payload);

#endif
