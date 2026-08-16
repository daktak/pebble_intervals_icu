#include "comm.h"
#include "main_menu.h"

int main(void) {
  comm_init();
  window_stack_push(main_menu_window_create(), true);
  app_event_loop();
  return 0;
}
