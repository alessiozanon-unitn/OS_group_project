#ifndef KITCHEN
#define KITCHEN

#include <semaphore.h>

typedef struct Resource {
  char* name;
  int clean_time;
  sem_t clean; //Initialized to resource count
  sem_t dirty; //Initialized at 0, signaled when the reource is used
  int* dirtyResourceCounters; //Of size resource count, initialized to full zeroes and updated individually
} Resource;

typedef struct Kitchen {
  int resourceCount;
  Resource* resources;
  sem_t sink; //Binary semaphore
} Kitchen;

#endif
