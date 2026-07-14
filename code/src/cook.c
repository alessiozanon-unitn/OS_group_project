#include "cook.h"
#include "restaurant.h"
#include "menu.h"
#include "kitchen.h"

#include <stdlib.h>
#include <unistd.h>

const int OVERWORK_THRESHOLD = 3;

void cookDish(int dishIndex, Kitchen Kitchen) {

}

void cleanResource(Kitchen* kitchen) {
  //Iterate over all resources
  //Find the ones with the least clean units
  //Among them, find the one with the least dirty units
  //Clean the resource
}

void setBusyTime(int expectedBusy, int* busyTimePosition) {
  
}

struct TaskQueue;
typedef struct TaskQueue {
  int dishIndex;
  int waiterIndex;
  struct TaskQueue* next;
  struct TaskQueue* prev;
} TaskQueue;

void addTask(TaskQueue** queue, int dishIndex, int waiterIndex) {
  TaskQueue* newNode = malloc(sizeof(TaskQueue*));
  newNode->dishIndex = dishIndex;
  newNode->waiterIndex = waiterIndex;
  newNode->next = NULL;
  
  if (*queue == NULL) {
    newNode->prev = NULL;
    *queue = newNode;
  } else {
    TaskQueue* expl = *queue;
    while (expl->next != NULL) {
      expl = expl->next;
    }
    expl->next = newNode;
    newNode->prev = expl;
  }
  return;
}

void getTask(TaskQueue* queue, int* returnDish, int* returnWaiter) {
  if (queue == NULL) {
    *returnDish = -1;
    *returnWaiter = -1;
  } else {
    *returnDish = queue->dishIndex;
    *returnWaiter = queue->waiterIndex;
    if (queue->prev != NULL) queue->prev->next = queue->next;
    if (queue->next != NULL) queue->next->prev = queue->prev;
    free(queue);
  }
  return;
}

void* cook(void* arg) {
  CookArg* initData = (CookArg*) arg;

  Kitchen* kitchen = initData->kitchen;
  int rxOrders = initData->rxOrders;
  int* txDishes = initData->txDishes;
  int* busyTime = initData->busyTime;

  free(initData);
  
  bool runFlag = true;
  int queueSize = 0;
  TaskQueue* queue = NULL;

  while (runFlag) {
    int receivedOrder[2] = {0, 0};
    int readStatus = read(rxOrders, &receivedOrder, sizeof(receivedOrder));//TODO make sure this works
    
    if (readStatus != -1) {
      addTask(&queue, receivedOrder[0], receivedOrder[1]);
      queueSize++; //TODO, make doubly linked and the operations location-agnostic
    }
    
    //Decision making starts
    bool foundTask = false;

    //Iterate over list searching for a dish doable with only clean resources
    

    if (foundTask) {
      //Prepare that dish
    } else if (queueSize <= OVERWORK_THRESHOLD) {
      cleanResource(kitchen);  
    } else {
      //Iterate over list searching for dish with least score deficit
      if(foundTask) {
        //Prepare that dish
        //Subtract scrore accordingly
      } else {
        cleanResource(kitchen);
      }
    }
  }


}; 
