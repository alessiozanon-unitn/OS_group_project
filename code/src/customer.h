#ifndef CUSTOMER
#define CUSTOMER
void* customer (void*);

typedef enum STATUS { 
  SATISFIED,
  UNSATISFIED,
  WAITING,
  UNSENT, 
  ERROR
} customerStatus;
#endif
