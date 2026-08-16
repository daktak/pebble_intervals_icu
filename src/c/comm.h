#ifndef COMM_H
#define COMM_H

#include <pebble.h>

typedef enum {
  CMD_OPEN_CONFIG = 1,
  CMD_WEEK = 2,
  CMD_LOAD = 3
} Cmd;

void comm_init(void);
void comm_send_cmd(Cmd cmd);
bool comm_has_api_key(void);

#endif
