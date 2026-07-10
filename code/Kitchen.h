#ifndef RESOURCE
#define RESOURCE

#include <string.h>
#include <semaphore.h>

struct ResourceUnit;
typedef struct ResourceUnit{
  String name;
} ResourceUnit

struct Resource;
typedef struct Resource {
  ResourceUnit unit;
  int clean_time;
  sem_t clean;
  sem_t dirty;
} Resource;

struct Kitchen {
  Resoure* resources;
  sem_t sink;
}

#endif
