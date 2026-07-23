#include "kitchen.h"
#include <math.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
#include "utils.h"

extern double gameSpeed;

void custom_sleep(){
  long cycles = lround(1000000.0 * 60.0 / gameSpeed);
  for(long i= cycles; i>0; i -= (1000000-1)){
    if (i>=(1000000-1))
      usleep((1000000-1));
    else {
      usleep(i);
    }
  }
}

void print_menu(Menu *menu){
  for(int i=0;i<menu->dishCount;i++){
    printf("Name: %s - Price: %d - Time: %d, RequiredSize: %d\nTypes: ", menu->dishes[i].name, menu->dishes[i].price, menu->dishes[i].time, menu->dishes[i].requiredSize);
    for(int j=0;j<menu->dishes[i].requiredSize;j++){
      printf("(Type: %d) * %d - ", menu->dishes[i].requiredTypes[j], menu->dishes[i].requiredCount[j]);
    }
    printf("\n");
  }
}
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
