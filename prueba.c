#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <math.h> //Para usar log(r) hay que linkear con -lm
#include "vehicle.h" 
#include "bridgeSide.h" 

#define MAX_SIZE 100

//Modo de administrar el puente
int admin_type;
//Sentido actual del puente
int current_way; 
//Sentido real del puente
int actual_way; 

//Lados del puente
BridgeSide west_side;
BridgeSide east_side;

//Puente
pthread_mutex_t bridge[MAX_SIZE];
int bridgeLength = 4;

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


//Inicializar los mutex del puente
void initializeBridge(int size){
    int i;
    for(i = 0; i<size; i++){
        pthread_mutex_init(&bridge[i], NULL);
    }
}

/*void* printThread(void* arg) {
    Vehicle* vehicle = (Vehicle*)arg;
    printf("Thread %ld executing with priority %d and speed %d\n", pthread_self(), vehicle->priority, vehicle->speed);
    pthread_exit(NULL);
}*/

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


//Función para que los carros del este pasen
void* travelEastToWest(void* arg){
    Vehicle* vehicle = (Vehicle*)arg;
    double sleep_time = 50/(vehicle->speed);
    struct timespec delay = {sleep_time, 0};
    int i;
    pthread_mutex_lock(&bridge[bridgeLength-1]); //Hace lock de la primera posición de este a oeste (la última)
    printf("<- Carro %ld entró al puente \n",pthread_self());
    for(i = bridgeLength-2; i >= 0; i--){
        printf("<- Carro %ld está cruzando sección %d \n", pthread_self(), i+1); //La posición de la que ya tiene el lock
        nanosleep(&delay, NULL);
        pthread_mutex_lock(&bridge[i]); //i es la siguiente posición
        pthread_mutex_unlock(&bridge[i+1]); //Hasta que se tiene la siguiente posición se libera la anterior
    }
    printf("<- Carro %ld está cruzando sección 0 \n", pthread_self());
    pthread_mutex_unlock(&bridge[0]);
    printf("<- Carro %ld salió del puente \n",pthread_self());
    pthread_exit(NULL);
}

//Creación de los vehículos en el lado este
void* createEastVehicles(void* size) {
    int east_max_size = *((int*) size);
    int i;
    // Inicializar los vehículos y crear los hilos
    for (i = 0; i < east_max_size; i++) {   
        east_side.vehicles[i].priority = generatePriority();
        east_side.vehicles[i].speed = 50 + i; // Ejemplo, hay que arreglarlo
        pthread_create(&east_side.threads[i], NULL, travelEastToWest, (void*)&east_side.vehicles[i]);
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

//Función para que los carros del oeste pasen el puente
void* travelWestToEast(void* arg){
    Vehicle* vehicle = (Vehicle*)arg;
    double sleep_time = 50/(vehicle->speed);
    struct timespec delay = {sleep_time, 0};
    int i;
    pthread_mutex_lock(&bridge[0]); //Hace lock de la primera posición de oeste a este 
    printf("-> Carro %ld entró al puente \n",pthread_self());
    for(i = 1; i < bridgeLength; i++){
        printf("-> Carro %ld está cruzando sección %d \n", pthread_self(), i-1); //Imprime la posición de la que se tiene lock actualmente
        nanosleep(&delay, NULL);
        pthread_mutex_lock(&bridge[i]); //Se intenta hacer lock de la siguiente posición
        pthread_mutex_unlock(&bridge[i-1]); //Se libera la anterior
    }
    printf("-> Carro %ld está cruzando sección %d \n", pthread_self(), bridgeLength-1);
    pthread_mutex_unlock(&bridge[bridgeLength-1]);
    printf("-> Carro %ld salió del puente \n",pthread_self());
    pthread_exit(NULL);
}


//Creación de los vehículos en el lado oeste
void* createWestVehicles(void* size) {
    int west_max_size = *((int*) size);
    int i;
    // Inicializar los vehículos y crear los hilos
    for (i = 0; i < west_max_size; i++) {   
        west_side.vehicles[i].priority = generatePriority();
        west_side.vehicles[i].speed = 50 + i; // Ejemplo, hay que arreglarlo
        pthread_create(&west_side.threads[i], NULL, travelWestToEast, &west_side.vehicles[i]);
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
    int west_max_cars = 3;
    int east_max_cars = 2;
    //Setear los parámetros de cada lado del puente
    initializeBridgeSide();
    //Inicializar el puente
    initializeBridge(bridgeLength);
    //Threads para generar vehículos
    //pthread_t generate_west;
    pthread_t generate_east;
    //pthread_create(&generate_west, NULL, createWestVehicles, &west_max_cars);
    pthread_create(&generate_east, NULL, createEastVehicles, &east_max_cars);
    //pthread_join(generate_west, NULL);
    pthread_join(generate_east, NULL);
    //printf("West vehicles: %d\n", west_side.size);
    printf("East vehicles: %d\n", east_side.size);
    return 0;
}

