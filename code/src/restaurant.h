#ifndef THREADARGS
#define THREADARGS

#include "kitchen.h"
#include "order.h"
#include <stdint.h>

typedef struct CookArg {
  uint64_t seed[4];
  Kitchen* kitchen;
  int rxOrders;
  int* txDishes; //Array of size #Waiter
  int* busyTime; //Position on the busyTime array
} CookArg;

/*
  The cook receives Waiter's ID and a dish to prepare, and does their thing.
  Once the dish is ready, uses waiter ID to index txDishes, and sends the dish
  Also, the cook keeps it's busyTime up to date by aadding the total time required by their plan to the current time and posting it on their position on the bustTime array
*/

typedef struct WaiterArg {
  uint64_t seed[4];
  int ID; //Allows cooks to reply properly
  int cookCount; //#Cook
  int* txOrders; //Array of size #Cook
  int rxDishes;
  int* busyTime; //Full array, for reading only
  int customerCount; //#Max_Customers
  Order** orderTable; //Full array, can see where orders are assigned;
  int* txServing; //Array of size #Max_Customers
} WaiterArg;

/*
  Waiter does their decisions, reading the order (contains cleint's patience and arrival time) and  busyTime array.
  Having chosen a cook for a dish (or more), sends their ID and the dish's index value in the correct rxOrders.
  Waiter entatains as the can, checking their rxOrders and updating the patience on the orderTable.
  Waiter recieves the completed dish ID, uses it to serve the custormer indexed in the same place as the order the dish is from into rxServing array.
  Waiter should always listen on rxArrival unless he has ceil(#Max_Customers/#Waiters) customers already.
 */

typedef struct CustomerArg {
  uint64_t seed[4];
  Order* orderSlot; //Position in orderTable for their order
  int rxServing;
} CustomerArg;

/*
  Customer is spawned, creates and order and posts it into their position orderSlot.
  When a dish is received in sxServing, the order is updated switching the boolean.
  Patience is routinely checked (inside order), and when time has run out or the order is complete the client sets the array position to NULL and leaves.
  Client updates score accordingly.
 */
#endif
