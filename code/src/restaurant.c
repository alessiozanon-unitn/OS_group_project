#include "restaurant.h"
#include "customer.h"
#include "waiter.h"
#include "cook.h"
#include "kitchen.h"
#include "menu.h"
#include "order.h"
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include "splitmix64.h"
#include "utils.h"

Menu* menu;
atomic_int score;
uint gameSpeed;

int main(){

  // Loads env variables
  const uint cookCount = atoi(getenv("NUM_COOKS"));
  const uint waiterCount = atoi(getenv("NUM_WAITERS"));
  const uint maxCustomers = atoi(getenv("MAX_CUSTOMERS"));
  const uint totalCustomers = atoi(getenv("TOTAL_CUSTOMERS"));
  const uint randomSeed = atoi(getenv("RANDOM_SEED"));
  gameSpeed = atoi(getenv("GAME_SPEED"));
  const char* menu_file = getenv("MENU_FILE");
  const char* resources_file = getenv("RESOURCES_FILE");

  // Splitmix state variable is initialized through the env variable randomSeed
  // and then used to initialize the state of the threads' states to use with xoshiro's generator
  x_splitmix64 = randomSeed;


  // Kitchen initalization
  Kitchen* kitchen = malloc(sizeof(Kitchen));
  kitchen->resourceCount = 0;
  kitchen->resources = NULL;
  sem_init(&kitchen->sink, 0, 1);

  // Loads resources to kitchen from file
  load_resources_from(resources_file, kitchen);

  // Loads menu from the csv
  load_menu_from(menu_file, kitchen);

  // For debugging
  //print_kitchen(kitchen);
  // print_menu(menu);



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
  int arrivalPipe[2];

  //Initializing pipes
  for (int i = 0; i<cookCount; i++) {
    if (pipe(ordersPipes[i]) < 0) {
      //Pipe error
    } else {
      fcntl(ordersPipes[i][0], F_SETFL, O_NONBLOCK);
    }
  }

  for (int i = 0; i<waiterCount; i++) {
    if (pipe(dishesPipes[i]) < 0) {
      //Pipe error
    } else {
      fcntl(dishesPipes[i][0], F_SETFL, O_NONBLOCK);
    }
  }

  for (int i = 0; i<maxCustomers; i++) {
    if (pipe(servingPipes[i]) < 0) {
      //Pipe error
    } else {
      fcntl(servingPipes[i][0], F_SETFL, O_NONBLOCK);
    }
  }

  if (pipe(arrivalPipe) < 0) {
    //Pipe error
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
  atomic_int busyTime[cookCount];
  for (int i = 0; i<cookCount; i++) {
    atomic_store(&busyTime[i], 0);
  }

  atomic_OrderP* orderTable[maxCustomers];
  for (int i = 0; i<maxCustomers; i++) {
    atomic_store(&orderTable[i], NULL);
  }

  //Argument arrays (client done later in repeating section)
  CookArg* cookArgs[cookCount];
  WaiterArg* waiterArgs[waiterCount];
  
  for (int i = 0; i<cookCount; i++) {
    cookArgs[i] = malloc(sizeof(CookArg*));
    cookArgs[i]->seed[0] = next_splitmix64();
    cookArgs[i]->seed[1] = next_splitmix64();
    cookArgs[i]->seed[2] = next_splitmix64();
    cookArgs[i]->seed[3] = next_splitmix64();
    cookArgs[i]->kitchen = kitchen;
    cookArgs[i]->rxOrders = ordersPipes[i][0];
    cookArgs[i]->txDishes = dishSenders;
    cookArgs[i]->busyTime = &busyTime[i];
  }

  for (int i = 0; i<waiterCount; i++) {
    waiterArgs[i] = malloc(sizeof(WaiterArg*));
    waiterArgs[i]->seed[0] = next_splitmix64();
    waiterArgs[i]->seed[1] = next_splitmix64();
    waiterArgs[i]->seed[2] = next_splitmix64();
    waiterArgs[i]->seed[3] = next_splitmix64();
    waiterArgs[i]->ID = i;
    waiterArgs[i]->cookCount = cookCount;
    waiterArgs[i]->txOrders = orderSenders;
    waiterArgs[i]->rxDishes = dishesPipes[i][0];
    waiterArgs[i]->busyTime = busyTime;
    waiterArgs[i]->customerCount = maxCustomers;
    waiterArgs[i]->rxArrival = arrivalPipe[0];
    waiterArgs[i]->orderTable = orderTable;
    waiterArgs[i]->txServing = servingSenders;
  }

  //Utility variables
  int customersSent = 0;

  //Start cooks
  for (int i = 0; i<cookCount; i++) {
    returnCooks[i] = pthread_create(&Cooks[i], NULL, cook, (void*) cookArgs[i]);
  }

  //Start waiters
  for (int i = 0; i<waiterCount; i++) {
    returnWaiters[i] = pthread_create(&Waiters[i], NULL, waiter, (void*) waiterArgs[i]);
  }

  //Clients loop
  while (customersSent < totalCustomers) {
    //Check if client slots are free TODO
    //Check if signals received TODO
    //Update status brief TODO
  }

  //Wait for all clients to leave 

  //Close cooks

  //Close waiters


  return 0;
}
