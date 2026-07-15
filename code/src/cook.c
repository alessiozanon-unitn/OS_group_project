#include "cook.h"
#include "restaurant.h"
#include "menu.h"
#include "kitchen.h"

#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <semaphore.h>
#include <stdatomic.h>

const int OVERWORK_THRESHOLD = 3;
extern Menu menu;
extern int score;
extern sem_t scoreMutex;

void addBusyTime(int expectedBusy, atomic_int* busyTimePosition) {

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
    addBusyTime(dish->time, busyTimePosition);
    
    //TODO Cook

    //Finished cooking
    for (int i = 0; i<dish->requiredSize; i++) { //Free all resource types
      Resource* currentResource = &kitchen->resources[dish->requiredTypes[i]];
      for (int ii = 0; ii<resourcesGot[i]; ii++) { //Signal as many as were acquired
        sem_post(&currentResource->dirty); //Signal the dirty instead of clean
      }
      sem_wait(&currentResource->dirtyCountersMutex); //Must block to update array
      for (int ii = currentResource->dirtyDishesCount-1; ii>=0; ii--) { //Shift all array contents right by the new dishes to add
        currentResource->dirtyResourceCounters[ii+resourcesGot[i]] = currentResource->dirtyResourceCounters[ii];
      }
      for (int ii = 0; ii<resourcesGot[i]; ii++) {
        currentResource->dirtyResourceCounters[ii] = 1; //Insert new "used once" dishes
      }
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
  if(cookDish(dish, kitchen, busyTimePosition)) return true;

  //On failure of that, cook using the least dirty resources, update evreything accordingly
  
  //Subtract score based on resources used
  
  //Return true if cooking happened, false otherwise
}

void cleanResource(Kitchen* kitchen, atomic_int* busyTimePosition) {
  //Iterate over all resources
  //Find the ones with the least clean units
  //Among them, find the one with the least dirty units
  //Clean the resource
  //If sink is busy, just return without waiting
  
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
      //TODO do the cleaning
      sem_wait(&toClean->dirtyCountersMutex); //Must block if busy, but usually operations are fast
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
  }
}

void* cook(void* arg) {
  CookArg* initData = (CookArg*) arg;

  Kitchen* kitchen = initData->kitchen;
  int rxOrders = initData->rxOrders;
  int* txDishes = initData->txDishes;
  atomic_int* busyTime = initData->busyTime;

  free(initData);
  
  bool runFlag = true;
  
  Queue queue = (Queue){.queueSize = 0, .dishIndexes = NULL, .waiterIDs = NULL};

  while (runFlag) {
    int receivedOrder[2] = {0, 0};
    int readStatus = read(rxOrders, &receivedOrder, sizeof(receivedOrder));//TODO make sure this works
    
    if (readStatus != -1) {
      addTask(&queue, receivedOrder[0], receivedOrder[1]);
    }
    
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
      for (int ii = 0; ii<toDoDish->requiredSize && allAvailable; i++) {
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
        write(txDishes[queue.waiterIDs[taskIndex]], &queue.dishIndexes[taskIndex], sizeof(int)); //Signal the dish is ready
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
        for (int ii = 0; ii<toDoDish->requiredSize && allAvailable; i++) {
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
          write(txDishes[queue.waiterIDs[taskIndex]], &queue.dishIndexes[taskIndex], sizeof(int)); //Signal the dish is ready
          rmTask(&queue, taskIndex); //Clear task from queue
        }
      } else {
        cleanResource(kitchen, busyTime);
      }
    }
  }
}; 
