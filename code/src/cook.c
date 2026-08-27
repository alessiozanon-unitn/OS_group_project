#include "cook.h"
#include "restaurant.h"
#include "menu.h"
#include "kitchen.h"
#include "errorcodes.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>

const int OVERWORK_THRESHOLD = 3;
extern Menu menu;
extern atomic_int score;
extern double gameSpeed;

void cookStop(ErrorVals errornumber) {
  perror("Cook runtime error");
  pthread_exit((void*) errornumber);
}

void cookSleep(int expectedBusy, atomic_int* busyTimePosition) {
  long minute = lround(1000000.0 * 60.0 / gameSpeed);
  for (int i = 0; i<expectedBusy; i++) {
    for (long ii = minute; ii>0; ii -= (1000000-1)) {
      if (ii>=(1000000-1))
          usleep(1000000-1);
      else
        usleep(ii);
    }
    atomic_fetch_sub(busyTimePosition, 1);
  }
}

bool cookDish(Dish* dish, Kitchen* kitchen, atomic_int* busyTimePosition) {
  //grab semaphore resources and do the deed
  //Increase dirty counter and insert new 1s in the array
  //Return true if cooking happened, false otherwise

  int resourcesGot[dish->requiredSize];
  int retval = 0;

  for (int i = 0; i<dish->requiredSize && retval != -1; i++) {
    resourcesGot[i] = 0;
    for (int ii = 0; ii<dish->requiredCount[i] && retval != -1; ii++) { //Try to take as many cleans as possible
      retval = sem_trywait(&kitchen->resources[dish->requiredTypes[i]].clean);
      if (retval != -1) resourcesGot[i]++; //If clean gotten successfully update counter
    }
  }
  if (retval != -1) {//If all resources aquired, can cook
    // Cook
    cookSleep(dish->time, busyTimePosition);

    //Finished cooking
    for (int i = 0; i<dish->requiredSize; i++) { //Free all resource types
      Resource* currentResource = &kitchen->resources[dish->requiredTypes[i]];
      for (int ii = 0; ii<resourcesGot[i]; ii++) { //Signal as many as were acquired
        sem_post(&currentResource->dirty); //Signal the dirty instead of clean
      }
      int semval = 0;
      do {
        semval = sem_wait(&currentResource->dirtyCountersMutex); //Must block to update array
        if (semval == -1 && errno != EINTR) cookStop(SEMAPHORE_FAIL); //A signal can interrupt a wait
      } while (semval != 0);
      for (int ii = currentResource->dirtyDishesCount-1; ii>=0; ii--) { //Shift all array contents right by the new dishes to add
        currentResource->dirtyResourceCounters[ii+resourcesGot[i]] = currentResource->dirtyResourceCounters[ii];
      }
      for (int ii = 0; ii<resourcesGot[i]; ii++) {
        currentResource->dirtyResourceCounters[ii] = 1; //Insert new "used once" dishes
      }
      currentResource->dirtyDishesCount += resourcesGot[i]; //Update counter
    sem_post(&currentResource->dirtyCountersMutex);
    }

    return true;
  } else { //If not all resources acquired, cooking aborted
    for (int i = 0; i<dish->requiredSize; i++) { //Free all resource types
      for (int ii = 0; ii<resourcesGot[i]; ii++) { //Signal as many as were acquired
        sem_post(&kitchen->resources[dish->requiredTypes[i]].clean);
      }
    }
    return false;
  }
}

bool cookDishDirty(Dish* dish, Kitchen* kitchen, atomic_int* busyTimePosition) {
  //attempt cleanCook, return true if that worked
  //On failure of that, cook using the least dirty resources, update evreything accordingly
  int resourcesGotClean[dish->requiredSize];
  int resourcesGotDirty[dish->requiredSize];
  int* dirtyTracker[dish->requiredSize];
  int retval = 0;

  for (int i = 0; i<dish->requiredSize; i++) { //Make sure to initialize all these pointers to NULL for later
    dirtyTracker[i] = NULL;
  }


  for (int i = 0; i<dish->requiredSize && retval != -1; i++) {
    resourcesGotClean[i] = 0;
    for (int ii = 0; ii<dish->requiredCount[i] && retval != -1; ii++) { //Try to take as many cleans as possible
      retval = sem_trywait(&kitchen->resources[dish->requiredTypes[i]].clean);
      if (retval != -1) resourcesGotClean[i]++; //If clean gotten successfully update counter
    }
  }
  retval = 0; //Reset for acquiring dirty dishes

  for (int i = 0; i<dish->requiredSize && retval != -1; i++) {
    resourcesGotDirty[i] = 0;
    if (resourcesGotClean[i] < dish->requiredCount[i]) { //Only get dirty resources if clean ones are not enough
      Resource* currentResource = &kitchen->resources[dish->requiredTypes[i]];
      for (int ii = resourcesGotClean[i]; ii<dish->requiredCount[i] && retval != 1; ii++) { //Try to take as many dirties as needed, by starting with clean amount accounted for
        retval = sem_trywait(&currentResource->dirty);
        if (retval != -1) resourcesGotDirty[i]++;
      }
      if (retval != -1) { //Only worth doing this if all resources necessary are aquired
       int semret = 0;
       do {
          semret = sem_wait(&currentResource->dirtyCountersMutex); //Must block if waiting
          if (semret == -1 && errno != EINTR) cookStop(SEMAPHORE_FAIL);
        } while (semret != 0);
        dirtyTracker[i] = malloc(sizeof(int)*resourcesGotDirty[i]); //Initialize dynamic arrays to track the individual dirty plates
        if (dirtyTracker[i] == NULL && resourcesGotDirty[i] != 0) cookStop(MALLOC_FAIL);

        for (int ii = 0; ii<resourcesGotDirty[i]; ii++) {//Move values into local tracker array, removing them from the shared one
          dirtyTracker[i][ii] = currentResource->dirtyResourceCounters[ii]; //We know that the leftmost values are the least, so we save them
          currentResource->dirtyResourceCounters[ii] = 0; //We empty the values moved, in case there aren't enough to the right to overwrite them
        }
        for (int ii = resourcesGotDirty[i]; ii<currentResource->dirtyDishesCount; ii++) {//all non-involved dishes are moved left
          currentResource->dirtyResourceCounters[ii - resourcesGotDirty[i]] = currentResource->dirtyResourceCounters[ii]; //Move value to the previously cloned slot
          currentResource->dirtyResourceCounters[ii] = 0; //Clear old slot
        }
        currentResource->dirtyDishesCount -= resourcesGotDirty[i];
        sem_post(&currentResource->dirtyCountersMutex);//Finished operating on the tracking array and its counter
      }
    }
  }

  if (retval != -1) {//If all resources aquired, can cook

    //Cook
    cookSleep(dish->time, busyTimePosition);

    //Finished cooking

    //subtract score for dirty resources
    double acc = 0.0f; //Accumulator
    for (int i = 0; i<dish->requiredSize; i++) {
      for (int ii = 0; ii < resourcesGotDirty[i]; ii++) {
        acc += pow(2, (double)dirtyTracker[i][ii]) * log2((double)(1+ kitchen->resources[dish->requiredTypes[i]].clean_time));
      }
    }
    atomic_fetch_sub(&score, round(acc));

    //Free the dirty resources and re-add them to the tracking array
    for (int i = 0; i<dish->requiredSize; i++){
      Resource* currentResource = &kitchen->resources[dish->requiredTypes[i]];
      if (dirtyTracker[i] != NULL || resourcesGotDirty[i] == 0) { //If no dish was tracked, no need to bother
        //Increment use counter for all dirty resources used
        for (int ii = 0; ii<resourcesGotDirty[i]; ii++) {
          dirtyTracker[i][ii]++;
        }

        int semret = 0;
        do {
          semret = sem_wait(&currentResource->dirtyCountersMutex);
          if (semret == -1 && errno != EINTR) cookStop(SEMAPHORE_FAIL);
        } while (semret != 0);

        for (int ii = currentResource->dirtyDishesCount-1; ii>=0; ii--) {//Move all tracked resources left
          currentResource->dirtyResourceCounters[ii+resourcesGotDirty[i]] = currentResource->dirtyResourceCounters[ii];
        }
        int offset = 0; //Secondary counter insertion
        for (int ii = 0; ii<resourcesGotDirty[i]+currentResource->dirtyDishesCount; ii++) { //Insert local tracks keeping the array sorted
          if (
            ii-offset >= resourcesGotDirty[i] || ( //If local tracked array is depleted
              offset < currentResource->dirtyDishesCount && //Remote tracked array is not depleted
              dirtyTracker[i][ii - offset] > currentResource->dirtyResourceCounters[resourcesGotDirty[i]+offset] //Remote track value is smaller than the local tracked one
            )
          ) {//Insert remote tracked value and increase offset
            currentResource->dirtyResourceCounters[ii] = currentResource->dirtyResourceCounters[resourcesGotDirty[i]+offset];
            offset++;
          } else {//Insert local tracked value and continue
            currentResource->dirtyResourceCounters[ii] = dirtyTracker[i][ii - offset];
          }
        }
        currentResource->dirtyDishesCount += resourcesGotDirty[i];
        sem_post(&currentResource->dirtyCountersMutex);
        for (int ii = 0; ii<resourcesGotDirty[i]; ii++) {
          sem_post(&currentResource->dirty);
        }
      }

    //Free clean resources from here on
      for (int ii = 0; ii<resourcesGotClean[i]; ii++) { //Signal as many as were acquired
        sem_post(&currentResource->dirty); //Signal the dirty instead of clean
      }

      int semret = 0;
      do {
        semret = sem_wait(&currentResource->dirtyCountersMutex); //Must block to update array
        if(semret == -1 && errno != EINTR) cookStop(SEMAPHORE_FAIL);
      } while (semret != 0);

      for (int ii = currentResource->dirtyDishesCount-1; ii>=0; ii--) { //Shift all array contents right by the new dishes to add
        currentResource->dirtyResourceCounters[ii+resourcesGotClean[i]] = currentResource->dirtyResourceCounters[ii];
      }
      for (int ii = 0; ii<resourcesGotClean[i]; ii++) {
        currentResource->dirtyResourceCounters[ii] = 1; //Insert new "used once" dishes
      }
      currentResource->dirtyDishesCount += resourcesGotClean[i]; //Update counter
      sem_post(&currentResource->dirtyCountersMutex);
    }

    for (int i = 0; i<dish->requiredSize; i++) {
      free(dirtyTracker[i]);
    }

    return true;
  } else { //If not all resources acquired, cooking aborted
    for (int i = 0; i<dish->requiredSize; i++) { //Free all resource types
      for (int ii = 0; ii<resourcesGotClean[i]; ii++) { //Signal as many as were acquired
        sem_post(&kitchen->resources[dish->requiredTypes[i]].clean);
      }
    }
    //Free the dirty resources and re-add them to the tracking array
    for (int i = 0; i<dish->requiredSize; i++){
      Resource* currentResource = &kitchen->resources[dish->requiredTypes[i]];
      if (dirtyTracker[i] != NULL || resourcesGotDirty[i] == 0) { //If no dish was tracked, no need to bother

        int semret = 0;
        do {
          semret = sem_wait(&currentResource->dirtyCountersMutex);
          if (semret == -1 && errno != EINTR) cookStop(SEMAPHORE_FAIL);
        } while (semret != 0);

        for (int ii = currentResource->dirtyDishesCount-1; ii>=0; ii--) {//Move all tracked resources left
          currentResource->dirtyResourceCounters[ii+resourcesGotDirty[i]] = currentResource->dirtyResourceCounters[ii];
        }
        int offset = 0; //Secondary counter insertion
        for (int ii = 0; ii<resourcesGotDirty[i]+currentResource->dirtyDishesCount; ii++) { //Insert local tracks keeping the array sorted
          if (
            ii-offset >= resourcesGotDirty[i] || ( //If local tracked array is depleted
              offset < currentResource->dirtyDishesCount && //Remote tracked array is not depleted
              dirtyTracker[i][ii - offset] > currentResource->dirtyResourceCounters[resourcesGotDirty[i]+offset] //Remote track value is smaller than the local tracked one
            )
          ) {//Insert remote tracked value and increase offset
            currentResource->dirtyResourceCounters[ii] = currentResource->dirtyResourceCounters[resourcesGotDirty[i]+offset];
            offset++;
          } else {//Insert local tracked value and continue
            currentResource->dirtyResourceCounters[ii] = dirtyTracker[i][ii - offset];
          }
        }
        currentResource->dirtyDishesCount += resourcesGotDirty[i];
        sem_post(&currentResource->dirtyCountersMutex);
        for (int ii = 0; ii<resourcesGotDirty[i]; ii++) {
          sem_post(&currentResource->dirty);
        }
      }
    }

    for (int i = 0; i<dish->requiredSize; i++) {
      free(dirtyTracker[i]);
    }

    return false;
  }
}

void cleanResource(Kitchen* kitchen, atomic_int* busyTimePosition) {
  //Iterate over all resources
  //Find the ones with the least clean units
  //Among them, find the one with the least dirty units
  //Clean the resource
  //If sink is busy

  //Initialize a default value
  Resource* toClean = &kitchen->resources[0];
  int cleanCount;
  sem_getvalue(&toClean->clean, &cleanCount);
  int dirtyCount;
  sem_getvalue(&toClean->dirty, &dirtyCount);

  //Iterate over all others
  for(int i = 1; i<kitchen->resourceCount; i++) {
    Resource* maybeClean = &kitchen->resources[i];
    int temp1, temp2;
    sem_getvalue(&maybeClean->clean, &temp1);
    sem_getvalue(&maybeClean->dirty, &temp2);
    if (
      temp2 != 0 && ( //Candidate resource has something to clean
        dirtyCount == 0 || ( //Current defualt-ish resource has nothing to clean
          temp1<cleanCount || ( //The candidate resource has less clean units
            temp1==cleanCount && temp2<dirtyCount //Candidate resource tie-breaks with dirty units
          )
        )
      )
    ) {
      toClean = maybeClean;
      cleanCount = temp1;
      dirtyCount = temp2;
    }
  }

  //Try to acquire sink, if it is busy then there's no point
  int retval = sem_trywait(&kitchen->sink);
  if (retval != -1) {
    //Try to clean, without blocking (in case the defualt value has no dirty, I.E. there's no dirty at all)
    retval = sem_trywait(&toClean->dirty);
    if (retval != -1){

      //Clean
      atomic_fetch_add(busyTimePosition, toClean->clean_time);
      cookSleep(toClean->clean_time, busyTimePosition);

      //Cleaning finished
      int semret = 0;
      do {
        semret = sem_wait(&toClean->dirtyCountersMutex); //Must block if busy, but usually operations are fast
        if (semret == -1 && errno != EINTR) cookStop(SEMAPHORE_FAIL);
      } while (semret != 0);
      toClean->dirtyDishesCount--; //Reduce count, turning it into the index of the rightmost dirty dish
      toClean->dirtyResourceCounters[toClean->dirtyDishesCount] = 0; //Void the value
      sem_post(&toClean->dirtyCountersMutex); //Release block for others
      sem_post(&toClean->clean);//Increment clean counter
    }
    sem_post(&kitchen->sink);
  }
}

typedef struct Queue {
  int queueSize;
  int* dishIndexes;
  int* waiterIDs;
} Queue;

void addTask(Queue* queue, int dish, int waiter) {
  if (queue == NULL) return;

  if (queue->queueSize == 0) {
    queue->dishIndexes = malloc(sizeof(int));
    queue->waiterIDs = malloc(sizeof(int));
  } else if ((queue->queueSize & (queue->queueSize -1)) == 0) { //Test if queueSize is a power of two, if so then expand array
    queue->dishIndexes = realloc(queue->dishIndexes, sizeof(int)*2*queue->queueSize);
    queue->waiterIDs = realloc(queue->waiterIDs, sizeof(int)*2*queue->queueSize);
  }
  queue->dishIndexes[queue->queueSize] = dish;
  queue->waiterIDs[queue->queueSize] = waiter;
  queue->queueSize++; //queueSize acts as the index of the first free slot
  if ((queue->dishIndexes == NULL || queue->waiterIDs == NULL) && errno == ENOMEM) cookStop(MALLOC_FAIL);
}

void rmTask(Queue* queue, int index) {
  if (queue == NULL) return;

  if (index < 0) return; //Not valid index
  if (queue->queueSize <= index) return; //Not valid index
  queue->queueSize--; //queueSize is now the index of the last valid value
  if (queue->queueSize == 0) {
    free(queue->dishIndexes);
    free(queue->waiterIDs);
    queue->dishIndexes = NULL;
    queue->waiterIDs = NULL;
    return;
  }

  for (int i = index; i<queue->queueSize; i++) { //Move all the elements after index left
    queue->dishIndexes[i] = queue->dishIndexes[i+1];
    queue->waiterIDs[i] = queue->waiterIDs[i+1];
  }

  if ((queue->queueSize & (queue->queueSize-1)) == 0) { //If queueSize is a power of two, half of the array is empty
    queue->dishIndexes = realloc(queue->dishIndexes, sizeof(int)*queue->queueSize);
    queue->waiterIDs = realloc(queue->waiterIDs, sizeof(int)*queue->queueSize);
    if ((queue->dishIndexes == NULL || queue->waiterIDs == NULL) && errno == ENOMEM) cookStop(MALLOC_FAIL);
  }
}

void* cook(void* arg) {
  CookArg* initData = (CookArg*) arg;

  atomic_bool* runFlag = initData->run;
  Kitchen* kitchen = initData->kitchen;
  int rxOrders = initData->rxOrders;
  int* txDishes = initData->txDishes;
  atomic_int* busyTime = initData->busyTime;

  free(initData);

  Queue queue = (Queue){.queueSize = 0, .dishIndexes = NULL, .waiterIDs = NULL};

  while (atomic_load(runFlag)) {
    int readStatus;
    do {
      int receivedOrder[2] = {0, 0};
      readStatus = read(rxOrders, &receivedOrder, sizeof(receivedOrder));
      if (readStatus == -1 && errno != EAGAIN ) cookStop(READ_FAIL);
      if (readStatus > 0) {
        printf("Cook receved:\t%d\t%d\t%d\n", readStatus, receivedOrder[0], receivedOrder[1]);
        addTask(&queue, receivedOrder[0], receivedOrder[1]);
      }
    } while (readStatus > 0); //Get all waiting orders in pipe

    //Decision making starts
    bool foundTask = false;
    Dish* toDoDish;
    int taskIndex;
    Resource* resources = kitchen->resources;

    //Iterate over list searching for a dish doable with only clean resources
    for (int i = 0; i<queue.queueSize && !foundTask; i++) {
      bool allAvailable = true;
      toDoDish = &menu.dishes[queue.dishIndexes[i]];
      taskIndex = i;

      //Iterate over all resources required by the dish
      for (int ii = 0; ii<toDoDish->requiredSize && allAvailable; ii++) {
        //Compare the clean dishes to the required amount of dishes
        int cleanCount;
        sem_getvalue(&resources[toDoDish->requiredTypes[ii]].clean, &cleanCount);
        allAvailable = cleanCount >= toDoDish->requiredCount[ii];
      }
      foundTask = allAvailable; //If all reosurces are available, the task is found
    }

    if (foundTask) {
      //Prepare that dish
      bool result = cookDish(toDoDish, kitchen, busyTime);
      if(result) {//Cooking worked
        int retwrite = 0;

        do {
          retwrite = write(txDishes[queue.waiterIDs[taskIndex]], &queue.dishIndexes[taskIndex], sizeof(int)); //Signal the dish is ready
          if (retwrite == -1 && errno != EINTR) cookStop(WRITE_FAIL);
        } while (retwrite != 0);
        rmTask(&queue, taskIndex); //Clear task from queue
      }
    } else if (queue.queueSize <= OVERWORK_THRESHOLD) {
      cleanResource(kitchen, busyTime);
    } else {
      //Do the first possible dish, as most likely to be urgent
      for (int i = 0; i<queue.queueSize && !foundTask; i++) {
        bool allAvailable = true;
        toDoDish = &menu.dishes[queue.dishIndexes[i]];
        taskIndex = i;
        //Iterate over all resources required by the dish
        for (int ii = 0; ii<toDoDish->requiredSize && allAvailable; ii++) {
          //Compare the clean dishes to the required amount of dishes
          int cleanCount;
          int dirtyCount;
          sem_getvalue(&resources[toDoDish->requiredTypes[ii]].clean, &cleanCount);
          sem_getvalue(&resources[toDoDish->requiredTypes[ii]].dirty, &dirtyCount);
          allAvailable = cleanCount + dirtyCount >= toDoDish->requiredCount[ii];
        }
        foundTask = allAvailable; //If all reosurces are available, the task is found
      }
      if(foundTask) {
        bool result = cookDishDirty(toDoDish, kitchen, busyTime);
        if(result) {//Cooking worked
          int retwrite = 0;
          do {
            retwrite = write(txDishes[queue.waiterIDs[taskIndex]], &queue.dishIndexes[taskIndex], sizeof(int)); //Signal the dish is ready
            if (retwrite == -1 && errno != EINTR) cookStop(WRITE_FAIL);
          } while (retwrite != 0);

          rmTask(&queue, taskIndex); //Clear task from queue
        }
      } else {
        cleanResource(kitchen, busyTime);
      }
    }
  }

  while (0<queue.queueSize) {
    rmTask(&queue, 0);
  }

  pthread_exit(ALL_OK);
};
