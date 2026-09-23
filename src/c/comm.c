#include "comm.h"
#include "activities.h"
#include "glance.h"
#include "load.h"
#include "season.h"
#include "stats.h"
#include "today.h"
#include "trend.h"
#include "ui.h"

#define PKEY_API_KEY 10
#define PKEY_ATHLETE_ID 11

static void comm_send_key(uint32_t key, const char *val);

static void parse_axis(DictionaryIterator *iter, char *a0, size_t n0, char *a1, size_t n1) {
  a0[0] = '\0';
  a1[0] = '\0';
  Tuple *tax = dict_find(iter, MESSAGE_KEY_AXIS);
  if (!tax) return;
  const char *p = tax->value->cstring;
  if (!p) return;
  while (*p == ' ' || *p == '\t') p++;
  size_t i = 0;
  while (*p && *p != ' ' && *p != '\t' && i < n0 - 1) a0[i++] = *p++;
  a0[i] = '\0';
  while (*p == ' ' || *p == '\t') p++;
  i = 0;
  while (*p && *p != ' ' && *p != '\t' && i < n1 - 1) a1[i++] = *p++;
  a1[i] = '\0';
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;

  APP_LOG(APP_LOG_LEVEL_INFO, "inbox: received message");

  t = dict_find(iter, MESSAGE_KEY_API_KEY);
  if (t) {
    persist_write_string(PKEY_API_KEY, t->value->cstring);
    comm_send_key(MESSAGE_KEY_API_KEY, t->value->cstring);
  }
  t = dict_find(iter, MESSAGE_KEY_ATHLETE_ID);
  if (t) {
    persist_write_string(PKEY_ATHLETE_ID, t->value->cstring);
    comm_send_key(MESSAGE_KEY_ATHLETE_ID, t->value->cstring);
  }

  t = dict_find(iter, MESSAGE_KEY_UNITS);
  if (t) {
    comm_send_key(MESSAGE_KEY_UNITS, t->value->cstring);
  }

  t = dict_find(iter, MESSAGE_KEY_ERR);
  if (t) {
    if (glance_is_loading()) {
      glance_error(t->value->cstring);
      return;
    }
    ui_show_error(t->value->cstring);
    return;
  }

  t = dict_find(iter, MESSAGE_KEY_ACTIVITIES);
  if (t) {
    APP_LOG(APP_LOG_LEVEL_INFO, "inbox: ACTIVITIES len=%d", (int)strlen(t->value->cstring));
    ui_dismiss_overlay();
    activities_show(t->value->cstring);
    return;
  }

  t = dict_find(iter, MESSAGE_KEY_TL_CTL);
  if (t) {
    int ctl = t->value->int32;
    int atl = 0;
    int tsb = 0;
    Tuple *ta = dict_find(iter, MESSAGE_KEY_TL_ATL);
    Tuple *tt = dict_find(iter, MESSAGE_KEY_TL_TSB);
    Tuple *ts = dict_find(iter, MESSAGE_KEY_TL_SERIES);
    if (ta) atl = ta->value->int32;
    if (tt) tsb = tt->value->int32;
    char ax0[8], ax1[8];
    parse_axis(iter, ax0, sizeof(ax0), ax1, sizeof(ax1));
    Tuple *tv = dict_find(iter, MESSAGE_KEY_VAR);
    ui_dismiss_overlay();
    load_show(ctl, atl, tsb, ts ? ts->value->cstring : "", ax0, ax1);
    if (tv) load_set_variability(tv->value->cstring);
    return;
  }

  t = dict_find(iter, MESSAGE_KEY_STATS);
  if (t) {
    APP_LOG(APP_LOG_LEVEL_INFO, "inbox: STATS len=%d", (int)strlen(t->value->cstring));
    stats_set_data(t->value->cstring);
    return;
  }

  t = dict_find(iter, MESSAGE_KEY_ACTIVITY_DETAIL);
  if (t) {
    APP_LOG(APP_LOG_LEVEL_INFO, "inbox: ACTIVITY_DETAIL len=%d", (int)strlen(t->value->cstring));
    activities_set_detail(t->value->cstring);
    return;
  }

  t = dict_find(iter, MESSAGE_KEY_TODAY);
  if (t) {
    APP_LOG(APP_LOG_LEVEL_INFO, "inbox: TODAY len=%d", (int)strlen(t->value->cstring));
    today_set_data(t->value->cstring);
    return;
  }

  t = dict_find(iter, MESSAGE_KEY_SEASON);
  if (t) {
    APP_LOG(APP_LOG_LEVEL_INFO, "inbox: SEASON len=%d", (int)strlen(t->value->cstring));
    season_set_data(t->value->cstring);
    return;
  }

  t = dict_find(iter, MESSAGE_KEY_TRENDS);
  if (t) {
    APP_LOG(APP_LOG_LEVEL_INFO, "inbox: TRENDS len=%d", (int)strlen(t->value->cstring));
    char ax0[8], ax1[8];
    parse_axis(iter, ax0, sizeof(ax0), ax1, sizeof(ax1));
    ui_dismiss_overlay();
    trend_show(t->value->cstring, ax0, ax1);
    return;
  }
}

void comm_send_key(uint32_t key, const char *val) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_cstring(out, key, val);
  app_message_outbox_send();
}

void comm_send_cmd(Cmd cmd) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  int32_t c = (int32_t)cmd;
  dict_write_int(out, MESSAGE_KEY_CMD, &c, sizeof(int32_t), true);
  app_message_outbox_send();
}

void comm_send_activity_detail(int idx) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  int32_t c = (int32_t)CMD_ACTIVITY_DETAIL;
  int32_t i = (int32_t)idx;
  dict_write_int(out, MESSAGE_KEY_CMD, &c, sizeof(int32_t), true);
  dict_write_int(out, MESSAGE_KEY_ACT_IDX, &i, sizeof(int32_t), true);
  app_message_outbox_send();
}

void comm_init(void) {
  app_message_register_inbox_received(inbox_received);
  app_message_open(1024, 64);

  if (persist_exists(PKEY_API_KEY)) {
    char buf[128];
    persist_read_string(PKEY_API_KEY, buf, sizeof(buf));
    comm_send_key(MESSAGE_KEY_API_KEY, buf);
  }
  if (persist_exists(PKEY_ATHLETE_ID)) {
    char buf[128];
    persist_read_string(PKEY_ATHLETE_ID, buf, sizeof(buf));
    comm_send_key(MESSAGE_KEY_ATHLETE_ID, buf);
  }
}
