#ifndef SEMAPHORE_H
#define SEMAPHORE_H
#include <pthread.h>

typedef struct Semaphore {
    int light;
    double time;
} Semaphore;

#endif 

