#include "unity.h"
#include <semaphore.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "kitchen.h"
#include "restaurant.h"

atomic_int score;
Menu menu;
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
    int dirtyCountersMutex;
    sem_getvalue(&kitchen->resources[i].clean, &clean);
    sem_getvalue(&kitchen->resources[i].dirty, &dirty);
    sem_getvalue(&kitchen->resources[i].dirtyCountersMutex, &dirtyCountersMutex);
    TEST_ASSERT_EQUAL_INT(i+1,clean);
    TEST_ASSERT_EQUAL_INT(0,dirty);
    TEST_ASSERT_EQUAL_INT(1,dirtyCountersMutex);

    for(int j=0;j<clean+dirty;j++){
      TEST_ASSERT_EQUAL_INT(0, kitchen->resources[i].dirtyResourceCounters[j]);
    }
  }
}

void test_menu_loading(){
  kitchen = malloc(sizeof(Kitchen));
  kitchen->resourceCount = 0;
  kitchen->resources = NULL;
  load_resources_from(resources_file, kitchen);

  load_menu_from(menu_file, kitchen);

  TEST_ASSERT_EQUAL_INT(3, menu.dishCount);

  TEST_ASSERT_EQUAL_STRING("dish1", menu.dishes[0].name);
  TEST_ASSERT_EQUAL_STRING("dish2", menu.dishes[1].name);
  TEST_ASSERT_EQUAL_STRING("dish3", menu.dishes[2].name);

  for(int i=0;i<menu.dishCount;i++){
    TEST_ASSERT_EQUAL_INT(i+1, menu.dishes[i].price);
    TEST_ASSERT_EQUAL_INT((i+1)*10, menu.dishes[i].time);
    TEST_ASSERT_EQUAL_INT(i+1, menu.dishes[i].requiredSize);
    for(int j=0;j<menu.dishes[i].requiredSize;j++){
      TEST_ASSERT_EQUAL_INT(j+1, menu.dishes[i].requiredCount[j]);
      TEST_ASSERT_EQUAL_INT(j, menu.dishes[i].requiredTypes[j]);
    }
  }
}
int main(){
  UNITY_BEGIN();
  RUN_TEST(test_resources_loading);
  RUN_TEST(test_menu_loading);
  //RUN_TEST(test_customer);
  return UNITY_END();
}
