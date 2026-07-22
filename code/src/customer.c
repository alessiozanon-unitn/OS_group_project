#include <math.h>
#include <pthread.h>
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
#include "utils.h"
#include <time.h>
#include "errorcodes.h"
#include <stdio.h>

extern Menu* menu;
extern atomic_int score;
extern double gameSpeed;

sem_t* slotMut; //Allows the thread-exiting function to do so cleanly and gracefully
Order* orderSlot;
atomic_int* status;

void customerStop (ErrorVals errornumber) {
  atomic_store(status, ERROR);
  perror("Customer runtime error");
  int semret = 0;
  do {
    semret = sem_wait(slotMut);
    if (semret == -1 && errno != EINTR) {
      perror("Customer can't access their slot mut");
      pthread_exit((void*) SEMAPHORE_FAIL);
    }
  } while (semret = 0);
  
  orderSlot = NULL;
  sem_post(slotMut);
  
  pthread_exit((void*) errornumber);
}

void* customer (void* arg) {
  int patienceFloor = 0;

  // Score variables
  int total_price = 0;
  int time_to_serve = 0; // Cyclicly updated by the customer (waiting time)
  int number_of_dishes_served = 0;


  //Loads args
  CustomerArg* args = (CustomerArg *) arg;

  orderSlot = (Order *) args->orderSlot;
  slotMut = (sem_t *) args->slotMut;
  int rxServing = (int) args->rxServing;
  int tableNumber = (int) args->tableNumber;
  int txArrival = (int) args->txArrival;
  int max_dishes_per_order = (int) args->max_dishes_per_order;
  int patience_level_range = (int) args->patience_level_range;
  status = args->status;
  // xoshiro's generator thread-local state loaded from the main thread
  uint64_t s[4];
  s[0] = args->seed[0];
  s[1] = args->seed[1];
  s[2] = args->seed[2];
  s[3] = args->seed[3];

  // Frees CustomerArgs
  free(args);

  // Generate random dish dynamic array
  int length = (next(s) % max_dishes_per_order) + 1;
  
  OrderNode *dishList = malloc(sizeof(OrderNode)*length);
  if (dishList == NULL && errno == ENOMEM) customerStop(MALLOC_FAIL);

  for(int i=0;i<length;i++){
    int dishIndex = next(s) % menu->dishCount;
    dishList[i].dish = &menu->dishes[dishIndex];
    dishList[i].satisfied = false;
    total_price += menu->dishes[dishIndex].price;
    // Adds dish time to patience floor
    patienceFloor += menu->dishes[dishIndex].time;
  }


  // Generates random order
  Order order;
  order.patienceLevel = patienceFloor + (next(s) % patience_level_range);
  atomic_store(&order.arrivalTime, time(NULL));
  order.count = length;
  order.dishList = dishList;

  // Sets order in the orderTable
  int semret = 0;
  do {
    semret = sem_wait(slotMut);
    if (semret == -1 && errno != EINTR) customerStop(SEMAPHORE_FAIL);
  } while (semret != 0);

  *orderSlot = order;

  sem_post(slotMut);

  // Sends its id (index over the orderTable) over the pipe to a receiving waiter
  int writeret = 0;
  do {
    writeret = write(txArrival, &tableNumber, sizeof(int));
    if (writeret == -1 && errno != EINTR) customerStop(WRITE_FAIL);
  } while (writeret != 0);

  //Officially ready to wait 
  atomic_store(status, WAITING);

  // Customer loop
  bool all_satisfied;
  while(true){
    all_satisfied = true;

    // If (patience is 0) OR (all dishes are satisfied) exits
    int semret = 0;
    do {
      semret = sem_wait(slotMut);
      if (semret == -1 && errno != EINTR) customerStop(SEMAPHORE_FAIL);
    } while (semret != 0);

    bool out_of_patience = orderSlot->patienceLevel <= time_to_serve;

    for(int i=0;i<orderSlot->count;i++){
      if(!orderSlot->dishList[i].satisfied)
        all_satisfied = false;
    }

    sem_post(slotMut);

    if(all_satisfied || out_of_patience)
      break;


    //Checks for incoming dishes
    int in_dish;
    ssize_t r; 
    do {
      r = read(rxServing, &in_dish, sizeof(int));
      if (r == -1 && errno != EAGAIN) customerStop(READ_FAIL);

      if(r > 0){
        int semret = 0;
        do {
          semret = sem_wait(slotMut);
          if (semret == -1 && errno != EINTR) customerStop(SEMAPHORE_FAIL);
        } while (semret != 0);
      
        bool expended = false;

        for(int i=0;i<orderSlot->count && !expended; i++){
          // Compares each dish in the customer order with the menu dish indexed by the number taken from the waiter
          if (orderSlot->dishList[i].dish == &menu->dishes[in_dish] && orderSlot->dishList[i].satisfied == false){
            orderSlot->dishList[i].satisfied = true;
            number_of_dishes_served++;
            expended = true;
          }
        }
        sem_post(slotMut);
      }

    } while (r != -1); //If the pipe is empty move on
    // Decrements patience
    do {
      semret = sem_wait(slotMut);
      if (semret == -1 && errno != EINTR) customerStop(SEMAPHORE_FAIL);
    } while (semret != 0);
    atomic_fetch_sub(&orderSlot->patienceLevel, 1);
    sem_post(slotMut);

    // Increments waited time
    time_to_serve++;

    custom_sleep();
  }


  // Changes global score
  do {
    semret = sem_wait(slotMut);
    if (semret == -1 && errno != EINTR) customerStop(SEMAPHORE_FAIL);
  } while (semret != 0);

  if(all_satisfied){ // If every order was satisfied
    atomic_fetch_add(&score, lround(total_price * (1.0 - ((double) time_to_serve/(double)orderSlot->patienceLevel))));
  }else{ // If patience ran out
    atomic_fetch_add(&score, lround(total_price * log2(1 + ((double) orderSlot->patienceLevel / (1 + number_of_dishes_served)))));
  }

  // Exits
  orderSlot = NULL;
  free(dishList);
  
  sem_post(slotMut);
  
  if (all_satisfied) atomic_store(status, SATISFIED);
  else atomic_store(status, UNSATISFIED);

  pthread_exit(ALL_OK);
}
