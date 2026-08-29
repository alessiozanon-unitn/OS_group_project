#include "customer.h"
#include "order.h"
#include "unity.h"
#include <bits/pthreadtypes.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "kitchen.h"
#include "restaurant.h"

atomic_int score = 0;
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
  free(kitchen);
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
  free(kitchen);
}
void test_unsatisfied_customer(){
    kitchen = malloc(sizeof(Kitchen));
    kitchen->resourceCount = 0;
    kitchen->resources = NULL;
    load_resources_from(resources_file, kitchen);
    load_menu_from(menu_file, kitchen);

    CustomerArg *customer_args = malloc(sizeof(CustomerArg));
    customer_args->seed[0] = 0;
    customer_args->seed[1] = 0;
    customer_args->seed[2] = 0;
    customer_args->seed[3] = 0;

    atomic_int *customerStatus = malloc(sizeof(atomic_int));
    atomic_init(customerStatus, UNSENT);

    Order ***order_table = calloc(maxCustomers, sizeof(Order**));
    order_table[0] = malloc(sizeof(Order*));
    *order_table[0] = NULL;
    Order **order_slot = order_table[0];

    sem_t slotMut;
    sem_init(&slotMut, 0, 1);

    int serving[2];
    int arrival[2];
    pipe(serving);
    pipe(arrival);
    fcntl(serving[0], F_SETFL, O_NONBLOCK);

    customer_args->status = customerStatus;
    customer_args->orderSlot = order_slot;
    customer_args->slotMut = &slotMut;
    customer_args->rxServing = serving[0];
    customer_args->tableNumber = 0;
    customer_args->txArrival = arrival[1];
    customer_args->max_dishes_per_order = max_dishes_per_order;
    customer_args->patience_level_range = patience_level_range;

    pthread_t customer_thread;
    pthread_create(&customer_thread, NULL, customer, (void*) customer_args);

    int recv_tableNumber;
    read(arrival[0], &recv_tableNumber, sizeof(int));
    TEST_ASSERT_EQUAL_INT(0, recv_tableNumber);
    TEST_ASSERT_EQUAL_INT(WAITING, atomic_load(customerStatus));

    sem_wait(&slotMut);
    TEST_ASSERT_NOT_NULL(*order_slot);
    Order *order = *order_slot;

    int total_price = 0;
    for(int i = 0; i < order->count; i++){
        total_price += order->dishList[i].dish->price;
    }
    int initial_patience = order->patienceLevel;
    sem_post(&slotMut);

    // Wait for customer thread to run out of patience and finish
    while(atomic_load(customerStatus) != UNSATISFIED) {
        usleep(1000);
    }

    int expected_score = lround(total_price * log2(1 + ((double) initial_patience / (1 + 0))));
    TEST_ASSERT_EQUAL_INT(-expected_score, atomic_load(&score));

    pthread_join(customer_thread, NULL);

    close(serving[0]);
    close(serving[1]);
    close(arrival[0]);
    close(arrival[1]);
    sem_destroy(&slotMut);
    free(customerStatus);
    free(order_table[0]);
    free(order_table);
    free(kitchen);
}
int main(){
  UNITY_BEGIN();
  RUN_TEST(test_resources_loading);
  RUN_TEST(test_menu_loading);
  RUN_TEST(test_unsatisfied_customer);
  return UNITY_END();
}
