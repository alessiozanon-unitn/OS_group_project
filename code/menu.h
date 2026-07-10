#ifndef DISH
#define DISH

#include <string.h>

struct Dish;
typedef struct Dish {
  String* name;
  int price;
  int time;
  int* requiredCount; //Dynamic array, holds size-1 in first element
  ResourceUnit* requiredTypes; //Dynamic array, size-1 of requiredCount
} Dish;
#endif

#ifndef MENU
#define MENU
struct Menu;
typedef struct Menu {
  int dishCount;
  Dish* dishes; //Dynamic array of dishCount size
} Menu;
#endif
