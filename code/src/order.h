#ifndef ORDER
#define ORDER

#include "menu.h"

typedef struct OrderNode {
  Dish* dish;
  bool satisfied;
} OrderNode;

typedef struct Order {
  int total_price;
  int total_prep;
  int count;
  OrderNode* dishList; //Dynamic Array
} Order;
#endif
