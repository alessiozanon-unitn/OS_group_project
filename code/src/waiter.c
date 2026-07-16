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

void waiterSleep(int sleepTime) {
  if (gameSpeed >= 1) {//Speed is faster than real-time, reduce usleep number
    for (int i = 0; i<sleepTime; i++) {
      for (int ii = 0; ii<60; ii++) {
        usleep(lround((1000000-1)/gameSpeed)); //Sleep for an in-game minute
      }
    }
  } else {//Speed is slower than real-time, sleep more times per in-game minute 
    double realSleepTime = 1000000.0f * 60.0f / gameSpeed;
    for (int i = 0; i<sleepTime; i++) {
      for (int ii = lround(realSleepTime); ii>0; ii - (1000000 -1)) { //Sleep all the time off in an in-game second
        usleep(ii%(1000000-1));
      }
    }
  }
}

int compDishes(const void* x, const void* y) {
  Dish* a = (Dish*)x;
  Dish*b = (Dish*)y;
  if (a->time != b-> time) return a->time - b->time; //Faster dishes are better, as they contribute to score loss equally
  return b->price - a->price; //More expensive plates have priority
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
  sem_t* orderTableMuts = args->orderTableMuts;
  int* txServing = args->txServing;

  free(arg);
  bool runFlag = true;
  
  while (runFlag); {
    //Grab a new customer if available
    int newClient = -1;
    if (read(rxArrival, &newClient, sizeof(newClient)) != -1) {
      if (newClient != -1) {
        sem_wait(&orderTableMuts[newClient]); //Must make sure no one is messing with the order memory (like freeing) while reading
        //Get all dishes in order to convert into indexes
        int dishesCount = orderTable[newClient]->count;
        Dish* dishes[dishesCount];
        for (int i = 0; i<dishesCount; i++) {
          dishes[i] = orderTable[newClient]->dishList[i].dish;
        }
        sem_post(&orderTableMuts[newClient]);

        //Get an order of assignment by sorting over price
        qsort(dishes, dishesCount, sizeof(Dish*), compDishes);

        //Map dishes to their index
        int dishMap[dishesCount];
        //Iterate on all possible dishes, matching any element in the array to it
        for (int i = 0; i<menu.dishCount; i++) {
          for (int ii = 0; ii<dishesCount; ii++) {
            if (dishes[ii]->name == menu.dishes[i].name) dishMap[ii] = i;
          }
        }

        for (int i = 0; i<dishesCount; i++) { //Iterate over the sorted mapping
          int leastBusyCook = 0;
          int leastVal = atomic_load(&busyTime[leastBusyCook]);
          int currentVal;
          for (int ii = 1; ii<cookCount; ii++) { //Find the least busy cook
            currentVal = atomic_load(&busyTime[ii]);
            if (currentVal < leastVal) {
              leastBusyCook = ii;
              currentVal = leastVal;
            }
          }
          int sendOrder[2] = {dishMap[i], ID}; //Create the dish package
          atomic_fetch_add(&busyTime[leastBusyCook], dishes[i]->time); //Increment the amount of time the cook is going to be busy
          write(txOrders[leastBusyCook], &sendOrder, sizeof(sendOrder)); //Send it
        }
      }
    }
    //Distribute dishes received


    //Entratain guests
  }
}
