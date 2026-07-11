#ifndef KITCHEN
#define KITCHEN

#include <string.h>
#include <semaphore.h>

typedef struct ResourceUnit{
  char* name;
} ResourceUnit;

typedef struct Resource {
  ResourceUnit unit;
  int clean_time;
  sem_t clean; //Initialized to resource count
  sem_t dirty; //Initialized at 0, signaled when the reource is used
} Resource;

typedef struct Kitchen {
  Resource* resources;
  sem_t sink; //Binary semaphore
} Kitchen;

#endif
