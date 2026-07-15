#include "kitchen.h"
#include <semaphore.h>
#include <stdio.h>
#include "utils.h"

void print_kitchen(Kitchen *kitchen){
  printf("Resource count: %d\n", kitchen->resourceCount);
  int sink;
  sem_getvalue(&kitchen->sink, &sink);
  printf("Sink: %d\n", sink);

  printf("Resources:\n");

  for(int i=0;i<kitchen->resourceCount;i++){
    printf("name: %s\n", kitchen->resources[i].name);
    printf("clean_time: %d\n", kitchen->resources[i].clean_time);

    int clean;
    sem_getvalue(&kitchen->resources[i].clean, &clean);
    printf("clean: %d\n", clean);

    int dirty;
    sem_getvalue(&kitchen->resources[i].dirty, &dirty);
    printf("clean: %d\n", dirty);

    printf("DirtyResourceCounters: ");
    for(int j=0;j<(clean+dirty);j++){
      printf("%d ", kitchen->resources[i].dirtyResourceCounters[j]);
    }
    printf("\n");
  }
}
