#ifndef ORDER
#define ORDER

#include "menu.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <time.h>

typedef struct OrderNode {
  Dish* dish;
  bool satisfied;
} OrderNode;

typedef struct Order {
  atomic_int patienceLevel; // Current patience level updated both by the customer and waiter
  time_t arrivalTime;
  int count; // number of dishes
  OrderNode* dishList; //Dynamic Array
} Order;
#endif
