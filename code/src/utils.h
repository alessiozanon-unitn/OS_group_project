#ifndef UTILS
#define UTILS

#include "kitchen.h"
#include "menu.h"

typedef _Atomic(time_t) atomic_time;

void custom_sleep();
void print_kitchen(Kitchen *kitchen);
void print_menu(Menu *menu);
#endif
