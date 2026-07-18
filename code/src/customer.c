#include <cerrno>
#include <cstdlib>
#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "menu.h"
#include "order.h"
#include "restaurant.h"
#include "customer.h"
#include "xoshiro256plusplus.h"
#include <stdbool.h>
#include <errno.h>
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
  int max_dishes_per_order = (int) args->max_dishes_per_order;
  int patience_level_range = (int) args->patience_level_range;

  // xoshiro's generator thread-local state loaded from the main thread
  uint64_t s[4];
  s[0] = args->seed[0];
  s[1] = args->seed[1];
  s[2] = args->seed[2];
  s[3] = args->seed[3];

  // Generate random dish dynamic array
  int length = (next(s) % max_dishes_per_order) + 1;

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
  order.patienceLevel = patienceFloor + (next(s) % patience_level_range);
  order.arrivalTime = time(NULL);
  order.count = length;
  order.dishList = dishList;

  // Sets order in the orderTable
  sem_wait(slotMut);

  orderSlot = &order;

  sem_post(slotMut);

  // Sends its id (index over the orderTable) over the pipe to a receiving waiter
  write(txArrival, &tableNumber, sizeof(int));

  // Customer loop
  bool exit = false;
  while(!exit){

    //Checks for incoming dishes
    void *in_dish;
    ssize_t r = read(rxServing, in_dish, sizeof(int));

    if(r > 0){
      int in_dish = (int) in_dish;
      for(int i=0;i<orderSlot->count;i++){
        // Compares each dish in the customer order with the menu dish indexed by the number taken from the waiter
        if (orderSlot->dishList[i].dish == &menu->dishes[in_dish] && orderSlot->dishList[i].satisfied == false){
          orderSlot->dishList[i].satisfied = true;
        }
      }
    }else if(r == -1 && errno == EAGAIN){ // Nothing available yet (I guess nothing happens here)

    }else if (r == 0){ // Pipe is closed (We won't close it I think)

    }else{ // Error
      return (void*) -1; // There is a better way
    }

    // Decrements patience
    orderSlot->patienceLevel--;

    // If patience is 0 exits
    if(orderSlot->patienceLevel <= 0){
      exit = true;
    }

    //If all dishes are satisfied exits
    for(int i=0;i<orderSlot->count;i++){
      if(!orderSlot->dishList[i].satisfied){
        break;
      }else{
        exit = true;
      }
    }
  }


  // Changes global score

  // Exits
  orderSlot = NULL;
  free(dishList);

  // Frees CustomerArgs
  free(args);
}
