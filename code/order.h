#ifndef ORDER
#define ORDER

#include "menu.h"

struct OrderNode;
typedef OrderNode {
  Dish dish;
  bool satisfied;
} OrderNode;

struct Order;
typedef struct Order {
  int total_price;
  int total_prep;
  int count;
  OrderNode* dishList; //Dynamic Array
} Order;
#endif
