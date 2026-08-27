#ifndef ORDER
#define ORDER

#include "restaurant.h"
#include "time.h"
#include "menu.h"
#include <stdatomic.h>

typedef struct OrderNode {
  Dish* dish;
  bool satisfied;
} OrderNode;

typedef struct Order {
  atomic_int patienceLevel; // Current patience level updated both by the customer and waiter
  atomic_time arrivalTime;
  int count; // number of dishes
  OrderNode* dishList; //Dynamic Array
} Order;
#endif
