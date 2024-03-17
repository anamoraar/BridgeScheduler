#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <math.h> //Para usar log(r) hay que linkear con -lm
#include "vehicle.h" 
#include "bridgeSide.h" 

#define MAX_SIZE 100

//Lados del puente
BridgeSide west_side;
BridgeSide east_side;

//Setear los parámetros que se dan en la entrada para cada lado del puente
void initializeBridgeSide() {
    //Parámetros del lado oeste del puente (en realidad se deben leer de consola)
    double west_exp_mean = 2;
    double west_spead_mean = 50;
    double west_min_speed = 40;
    double west_max_speed = 60; 
    west_side.size = 0;
    west_side.exp_mean = west_exp_mean;
    west_side.speed_mean = west_spead_mean;
    west_side.min_speed = west_min_speed;
    west_side.max_speed = west_max_speed;
    pthread_mutex_init(&(west_side.size_mutex), NULL);
    //Parámetros del lado este del puente
    double east_exp_mean = 2;
    double east_spead_mean = 50;
    double east_min_speed = 40;  
    double east_max_speed = 60; 
    east_side.size = 0;
    east_side.exp_mean = east_exp_mean;
    east_side.speed_mean = east_spead_mean;
    east_side.min_speed = east_min_speed;
    east_side.max_speed = east_max_speed;
    pthread_mutex_init(&(east_side.size_mutex), NULL);
}

void* printThread(void* arg) {
    Vehicle* vehicle = (Vehicle*)arg;
    printf("Thread %ld executing with priority %d and speed %d\n", pthread_self(), vehicle->priority, vehicle->speed);
    pthread_exit(NULL);
}

//Retorna 2 si el vehículo es carro y 1 si es ambulancia, es mucho más probable que sea carro
int generatePriority() {
    int random_number = rand() % 100; //Generar un numero random entre 0 y 99
    if (random_number < 20) return 1; //Hay un 20% de probabilidad de que sea ambulancia
    return 2;
}

//Dormir entre la creación de los vehículos según la media de la dist. exponencial
void* creationSleep(void* mean) {
    double exp_mean = *((double*) mean); 
    double r = (double) rand() / RAND_MAX; //r debe estar en el intervalo [0, 1[
    double sleep_time = -exp_mean * log(1-r); //Tiempo entre creación de vehículos
    struct timespec delay = {sleep_time, 0};
    nanosleep(&delay, NULL); //nanosleep es más preciso que sleep
    return NULL;
}

//Creación de los vehículos en el lado este
void* createEastVehicles(void* size) {
    int east_max_size = *((int*) size);
    int i;
    double r, sleep_time;
    // Inicializar los vehículos y crear los hilos
    for (i = 0; i < east_max_size; i++) {   
        east_side.vehicles[i].priority = generatePriority();
        east_side.vehicles[i].speed = 50 + i; // Ejemplo, hay que arreglarlo
        pthread_create(&east_side.threads[i], NULL, printThread, (void*)&east_side.vehicles[i]);
        pthread_mutex_lock(&east_side.size_mutex);
        east_side.size++;
        pthread_mutex_unlock(&east_side.size_mutex);
        //Tiempo de espera
        creationSleep(&east_side.exp_mean);
    }
    for (i = 0; i < MAX_SIZE; i++) {
        pthread_join(east_side.threads[i], NULL);
    }
    return NULL;
}

//Creación de los vehículos en el lado oeste
void* createWestVehicles(void* size) {
    int west_max_size = *((int*) size);
    int i;
    double r, sleep_time;
    // Inicializar los vehículos y crear los hilos
    for (i = 0; i < west_max_size; i++) {   
        west_side.vehicles[i].priority = generatePriority();
        west_side.vehicles[i].speed = 50 + i; // Ejemplo, hay que arreglarlo
        pthread_create(&west_side.threads[i], NULL, printThread, (void*)&west_side.vehicles[i]);
        pthread_mutex_lock(&west_side.size_mutex);
        west_side.size++;
        pthread_mutex_unlock(&west_side.size_mutex);
        //Tiempo de espera
        creationSleep(&west_side.exp_mean);
    }
    for (i = 0; i < MAX_SIZE; i++) {
        pthread_join(west_side.threads[i], NULL);
    }
    return NULL;
}


int main() {
    srand(time(NULL)); //Plantar la semilla para generar números aleatorios
    int west_max_size = 4;
    int east_max_size = 2;
    //Setear los parámetros de cada lado del puente
    initializeBridgeSide();
    //Threads para generar vehículos
    pthread_t generate_west;
    pthread_t generate_east;
    pthread_create(&generate_west, NULL, createWestVehicles, &west_max_size);
    pthread_create(&generate_east, NULL, createEastVehicles, &east_max_size);
    pthread_join(generate_west, NULL);
    pthread_join(generate_east, NULL);
    printf("West vehicles: %d\n", west_side.size);
    printf("East vehicles: %d\n", east_side.size);
    return 0;
}

