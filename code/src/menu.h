#ifndef DISH
#define DISH

#include "kitchen.h"

typedef struct Dish {
  char* name;
  int price;
  int time;
  int* requiredCount; //Dynamic array, holds size-1 in first element
  ResourceUnit* requiredTypes; //Dynamic array, size-1 of requiredCount
} Dish;
#endif

#ifndef MENU
#define MENU
typedef struct Menu {
  int dishCount;
  Dish* dishes; //Dynamic array of dishCount size
} Menu;
#endif
