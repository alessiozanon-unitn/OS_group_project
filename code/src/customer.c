#include <stdint.h>
#include <stdlib.h>
#include "menu.h"
#include "order.h"
#include "restaurant.h"
#include "customer.h"
#include "xoshiro256plusplus.h"

extern Menu* menu;


void* customer (void* arg) {
  uint64_t s[4]; // Local state to pass to next() to generate random numbers

  CustomerArg* args = (CustomerArg *) arg;

  // xoshiro's generator thread-local state loaded from the main thread
  s[0] = args->seed[0];
  s[1] = args->seed[1];
  s[2] = args->seed[2];
  s[3] = args->seed[3];


  int sel = menu->dishCount;
  int orderSize = rand()%sel +1; //Choose how many plates, from 1 to the full menu
  Order* order = malloc(sizeof(Order));
  order->count = orderSize;
  order->total_price = 0;
  order->total_prep = 0;
  order->dishList = malloc(sizeof(OrderNode)*orderSize);
  for (int i = 0; i<orderSize; i++) {
    order->dishList[i] = (OrderNode){.satisfied = false, .dish =& menu->dishes[rand()%sel]}; //Init each element of order as an unsatisfied random dish
    order->total_price += order->dishList[i].dish->price; //Add price to the count
    order->total_prep += order->dishList[i].dish->time; //Add prep time to the count
  }
  //TODO Here patience level can be calculated, when we decide a cap on it


}
