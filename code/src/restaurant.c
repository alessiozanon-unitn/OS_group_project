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
#include <signal.h>
#include "errorcodes.h"

const int SLEEP_MULT = 1; //How many in-game minutes to sleep between loops of the customer deployment, to encourage or discourage traffic spikes

Menu menu;
atomic_int score;
double gameSpeed;

//Globals to allow access by signal handler
int totalCustomers = 0;
uint cookCount;
atomic_int** busyTime;
Kitchen* kitchen;
atomic_int** customerStatuses;

void status(int signum) {
  int sent = 0;
  int disappointment = 0;
  int waiting = 0;
  int dead = 0;
  for (int i = 0; i<totalCustomers; i++) {
    int current = atomic_load(customerStatuses[i]);
    if (current != UNSENT) {
      sent++;
      if (current == WAITING) waiting++;
      else if (current == UNSATISFIED) disappointment++;
      else if (current == ERROR) dead++;
    }
  }

  printf("Score:\t%d\nCurrent customers:\t%d\nUnsatisfied customers:\t%d\nErrored customers:\t%d\nProgress:\t%d/%d\n", atomic_load(&score), waiting, disappointment, dead, sent, totalCustomers);
  for (int i = 0; i<cookCount; i++) {
    printf("Cook%d's queue size:\t%d\n", i, atomic_load(busyTime[i]));
  }
  for (int i = 0; i<kitchen->resourceCount; i++) {
    int clean;
    int dirty;
    sem_getvalue(&kitchen->resources[i].clean, &clean);
    sem_getvalue(&kitchen->resources[i].dirty, &dirty);
    printf("%s status:\t%d Clean\t%d Dirty\n", kitchen->resources[i].name, clean, dirty);
  }
  printf("\n\n\n");
}

int main(){

  // Loads env variables
  cookCount = atoi(getenv("NUM_COOKS"));
  const uint waiterCount = atoi(getenv("NUM_WAITERS"));
  const uint maxCustomers = atoi(getenv("MAX_CUSTOMERS"));
  totalCustomers = atoi(getenv("TOTAL_CUSTOMERS"));
  const uint randomSeed = atoi(getenv("RANDOM_SEED"));
  gameSpeed = atof(getenv("GAME_SPEED"));
  const char* menu_file = getenv("MENU_FILE");
  const char* resources_file = getenv("RESOURCES_FILE");
  const int max_dishes_per_order = atoi(getenv("MAX_DISHES_PER_ORDER"));
  const int patience_level_range = atoi(getenv("PATIENCE_LEVEL_RANGE"));

  int pidHolder = creat("/tmp/restaurant.pid", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (pidHolder < 0) {
    perror("Could not crate PID file in tmp");
    return FILE_FAIL;
  } else {
    pid_t selfPID = getpid();
    int retval = dprintf(pidHolder, "%d", selfPID);
    close(pidHolder);
    if (retval <= -1) return WRITE_FAIL;
  }

  // Splitmix state variable is initialized through the env variable randomSeed
  // and then used to initialize the state of the threads' states to use with xoshiro's generator
  x_splitmix64 = randomSeed;

  //Prepare the signal handling system
  struct sigaction act;
  act.sa_handler = status;
  act.sa_flags = SA_RESTART; //Saves up a lot of code on R/W EINTR

  sigset_t usr1;
  sigemptyset(&usr1);
  sigaddset(&usr1, SIGUSR1);

  //Mask incoming user signals while not ready to handle them, so they get queued (exploits the inheritance to also initialize all)
  pthread_sigmask(SIG_BLOCK, &usr1, NULL);

  // Kitchen initalization
  kitchen = malloc(sizeof(Kitchen));
  if (kitchen == NULL) {
    perror("Could not allocate kitchen space");
    return MALLOC_FAIL;
  }
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
  pthread_t Customers[totalCustomers];

  //Run-status holders
  atomic_bool** cookRun = calloc(cookCount, sizeof(atomic_bool*));
  if (cookRun == NULL) {
    perror("Failed to create cook running status array");
    return MALLOC_FAIL;
  } else for (int i = 0; i<cookCount; i++) {
    cookRun[i] = malloc(sizeof(atomic_bool));
    if (cookRun[i] == NULL) {
      perror("Failed to create cook running status element");
      return MALLOC_FAIL;
    } else atomic_init(cookRun[i], true);
  }

  atomic_bool** waiterRun = calloc(waiterCount, sizeof(atomic_bool*));
  if (waiterRun == NULL) {
    perror("Failed to create waiter running status array");
    return MALLOC_FAIL;
  } else for (int i = 0; i<waiterCount; i++) {
    waiterRun[i] = malloc(sizeof(atomic_bool));
    if (waiterRun == NULL) {
      perror("Failed to create waiter running status array");
      return MALLOC_FAIL;
    } else atomic_init(waiterRun[i], true);
  }

  //Pipes
  int ordersPipes[cookCount][2];
  int dishesPipes[waiterCount][2];
  int servingPipes[maxCustomers][2];
  int arrivalPipe[2];

  //Initializing pipes
  for (int i = 0; i<cookCount; i++) {
    if (pipe(ordersPipes[i]) < 0) {
      perror("Could not create order Pipes");
      return PIPE_FAIL;
    } else {
      fcntl(ordersPipes[i][0], F_SETFL, O_NONBLOCK);
    }
  }

  for (int i = 0; i<waiterCount; i++) {
    if (pipe(dishesPipes[i]) < 0) {
      perror("Could not create dish Pipes");
      return PIPE_FAIL;
    } else {
      fcntl(dishesPipes[i][0], F_SETFL, O_NONBLOCK);
    }
  }

  for (int i = 0; i<maxCustomers; i++) {
    if (pipe(servingPipes[i]) < 0) {
      perror("Could not create serving Pipes");
      return PIPE_FAIL;
    } else {
      fcntl(servingPipes[i][0], F_SETFL, O_NONBLOCK);
    }
  }

  if (pipe(arrivalPipe) < 0) {
    perror("Could not create arrival Pipe");
    return PIPE_FAIL;
  }

  fcntl(arrivalPipe[0], F_SETFL, O_NONBLOCK);

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
  busyTime = calloc(cookCount, sizeof(atomic_int*));
  if (busyTime == NULL) {
    perror("Failed to create busyTime array");
    return MALLOC_FAIL;
  } else for (int i = 0; i<cookCount; i++) {
    busyTime[i] = malloc(sizeof(atomic_int));
    if (busyTime[i] == NULL) {
      perror("Could not generate busyTime element");
      return MALLOC_FAIL;
    } else atomic_init(busyTime[i], 0);
  }

  Order*** orderTable = calloc(maxCustomers, sizeof(Order**));
  if (orderTable == NULL) {
    perror("Could not create orderTable array");
    return MALLOC_FAIL;
  }
  sem_t** orderTableMuts = calloc(maxCustomers, sizeof(sem_t*));
  if (orderTableMuts == NULL) {
    perror("Could not create orderTable mutex array");
    return MALLOC_FAIL;
  }
  atomic_time** arrivalTimeMatcher = calloc(maxCustomers, sizeof(atomic_time*));
  if (arrivalTimeMatcher == NULL) {
    perror("Could not create shared arrivalTimeMatcher array");
    return MALLOC_FAIL;
  }
  for (int i = 0; i<maxCustomers; i++) {
    orderTable[i] = malloc(sizeof(Order*));
    if (orderTable[i] == NULL) {
      perror("Could not create order table pointer element");
      return MALLOC_FAIL;
    } else orderTableMuts[i] = malloc(sizeof(sem_t));
    if (orderTableMuts[i] == NULL) {
      perror("Could not create mutex element for the order table array");
      return MALLOC_FAIL;
    } else sem_init(orderTableMuts[i], 0, 1);
    arrivalTimeMatcher[i] = malloc(sizeof(atomic_time));
    if (arrivalTimeMatcher[i] == NULL) {
      perror("Could not create arrival time-match storage element");
      return MALLOC_FAIL;
    } else atomic_init(arrivalTimeMatcher[i], 0);
  }

  customerStatuses = calloc(totalCustomers, sizeof(atomic_int*));
  if (customerStatuses == NULL) {
    perror("Could not generate customer status array");
    return MALLOC_FAIL;
  } else for (int i = 0; i<totalCustomers; i++) {
    customerStatuses[i] = malloc(sizeof(atomic_int));
    if (customerStatuses[i] == NULL) {
      perror("Could not create customer status element");
      return MALLOC_FAIL;
    } else atomic_init(customerStatuses[i], UNSENT);
  }

  //Argument arrays (client done later in repeating section)
  CookArg** cookArgs = calloc(cookCount, sizeof(CookArg*));
  if (cookArgs == NULL) {
    perror("Could not generate cook arguments array");
    return MALLOC_FAIL;
  }
  WaiterArg** waiterArgs = calloc(waiterCount, sizeof(WaiterArg*));
  if (waiterArgs == NULL) {
    perror("Could not generate waiter arguments array");
    return MALLOC_FAIL;
  }

  for (int i = 0; i<cookCount; i++) {
    cookArgs[i] = malloc(sizeof(CookArg));
    if (cookArgs[i] == NULL) {
      perror("Failed to allocate cook argument element");
      return MALLOC_FAIL;
    }
    cookArgs[i]->run = cookRun[i];
    cookArgs[i]->seed[0] = next_splitmix64();
    cookArgs[i]->seed[1] = next_splitmix64();
    cookArgs[i]->seed[2] = next_splitmix64();
    cookArgs[i]->seed[3] = next_splitmix64();
    cookArgs[i]->kitchen = kitchen;
    cookArgs[i]->rxOrders = ordersPipes[i][0];
    cookArgs[i]->txDishes = dishSenders;
    cookArgs[i]->busyTime = busyTime[i];
  }

  for (int i = 0; i<waiterCount; i++) {
    waiterArgs[i] = malloc(sizeof(WaiterArg));
    if (waiterArgs[i] == NULL) {
      perror("Failed to allocate waiter argument element");
      return MALLOC_FAIL;
    }
    waiterArgs[i]->run = waiterRun[i];
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
      perror("Could not create cook thread");
      return THREAD_FAIL;
    }
  }

  //Start waiters
  for (int i = 0; i<waiterCount; i++) {
    if(pthread_create(&Waiters[i], NULL, waiter, (void*) waiterArgs[i]) != 0){
      perror("Could not create waiter thread");
      return THREAD_FAIL;
    }
  }

  // Customers
  int customersSent = 0;

  //Attach signal handler and enable SIGUSR1
  if (sigaction(SIGUSR1, &act, NULL) != 0) {
    perror("Failed to set up sigaction");
    return THREAD_FAIL;
  }
  pthread_sigmask(SIG_UNBLOCK, &usr1, NULL);

  int currentActives[maxCustomers]; //Local
  for (int i = 0; i<maxCustomers; i++) {
    currentActives[i] = -1; //No customer here value
  }

  time_t startTime = time(NULL);

  // Customers loop
  for (int loopCount = 0; customersSent < totalCustomers; loopCount++) {
    int deployedCustomers = 0;
    printf("Cycle %d(%ds)\n", loopCount, time(NULL)-startTime);
    //Check over all slots, making sure to stop if all customers to send have been
    for(int i = 0; i<maxCustomers && customersSent < totalCustomers; i++) {
      if(
        currentActives[i] == -1 || ( //If the slot is empty or
          atomic_load(customerStatuses[currentActives[i]]) != WAITING && //The customer is not waiting and
          atomic_load(customerStatuses[currentActives[i]]) != UNSENT //The customer is not unsent
        //I.E the customer is either satisfied, unsatisfied, or errored
        )
      ) { //Can be replaced
        // Sets new customer args
        CustomerArg* customerArgs = malloc(sizeof(CustomerArg)); // Are freed by the customer using it
        if (customerArgs == NULL) {
          perror("Could not allocate customer argument space");
          return MALLOC_FAIL;
        }
        customerArgs->seed[0] = next_splitmix64();
        customerArgs->seed[1] = next_splitmix64();
        customerArgs->seed[2] = next_splitmix64();
        customerArgs->seed[3] = next_splitmix64();
        customerArgs->status = customerStatuses[customersSent];
        customerArgs->orderSlot = orderTable[i];
        customerArgs->slotMut = orderTableMuts[i];
        customerArgs->rxServing = servingPipes[i][0];
        customerArgs->tableNumber = i;
        customerArgs->txArrival = arrivalPipe[1];
        customerArgs->max_dishes_per_order = max_dishes_per_order;
        customerArgs->patience_level_range = patience_level_range;

        //Temporarily block SIGUSR1
        pthread_sigmask(SIG_BLOCK, &usr1, NULL);
        // Spawns new customer
        if (pthread_create(&Customers[customersSent], NULL, &customer, (void*) customerArgs) != 0) {
          perror("Could not create customer thread");
          return THREAD_FAIL;
        }
        pthread_sigmask(SIG_UNBLOCK, &usr1, NULL);

        currentActives[i] = customersSent;
        customersSent++;
      }
      if (currentActives[i] != -1) deployedCustomers++; //Count the current customers in the restaurant
    }

    status(0);
    printf("Score:\t%d\nDeployed Customers:\t%d\nCustomer progress:\t%d\t%f%%\n\n\n", atomic_load(&score), deployedCustomers, customersSent,(double)customersSent/totalCustomers*100.0);
    for (int i = 0; i<SLEEP_MULT; i++) custom_sleep();
  }

  //Wait for all clients to leave and joins them
  int returnCustomers[totalCustomers];
  for(int i=0;i<totalCustomers;i++){
    void *return_value;
    pthread_join(Customers[i], &return_value);
    returnCustomers[i] = (int)(intptr_t) return_value;
  }

  int returnCooks[cookCount];
  //Joins cooks
  for(int i=0;i<cookCount;i++){
    atomic_store(cookRun[i], false);
    void *return_value;
    pthread_join(Cooks[i], &return_value);
    returnCooks[i] = (int)(intptr_t) return_value;
  }

  int returnWaiters[waiterCount];
  //Joins waiters
  for(int i=0;i<waiterCount;i++){
    atomic_store(waiterRun[i], false);
    void *return_value;
    pthread_join(Waiters[i], &return_value);
    returnWaiters[i] = (int)(intptr_t) return_value;
  }

  remove("/tmp/restaurant.pid"); //Remove PID file

  return ALL_OK;
}
