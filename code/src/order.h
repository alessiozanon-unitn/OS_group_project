#ifndef ORDER
#define ORDER

#include "time.h"
#include "menu.h"

typedef struct OrderNode {
  Dish* dish;
  bool satisfied;
} OrderNode;

typedef struct Order {
  int count;
  int patienceLevel;
  time_t arrivalTime;
  OrderNode* dishList; //Dynamic Array
} Order;
#endif
