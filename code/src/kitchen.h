#ifndef KITCHEN
#define KITCHEN

#include <semaphore.h>

typedef struct Resource {
  char* name;
  int clean_time;
  sem_t clean; //Initialized to resource quantity
  sem_t dirty; //Initialized at 0, signaled when the resource is used
  int dirtyDishesCount;
  sem_t dirtyCountersMutex;
  int* dirtyResourceCounters; //Of size quantity (clean + dirty), initialized to full zeroes and updated individually
} Resource;

typedef struct Kitchen {
  int resourceCount;
  Resource* resources;
  sem_t sink; //Binary semaphore
} Kitchen;

int load_resources_from(const char* file_path, Kitchen *kitchen);

#endif
