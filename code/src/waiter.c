#include "waiter.h"
#include "menu.h"
#include "restaurant.h"
#include "errorcodes.h"

#include "xoshiro256plusplus.h"

#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <time.h>
#include <errno.h>

extern Menu menu;
extern atomic_int score;
extern double gameSpeed;

const int ESUCCESSWEIGHT = 3;   //>=0, the entratainment adds some value
const int EFAILWEIGHT = 16;      //>=0, the entratainment does nothing
const int ECRITFAILWEIGHT = 1;  //>=0, the entratainment subtracts some value
const int MINSUCCESS = 30;       //How much a success will contribute at least (patience-minutes)
const int MAXSUCCESS = 60;       //How much a success can contribute at most (patience-minutes)
const int MINCFAIL = 15;         //How much a critical fail can detract at least (positive)
const int MAXCFAIL = 60;         //How much a critical fail can detract at most (positive)

Order** orderTable; //Must be global because qsort_r is a feature test macro, thus not as portable as required
atomic_time** arrivalTimeMatcher; //Used to make sure the client hasn't left and been replaced

void waiterStop(ErrorVals errornumber) {
  pthread_exit((void*) errornumber);
}

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
  Dish* b = (Dish*)y;
  if (a->time != b-> time) return a->time - b->time; //Faster dishes are better, as they contribute to score loss equally
  return b->price - a->price; //More expensive plates have priority
}

int compClients(const void* x, const void*y) {
  int a = *(int*)x;
  int b = *(int*)y;

  if ((orderTable[a] == NULL || orderTable[a]->arrivalTime == atomic_load(arrivalTimeMatcher[a])) && (orderTable[b] == NULL || orderTable[b]->arrivalTime == atomic_load(arrivalTimeMatcher[b]))) return 0; //If both are null, doesn't matter 
  if (orderTable[a] == NULL || orderTable[a]->arrivalTime == atomic_load(arrivalTimeMatcher[a])) return -1; //If client "a" left, client "b" has better priority
  if (orderTable[b] == NULL || orderTable[b]->arrivalTime == atomic_load(arrivalTimeMatcher[b])) return 1; //Effectively, I shunt leavers and duplicates to the right of the array
  
  //Both are here, no-problem comparison
  a = atomic_load(&orderTable[*(int*)x]->patienceLevel);
  b = atomic_load(&orderTable[*(int*)y]->patienceLevel);
  return a-b;
}

typedef struct dishReceipt {
  int size;
  int* clientIndexes;
} dishReceipt;

void addReceipt(dishReceipt* queue, int dish) {
  if (queue == NULL) return;

  if (queue->size == 0) {
    queue->clientIndexes = malloc(sizeof(int));
    if (queue->clientIndexes == NULL) waiterStop(MALLOC_FAIL);
  } else if ((queue->size & (queue->size -1)) == 0) { //Test if size is a power of two, if so then expand array 
    queue->clientIndexes = realloc(queue->clientIndexes, sizeof(int)*2*queue->size);
    if (queue->clientIndexes == NULL && errno == ENOMEM) waiterStop(MALLOC_FAIL);
  }
  queue->clientIndexes[queue->size] = dish;
  queue->size++; //size acts as the index of the first free slot
}

void rmReceipt(dishReceipt* queue, int index) {
  if (queue == NULL) return;
  
  if (index < 0) return; //Not valid index
  if (queue->size <= index) return; //Not valid index
  queue->size--; //size is now the index of the last valid value
  if (queue->size == 0) {
    free(queue->clientIndexes);
    queue->clientIndexes = NULL;
    return;
  }
  
  for (int i = index; i<queue->size; i++) { //Move all the elements after index left
    queue->clientIndexes[i] = queue->clientIndexes[i+1];
  }

  if ((queue->size & (queue->size-1)) == 0) { //If size is a power of two, half of the array is empty 
    queue->clientIndexes = realloc(queue->clientIndexes, sizeof(int)*queue->size);
    if (queue->clientIndexes == NULL && errno == ENOMEM) waiterStop(MALLOC_FAIL);
  }
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
  orderTable = args->orderTable; //Global
  sem_t** orderTableMuts = args->orderTableMuts;
  atomic_time** threadArrivalTimeMatcher = args->arrivalTimeMatcher;
  int* txServing = args->txServing;

  free(arg);
  bool runFlag = true;
  dishReceipt receipts[menu.dishCount]; //Keep track of what ordered for who
  for (int i = 0; i<menu.dishCount; i++) {
    receipts[i] = (dishReceipt){.size = 0, .clientIndexes = NULL}; //Init the fields
  }
  time_t localArrivalMatcher[customerCount];
  for (int i = 0; i<customerCount; i++) {
    arrivalTimeMatcher[i] = threadArrivalTimeMatcher[i];
    localArrivalMatcher[i] = 0;
  }
  
  while (runFlag) {
    //Grab new customers if available
    int newClients[customerCount];
    int newCount = 0;

    int readret = 0;
    do {
      readret = read(rxArrival, &newClients[newCount], sizeof(int));
      if (readret == -1 && errno != EAGAIN) waiterStop(READ_FAIL); 
      if (readret > 0) newCount++;
    } while (readret != 1);
    
    if (newCount != 0) {
      for (int i = 0; i<newCount; i++) {
        int semret = 0;
        do {
          semret = sem_wait(orderTableMuts[newClients[i]]); //Must make sure no one is messing with the order memory (like freeing) while reading
          if (semret == -1 && errno != EINTR) waiterStop(SEMAPHORE_FAIL);
        } while (semret != 0);
      }
      
      //Prioritize the customers with the lowest patience level (by sorting the array)
      qsort(newClients, newCount, sizeof(int), compClients);

      int offset = 0;
      for (int i = 0; i<newCount; i++) { //Count the clients that left in the queue
        if (orderTable[newClients[i]] == NULL || orderTable[i]->arrivalTime == atomic_load(arrivalTimeMatcher[newClients[i]])) offset++; //If the arrival time matches the last recorded, it means that a duplicate snuck through and it's discardable
      }

      newCount -= offset; //Shorten the array "size" by that amount, since the leavers had been shoved right

      for (int i = 0; i<newCount; i++) {
        atomic_store(arrivalTimeMatcher[newClients[i]], orderTable[newClients[i]]->arrivalTime); //The new arrivals are accepted and their time stored
        localArrivalMatcher[newClients[i]] = orderTable[newClients[i]]->arrivalTime; //Local array is updated too
        sem_post(orderTableMuts[newClients[i]]); //Temporarily free the customers
      }

      int newClient; //A single of the new clients
      for (int clientScroller = 0; clientScroller<newCount; clientScroller++) {
        newClient = newClients[clientScroller]; //Do them in order of the sorted array
        int semret = 0;
        do {
          semret = sem_wait(orderTableMuts[newClient]); //Re-lock currently used customer
          if (semret == -1 && errno != EINTR) waiterStop(SEMAPHORE_FAIL);
        } while (semret != 0);
        
        if (orderTable[newClient] != NULL && localArrivalMatcher[newClient] == orderTable[newClient]->arrivalTime) { //Do nothing if the client left in the short break of freedom (use local copy so of the client was replaced by another waiter this fails)
          //Get all dishes in order to convert into indexes
          int dishesCount = orderTable[newClient]->count;
          Dish* dishes[dishesCount];
          for (int i = 0; i<dishesCount; i++) {
            dishes[i] = orderTable[newClient]->dishList[i].dish;
          }
          sem_post(orderTableMuts[newClient]);
          
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
            addReceipt(&receipts[dishMap[i]], newClient); //Add the client to the receipts of the dish
            atomic_fetch_add(&busyTime[leastBusyCook], dishes[i]->time); //Increment the amount of time the cook is going to be busy
            
            int retwrite = 0;
            do {
              retwrite = write(txOrders[leastBusyCook], &sendOrder, sizeof(sendOrder)); //Send it
              if (retwrite == -1 && errno != EINTR) waiterStop(WRITE_FAIL);
            } while (retwrite != 0);
          }
        }
      }
    }

    //Distribute dishes received
    int dishIndex = 0;
    readret = 0;
    do {
      readret = read(rxDishes, &dishIndex, sizeof(dishIndex));
      if (readret == -1 && errno != EAGAIN) waiterStop(READ_FAIL);
      if (readret > 0) {
        //Waiter has a plate, do something with it
        bool deliveredFlag = false;
        while (!deliveredFlag && receipts[dishIndex].size > 0) {//Try to deliver a dish as long as there's someone to accept it
          int currentCustomer = receipts[dishIndex].clientIndexes[0]; //Leftmost client is the oldest one
          rmReceipt(&receipts[dishIndex], 0); //Remove receipt, either delivered or deprecated
        
          int semret = 0;
          do {
            semret = sem_wait(orderTableMuts[currentCustomer]); //Lock the order for operations
            if (semret == -1 && errno != EINTR) waiterStop(SEMAPHORE_FAIL);
          } while (semret != 0);

          if (orderTable[currentCustomer] != NULL && localArrivalMatcher[currentCustomer] == orderTable[currentCustomer]->arrivalTime) { //If the client hasn't left (use local array again)
            int writeret = 0;
            do {
              writeret = write(txServing[currentCustomer], &dishIndex, sizeof(dishIndex));
              if (writeret == -1 && errno != EINTR) waiterStop(WRITE_FAIL);
            } while (writeret != 0);
  
              //Dish is now delivered;
          }
          sem_post(orderTableMuts[currentCustomer]); //Nothing else to do on the orderTable for now
        }
      }
    } while (readret != -1);


    //Entratainment 
    if (ESUCCESSWEIGHT+ECRITFAILWEIGHT <= 0) { //If no chance of something happening, nothing to do here
      //Find a client to entratain
      int grumpiestCustomer = -1;
      int patience = -1;
      for (int i = 0; i<customerCount; i++) {
        int semret = 0;
        do {
          semret = sem_wait(orderTableMuts[i]);
          if (semret == -1 && errno != EINTR) waiterStop(SEMAPHORE_FAIL);
        } while (semret != 0);

        int currentPatience = -1;
        if (orderTable[i] != NULL) currentPatience = atomic_load(&orderTable[i]->patienceLevel);
        sem_post(orderTableMuts[i]);
        if (currentPatience != -1 && (grumpiestCustomer == -1 || currentPatience < patience)) { //If you got a valid read, and no customer was found or the previous find is better-off than this one 
          grumpiestCustomer = i;
          patience = currentPatience;
        }
      }

      //Entratain guests
      if (grumpiestCustomer != -1) {//Someone was found
        int semret = 0;
        do {
          semret = sem_wait(orderTableMuts[grumpiestCustomer]); //Make sure the customer doesn't pull the carpet under the waiter
          if (semret == -1 && errno != EINTR) waiterStop(SEMAPHORE_FAIL);
        } while (semret != 0);

        if (orderTable[grumpiestCustomer] != NULL && orderTable[grumpiestCustomer]->arrivalTime == atomic_load(arrivalTimeMatcher[grumpiestCustomer])) { //And the client is even still there
          int roll = next(seed)%(ESUCCESSWEIGHT+EFAILWEIGHT+ECRITFAILWEIGHT); //Roll is in the bounds of all chances
          if (roll < ECRITFAILWEIGHT) { //Rolled bad enough to get a critical fail
            //Detract score
            int roll = next(seed)%(MAXCFAIL - MINCFAIL)+MINCFAIL;
            atomic_fetch_sub(&orderTable[grumpiestCustomer], roll);
          } else if (roll >= ECRITFAILWEIGHT+EFAILWEIGHT) {//Rolled good enough to get a success
            //Increase score
            int roll = next(seed)%(MAXSUCCESS - MINSUCCESS)+MINSUCCESS;
            atomic_fetch_add(&orderTable[grumpiestCustomer], roll);
          } //Otherwise nothing happens
        }
        sem_post(orderTableMuts[grumpiestCustomer]); //Client is no longer hostage
      }
    }
  }
}
