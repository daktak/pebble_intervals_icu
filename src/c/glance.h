#ifndef GLANCE_H
#define GLANCE_H

#include <pebble.h>
#include "comm.h"

void glance_show(Cmd cmd, const char *loading_msg);
void glance_set_data(const char *payload);
void glance_error(const char *msg);
bool glance_is_loading(void);

#endif