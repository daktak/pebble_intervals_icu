#include "comm.h"
#include "activities.h"
#include "load.h"
#include "ui.h"
#include "main_menu.h"

#define PKEY_API_KEY 10
#define PKEY_ATHLETE_ID 11

static void comm_send_key(uint32_t key, const char *val);

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;

  APP_LOG(APP_LOG_LEVEL_INFO, "inbox: received message");

  t = dict_find(iter, MESSAGE_KEY_API_KEY);
  if (t) {
    persist_write_string(PKEY_API_KEY, t->value->cstring);
    comm_send_key(MESSAGE_KEY_API_KEY, t->value->cstring);
    main_menu_reload();
  }
  t = dict_find(iter, MESSAGE_KEY_ATHLETE_ID);
  if (t) {
    persist_write_string(PKEY_ATHLETE_ID, t->value->cstring);
    comm_send_key(MESSAGE_KEY_ATHLETE_ID, t->value->cstring);
  }

  t = dict_find(iter, MESSAGE_KEY_ERR);
  if (t) {
    ui_show_error(t->value->cstring);
    return;
  }

  t = dict_find(iter, MESSAGE_KEY_ACTIVITIES);
  if (t) {
    APP_LOG(APP_LOG_LEVEL_INFO, "inbox: ACTIVITIES len=%d", (int)strlen(t->value->cstring));
    activities_show(t->value->cstring);
    ui_dismiss_overlay();
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
    load_show(ctl, atl, tsb, ts ? ts->value->cstring : "");
    ui_dismiss_overlay();
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

bool comm_has_api_key(void) {
  return persist_exists(PKEY_API_KEY);
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
