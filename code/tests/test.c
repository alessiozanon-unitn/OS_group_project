#include "unity.h"
#include <semaphore.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "menu.h"
#include "kitchen.h"

atomic_int score;
Menu* menu;
Kitchen* kitchen;

// env variables
const uint cookCount = 4;
const uint waiterCount = 3;
const uint maxCustomers = 10;
const uint totalCustomers = 30;
const uint randomSeed = 42;
double gameSpeed = 10;
const char* menu_file = "./tests/data/menu.csv";
const char* resources_file = "./tests/data/resources.csv";
const int max_dishes_per_order = 10;
const int patience_level_range = 30;


void setUp(void){ // For shared state set up

}

void tearDown(void){

}

void test_resources_loading(){
  kitchen = malloc(sizeof(Kitchen));
  kitchen->resourceCount = 0;
  kitchen->resources = NULL;
  load_resources_from(resources_file, kitchen);
  TEST_ASSERT_EQUAL_INT(3, kitchen->resourceCount);

  TEST_ASSERT_EQUAL_STRING("resource1",kitchen->resources[0].name);
  TEST_ASSERT_EQUAL_STRING("resource2",kitchen->resources[1].name);
  TEST_ASSERT_EQUAL_STRING("resource3",kitchen->resources[2].name);


  for(int i=0;i<kitchen->resourceCount;i++){
    TEST_ASSERT_EQUAL_INT((i + 1)*100, kitchen->resources[i].clean_time);
    TEST_ASSERT_EQUAL_INT(0, kitchen->resources[i].dirtyDishesCount);
    int clean;
    int dirty;
    sem_getvalue(&kitchen->resources[i].clean, &clean);
    sem_getvalue(&kitchen->resources[i].dirty, &dirty);
    TEST_ASSERT_EQUAL_INT(kitchen->resourceCount,clean);
    TEST_ASSERT_EQUAL_INT(0,dirty);

    for(int j=0;j<kitchen->resourceCount;j++){
      TEST_ASSERT_EQUAL_INT(0, kitchen->resources[i].dirtyResourceCounters[j]);
    }
  }
}

int main(){
  UNITY_BEGIN();
  RUN_TEST(test_resources_loading);
  return UNITY_END();
}
