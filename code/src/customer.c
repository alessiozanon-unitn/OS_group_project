#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include "menu.h"
#include "order.h"
#include "restaurant.h"
#include "customer.h"
#include "xoshiro256plusplus.h"

extern Menu* menu;

void* customer (void* arg) {
  int patienceFloor = 0;
  int waitedTime = 0; // Cyclicly updated by the customer


  //Loads args
  CustomerArg* args = (CustomerArg *) arg;

  Order *orderSlot = (Order *) args->orderSlot;
  sem_t *slotMut = (sem_t *) args->slotMut;
  int rxServing = (int) args->rxServing;
  int tableNumber = (int) args->tableNumber;
  int txArrival = (int) args->txArrival;

  // xoshiro's generator thread-local state loaded from the main thread
  uint64_t s[4];
  s[0] = args->seed[0];
  s[1] = args->seed[1];
  s[2] = args->seed[2];
  s[3] = args->seed[3];

  // Generate random dish dynamic array
  int length = (next(s) % 10) + 1; // Maximum dishes per order capped at 10, might want to change that (NEW ENV)

  OrderNode *dishList = malloc(sizeof(OrderNode)*length);

  for(int i=0;i<length;i++){
    int dishIndex = next(s) % menu->dishCount;
    dishList[i].dish = &menu->dishes[dishIndex];
    dishList[i].satisfied = false;

    // Adds dish time to patience floor
    patienceFloor += menu->dishes[dishIndex].time;
  }



  // Generates random order
  Order order;
  order.patienceLevel = patienceFloor + (next(s) % 30); // 30 is a placeholder, will make an env var out of that
  order.arrivalTime = time(NULL);
  order.count = length;
  order.dishList = dishList;

  // Sets order in the orderTable
  sem_wait(slotMut);

  orderSlot = &order;

  sem_post(slotMut);



  orderSlot = NULL;
  free(dishList);
}
