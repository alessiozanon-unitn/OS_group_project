#include "cook.h"
#include "restaurant.h"
#include "menu.h"
#include "kitchen.h"

#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <semaphore.h>

const int OVERWORK_THRESHOLD = 3;
extern Menu menu;
extern int score;
extern sem_t scoreMutex;

bool cookDish(Dish* dish, Kitchen* Kitchen, int* busyTimePosition) {
  //grab semaphore resources and do the deed
  //Increase dirty counter and insert new 1s in the array
  //Return true if cooking happened, false otherwise
}

bool cookDishDirty(Dish* dish, Kitchen* Kithcen, int* busyTimePosition) {
  //attempt cleanCook, return true if that worked
  //On failure of that, cook using the least dirty resources, update evreything accordingly
  //Subtract score based on resources used
  //Return true if cooking happened, false otherwise
}

void cleanResource(Kitchen* kitchen, int* busyTimePosition) {
  //Iterate over all resources
  //Find the ones with the least clean units
  //Among them, find the one with the least dirty units
  //Clean the resource
  //If sink is busy, just return without waiting
}

void setBusyTime(int expectedBusy, int* busyTimePosition) {
  
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
  int* busyTime = initData->busyTime;

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
    Resource* resources = kitchen->resources;

    //Iterate over list searching for a dish doable with only clean resources
    for (int i = 0; i<queue.queueSize && !foundTask; i++) {
      bool allAvailable = true;
      Dish toDoDish = menu.dishes[queue.dishIndexes[i]];
      //Iterate over all resources required by the dish
      for (int ii = 0; ii<toDoDish.requiredSize && allAvailable; i++) {
        //Compare the clean dishes to the required amount of dishes
        int cleanCount;
        sem_getvalue(&resources[toDoDish.requiredTypes[ii]].clean, &cleanCount);
        allAvailable = cleanCount >= toDoDish.requiredCount[ii];
      }
      foundTask = allAvailable; //If all reosurces are available, the task is found
    }

    if (foundTask) {
      //Prepare that dish
      bool result = cookDish(toDoDish, kitchen, busyTime); 
    } else if (queue.queueSize <= OVERWORK_THRESHOLD) {
      cleanResource(kitchen, busyTime);  
    } else {
      //Do the first possible dish, as most likely to be urgent
      for (int i = 0; i<queue.queueSize && !foundTask; i++) {
        bool allAvailable = true;
        Dish toDoDish = menu.dishes[queue.dishIndexes[i]];
        //Iterate over all resources required by the dish
        for (int ii = 0; ii<toDoDish.requiredSize && allAvailable; i++) {
          //Compare the clean dishes to the required amount of dishes
          int cleanCount;
          int dirtyCount;
          sem_getvalue(&resources[toDoDish.requiredTypes[ii]].clean, &cleanCount);
          sem_getvalue(&resources[toDoDish.requiredTypes[ii]].dirty, &dirtyCount);
          allAvailable = cleanCount + dirtyCount >= toDoDish.requiredCount[ii];
        }
        foundTask = allAvailable; //If all reosurces are available, the task is found
      }
      if(foundTask) {
        bool result = cookDishDirty(toDoDish, kitchen, busyTime);
      } else {
        cleanResource(kitchen, busyTime);
      }
    }
  }

}; 
