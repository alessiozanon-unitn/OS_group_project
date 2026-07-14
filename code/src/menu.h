#ifndef DISH
#define DISH

#include "kitchen.h"

int load_menu_from(char *file_path);

typedef struct Dish {
  char* name;
  int price;
  int time;
  int requiredSize; //Holds size of the following two arrays
  int* requiredCount; //Holds how much of a resource is needed
  int* requiredTypes; //Holds index of resouce needed;
} Dish;
#endif

#ifndef MENU
#define MENU
typedef struct Menu {
  int dishCount;
  Dish* dishes; //Array of dishCount size
} Menu;
#endif
