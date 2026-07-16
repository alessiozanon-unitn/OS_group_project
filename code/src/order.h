#ifndef ORDER
#define ORDER

#include "time.h"
#include "menu.h"
#include <stdatomic.h>

typedef struct OrderNode {
  Dish* dish;
  bool satisfied;
} OrderNode;

typedef struct Order {
  int ID; // Unique order identifier, used by the waiter to check if the customer has left
  int waitedTime; // Cyclicly updated by the customer
  atomic_int patienceLevel; // Current patience level updated both by the customer and waiter
  time_t arrivalTime;
  atomic_int count; // number of dishes
  OrderNode* dishList; //Dynamic Array
} Order;
#endif
