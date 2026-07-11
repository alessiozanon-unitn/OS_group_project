#include <stdlib.h>
#include "menu.h"
#include "order.h"
#include "restaurant.h"
#include "customer.h"

extern Menu* menu;


void* customer (void* arg) {
  RandSeed* argument;
  argument = (RandSeed*)arg;
  srand(argument->seed); //Seed the rand with the argument TODO replace with good randing
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
