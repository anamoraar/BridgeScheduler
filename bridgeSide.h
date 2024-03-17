#ifndef BRIDGE_SIDE_H
#define BRIDGE_SIDE_H

#include <pthread.h>
#include "vehicle.h" 

#define MAX_SIZE 100

typedef struct BridgeSide {
    pthread_t threads[MAX_SIZE]; //Hilos asociados a los vehiculos
    Vehicle vehicles[MAX_SIZE]; //Arreglo de vehiculos
    int size; //Cantidad actual de vehiculos en el lado
    pthread_mutex_t size_mutex; 
    double exp_mean; //Media de la dist. exponencial
    double speed_mean; //Media de la velocidad
    double min_speed; //Limite superior del intervalo de velocidad
    double max_speed; //Limite superior del intervalo de velocidad
} BridgeSide;

#endif 

