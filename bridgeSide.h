#ifndef BRIDGE_SIDE_H
#define BRIDGE_SIDE_H

#include <pthread.h>
#include "vehicle.h" 

#define MAX_SIZE 100

typedef struct BridgeSide {
    pthread_t threads[MAX_SIZE]; //Hilos asociados a los vehículos
    Vehicle vehicles[MAX_SIZE]; //Arreglo de vehiculos
    int max_cars; //Cantidad de vehículos que entran al lado del puente
    int current_cars; //Cantidad actual de vehículos en el lado
    pthread_mutex_t current_cars_mutex; 
    double exp_mean; //Media de la dist. exponencial
    double speed_mean; //Media de la velocidad
    double min_speed; //Limite inferior del intervalo de velocidad
    double max_speed; //Limite superior del intervalo de velocidad
    double total_speed; //Usado para calcular las velocidades que se asignan a los vehículos de cada lado
} BridgeSide;

#endif 

