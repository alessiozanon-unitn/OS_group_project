#include "waiter.h"
#include "menu.h"
#include "restaurant.h"

#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <semaphore.h>
#include <stdatomic.h>

extern Menu menu;
extern atomic_int score;
extern double gameSpeed;

void waiterSleep() { //Fixed sleeping time of an in-game minute
  double waitInMicro  = 1000000.0f * 60.0f / gameSpeed;
  int fullSleeps = lround(waitInMicro/(1000000.0f - 1.0f));
  int lastSleep = lround(waitInMicro)%(1000000 - 1);
  for (int i = 0; i<fullSleeps; i++) {
    usleep(1000000 -1);
  }
  usleep(lastSleep);
}

void* waiter(void* arg) {
  
  WaiterArg* args = (WaiterArg*) arg;
  uint64_t seed[] = {args->seed[0], args->seed[1], args->seed[2], args->seed[3]};
  int ID = args->ID;
  int cookCount = args->cookCount;
  int* txOrders = args->txOrders;
  int rxDishes = args->rxDishes;
  atomic_int* busyTime = args->busyTime;
  int customerCount = args->customerCount;
  int rxArrival = args->rxArrival;
  Order** orderTable = args->orderTable;
  int* txServing = args->txServing;

  free(arg);

  bool clientTaken[customerCount];  
  bool runFlag = true;

}
