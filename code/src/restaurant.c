#include "restaurant.h"
#include "customer.h"
#include "waiter.h"
#include "cook.h"
#include "kitchen.h"
#include "menu.h"
#include "order.h"

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

Menu* menu;

int main(){
  
  //TODO temp contnsts, should be read as arguments later
  const int cookCount = 3;
  const int waiterCount = 3;
  const int maxCustomers = 10;

  //Kitchen and menu preparation TODO
  Kitchen* kitchen = malloc(sizeof(Kitchen*));


  //PID holders
  pthread_t Cooks[cookCount];
  pthread_t Waiters[waiterCount];
  pthread_t Customers[maxCustomers];

  //Returns
  int returnCooks[cookCount];
  int returnWaiters[waiterCount];
  int returnCustomers[maxCustomers]; 
  
  //Pipes
  int ordersPipes[cookCount][2];
  int dishesPipes[waiterCount][2];
  int servingPipes[maxCustomers][2];

  //Initializing pipes
  for (int i = 0; i<cookCount; i++) {
    if (pipe(ordersPipes[i]) == -1) {
      //Pipe error
    }
  }

  for (int i = 0; i<waiterCount; i++) {
    if (pipe(dishesPipes[i]) == -1) {
      //Pipe error
    }
  }

  for (int i = 0; i<maxCustomers; i++) {
    if (pipe(servingPipes[i]) == -1) {
      //Pipe error
    }
  }

  //sender arrays
  int orderSenders[cookCount];
  for (int i = 0; i<cookCount; i++) {
    orderSenders[i] = ordersPipes[i][1];
  }

  int dishSenders[waiterCount];
  for (int i = 0; i<waiterCount; i++) {
    dishSenders[i] = dishesPipes[i][1];
  }
  
  int servingSenders[maxCustomers];
  for (int i = 0; i<maxCustomers; i++) {
    servingSenders[i] = servingPipes[i][1];
  }


  //Shared arrays
  int busyTime[cookCount];
  for (int i = 0; i<cookCount; i++) {
    busyTime[i] = 0;
  }

  Order* orderTable[maxCustomers];
  for (int i = 0; i<maxCustomers; i++) {
    orderTable[i] = NULL;
  }

  //Argument arrays (client done later in repeating section)
  CookArg* cookArgs[cookCount];
  WaiterArg* waiterArgs[waiterCount];
  
  for (int i = 0; i<cookCount; i++) {
    cookArgs[i] = malloc(sizeof(CookArg*));
    cookArgs[i]->randSeed = rand();
    cookArgs[i]->kitchen = kitchen;
    cookArgs[i]->rxOrders = ordersPipes[i][0];
    cookArgs[i]->txDishes = dishSenders;
    cookArgs[i]->busyTime = &busyTime[i];
  }

  for (int i = 0; i<waiterCount; i++) {
    waiterArgs[i] = malloc(sizeof(WaiterArg*)); 
    waiterArgs[i]->randSeed = rand();
    waiterArgs[i]->ID = i;
    waiterArgs[i]->cookCount = cookCount;
    waiterArgs[i]->txOrders = orderSenders;
    waiterArgs[i]->rxDishes = dishesPipes[i][0];
    waiterArgs[i]->busyTime = busyTime;
    waiterArgs[i]->customerCount = maxCustomers;
    waiterArgs[i]->orderTable = orderTable;
    waiterArgs[i]->txServing = servingSenders;
  }

  return 0;
}
