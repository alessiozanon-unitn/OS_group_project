#include "restaurant.h"
#include "customer.h"
#include "waiter.h"
#include "cook.h"
#include "kitchen.h"
#include "menu.h"
#include "order.h"
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include "splitmix64.h"
#include "utils.h"

extern Menu* menu;
atomic_int score;
uint gameSpeed;

int main(){

  // Loads env variables
  const uint cookCount = atoi(getenv("NUM_COOKS"));
  const uint waiterCount = atoi(getenv("NUM_WAITERS"));
  const uint maxCustomers = atoi(getenv("MAX_CUSTOMERS"));
  const uint totalCustomers = atoi(getenv("TOTAL_CUSTOMERS"));
  const uint randomSeed = atoi(getenv("RANDOM_SEED"));
  const u_long gameSpeed = atoi(getenv("GAME_SPEED"));
  const char* menu_file = getenv("MENU_FILE");
  const char* resources_file = getenv("RESOURCES_FILE");
  const int max_dishes_per_order = atoi(getenv("MAX_DISHES_PER_ORDER"));
  const int patience_level_range = atoi(getenv("PATIENCE_LEVEL_RANGE"));

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

  Order* orderTable[maxCustomers];
  sem_t orderTableMuts[maxCustomers];
  atomic_time* arrivalTimeMatcher[maxCustomers];
  for (int i = 0; i<maxCustomers; i++) {
    orderTable[i] = NULL;
    sem_init(&orderTableMuts[i], 0, 1);
    arrivalTimeMatcher[i] = malloc(sizeof(atomic_time));
    atomic_store(arrivalTimeMatcher[i], 0);
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
    waiterArgs[i]->orderTableMuts = orderTableMuts;
    waiterArgs[i]->arrivalTimeMatcher = arrivalTimeMatcher;
    waiterArgs[i]->txServing = servingSenders;
  }



  //Start cooks
  for (int i = 0; i<cookCount; i++) {
    if(pthread_create(&Cooks[i], NULL, cook, (void*) cookArgs[i]) != 0){
      fprintf(stderr, "Could not create cook n.%d's thread", i);
    }
  }

  //Start waiters
  for (int i = 0; i<waiterCount; i++) {
    if(pthread_create(&Waiters[i], NULL, waiter, (void*) waiterArgs[i]) != 0){
      fprintf(stderr, "Could not create waiter n.%d's thread", i);
    }
  }

  // Customers

  int customersSent = 0;

  // Customers loop
  while (customersSent < totalCustomers) {

    // Check if there is any free slot and in case spawns a new customer
    for(int i=0;i<maxCustomers;i++){
      if (orderTable[i] == NULL){

        // Sets new customer args
        CustomerArg *customerArgs = malloc(sizeof(CustomerArg)); // Are freed by the customer using it
        customerArgs->seed[0] = next_splitmix64();
        customerArgs->seed[1] = next_splitmix64();
        customerArgs->seed[2] = next_splitmix64();
        customerArgs->seed[3] = next_splitmix64();
        customerArgs->orderSlot = orderTable[i];
        sem_init(customerArgs->slotMut, 0, 1);
        customerArgs->rxServing = servingPipes[i][0];
        customerArgs->tableNumber = i;
        customerArgs->txArrival = arrivalPipe[1];
        customerArgs->max_dishes_per_order = max_dishes_per_order;
        customerArgs->patience_level_range = patience_level_range;

        // Spawns new customer
        returnCustomers[i] = pthread_create(&Customers[i], NULL, &customer, (void*) customerArgs);
        customersSent++;
      }
    }
    //Check if signals received TODO
    //Update status brief TODO
  }

  //Wait for all clients to leave and joins them
  for(int i=0;i<maxCustomers;i++){
    void *return_value;
    pthread_join(Customers[i], &return_value);
    returnCustomers[i] = (int)(intptr_t) return_value;
  }

  //Joins cooks
  for(int i=0;i<cookCount;i++){
    void *return_value;
    pthread_join(Cooks[i], &return_value);
    returnCooks[i] = (int)(intptr_t) return_value;
  }

  //Joins waiters
  for(int i=0;i<waiterCount;i++){
    void *return_value;
    pthread_join(Waiters[i], &return_value);
    returnWaiters[i] = (int)(intptr_t) return_value;
  }


  return 0;
}
