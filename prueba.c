#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <math.h> //Para usar log(r) hay que linkear con -lm
#include "vehicle.h" 
#include "bridgeSide.h" 

#define MAX_SIZE 100
#define K_CONST 50
#define VEHICLE_TYPE (vehicle->priority==1 ? "\033[31;1mAmbulancia\033[0m" : "Carro") //Para poder imprimir si soy ambulancia o carro en terminal

//Modo de administrar el puente
int admin_type;
//Sentido actual del puente
int current_way; 
//Sentido real del puente
int actual_way = 0; 
pthread_mutex_t actual_way_mutex = PTHREAD_MUTEX_INITIALIZER; //Mutex para proteger el actual_way

//Lados del puente
BridgeSide west_side;
BridgeSide east_side;

//Puente
pthread_mutex_t bridge[MAX_SIZE];
int bridgeLength = 4;

//Contador para determinar la cantidad de carros en el puente
int cars_crossing = 0;
pthread_mutex_t cars_crossing_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex para proteger cars_crossing

pthread_cond_t enter_bridge_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t enter_bridge_mutex = PTHREAD_MUTEX_INITIALIZER;


//Setear los parámetros que se dan en la entrada para cada lado del puente
void initializeBridgeSide() {
    //Parámetros del lado oeste del puente (en realidad se deben leer de consola)
    double west_exp_mean = 3;
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

void addCarsCrossing(){
    pthread_mutex_lock(&cars_crossing_mutex);
        cars_crossing++;
    pthread_mutex_unlock(&cars_crossing_mutex);
}


//Funcion para determinar que un carro salio del puente
void carExiting(){
    pthread_mutex_lock(&cars_crossing_mutex);
    cars_crossing--;
    pthread_mutex_unlock(&cars_crossing_mutex);
   // Desbloquear hilos si no hay más carros cruzando
    if (cars_crossing == 0) {
        pthread_cond_broadcast(&enter_bridge_cond);
    }
}

//Función para que los carros del este pasen
void* travelEastToWest(void* arg){
    Vehicle* vehicle = (Vehicle*)arg;
    double sleep_time = K_CONST/(vehicle->speed);
    struct timespec delay = {sleep_time, 0};
    int i;
    pthread_mutex_lock(&bridge[bridgeLength-1]); //Hace lock de la primera posición de este a oeste (la última)
    printf("\033[35;1m<-\033[0m %s %ld entró al puente \n", VEHICLE_TYPE,pthread_self());
    for(i = bridgeLength-2; i >= 0; i--){
        printf("\033[35;1m<-\033[0m %s %ld está cruzando sección %d \n", VEHICLE_TYPE,pthread_self(), i+1); //La posición de la que ya tiene el lock
        nanosleep(&delay, NULL);
        pthread_mutex_lock(&bridge[i]); //i es la siguiente posición
        pthread_mutex_unlock(&bridge[i+1]); //Hasta que se tiene la siguiente posición se libera la anterior
    }
    printf("\033[35;1m<-\033[0m %s %ld está cruzando sección 0 \n", VEHICLE_TYPE,pthread_self());
    pthread_mutex_unlock(&bridge[0]);
    printf("\033[35;1m<-\033[0m %s %ld salió del puente \n",VEHICLE_TYPE, pthread_self());
    
    carExiting();
}


//Función para que los carros del oeste pasen el puente
void* travelWestToEast(void* arg){
    Vehicle* vehicle = (Vehicle*)arg;
    double sleep_time = K_CONST/(vehicle->speed);
    struct timespec delay = {sleep_time, 0};
    int i;
    pthread_mutex_lock(&bridge[0]); //Hace lock de la primera posición de oeste a este 
    printf("\033[36;1m->\033[0m %s %ld entró al puente \n",VEHICLE_TYPE ,pthread_self());
    for(i = 1; i < bridgeLength; i++){
        printf("\033[36;1m->\033[0m %s %ld está cruzando sección %d \n",VEHICLE_TYPE ,pthread_self(), i-1); //Posición de la que se tiene lock actualmente
        nanosleep(&delay, NULL);
        pthread_mutex_lock(&bridge[i]); //Se intenta hacer lock de la siguiente posición
        pthread_mutex_unlock(&bridge[i-1]); //Se libera la anterior
    }
    printf("\033[36;1m->\033[0m %s %ld está cruzando sección %d \n",VEHICLE_TYPE, pthread_self(), bridgeLength-1);
    pthread_mutex_unlock(&bridge[bridgeLength-1]);
    printf("\033[36;1m->\033[0m %s %ld salió del puente \n", VEHICLE_TYPE,pthread_self());
    carExiting(); //hay un vehículo menos
}

//Función para determinar que un carro entro al puente o no
void* carEntering(void* arg) {
    Vehicle* vehicle = (Vehicle*) arg;
    int ambulance_is_waiting=0; //Variable para determinar si hay una ambulancia esperando en cualquier lado
    pthread_mutex_lock(&enter_bridge_mutex); //Bloquear el acceso a la variable de condición para entrar al puente
    //El thread entra al puente hasta que no haya vehiculos en el puente o estén yendo en la misma dirección o no haya 
    //ambulancias esperando en el lado opuesto si este no es ambulancia
    while (cars_crossing != 0 && actual_way != vehicle->way || vehicle->priority!=1 && ambulance_is_waiting) {
        if(vehicle->priority==1){
            ambulance_is_waiting = 1; //Si soy ambulancia marco que estoy esperando 
        }
        pthread_cond_wait(&enter_bridge_cond, &enter_bridge_mutex); //Esperar en la variable de condición
    }
    //Puede entrar
    addCarsCrossing(); //Un vehículo más en el puente
    if(actual_way != vehicle->way || vehicle->priority==1) { //Actualiza el sentido del puente en caso de ser necesario
        pthread_mutex_lock(&actual_way_mutex);
        actual_way = vehicle->way;
        pthread_mutex_unlock(&actual_way_mutex);
    }
    if(vehicle->priority==1){ 
        ambulance_is_waiting=0;
        pthread_cond_broadcast(&enter_bridge_cond);  //Despierta a los hilos que estaban esperando
    }
    pthread_mutex_unlock(&enter_bridge_mutex); // Desbloquea el mutex antes de salir de la función
    //Pasar el puente en la dirección que corresponda 
    if(actual_way == 1) travelWestToEast(vehicle);
    else travelEastToWest(vehicle);
    pthread_exit(NULL);
}


//Creación de los vehículos en el lado este
void* createEastVehicles(void* size) {
    int east_max_cars = *((int*) size);
    int i;
    // Inicializar los vehículos y crear los hilos
    for (i = 0; i < east_max_cars; i++) {   
        east_side.vehicles[i].way = 2;
        east_side.vehicles[i].priority = generatePriority();
        east_side.vehicles[i].speed = 50 + i; // Ejemplo, hay que arreglarlo
        pthread_create(&east_side.threads[i], NULL, carEntering, &east_side.vehicles[i]);
        pthread_mutex_lock(&east_side.size_mutex);
        east_side.size++;
        pthread_mutex_unlock(&east_side.size_mutex);
        //Tiempo de espera
        creationSleep(&east_side.exp_mean);
    }
    for (i = 0; i < east_max_cars; i++) {
        pthread_join(east_side.threads[i], NULL);
    }
    return NULL;
}


//Creación de los vehículos en el lado oeste
void* createWestVehicles(void* size) {
    int west_max_cars = *((int*) size);
    int i;
    // Inicializar los vehículos y crear los hilos
    for (i = 0; i < west_max_cars; i++) {   
        west_side.vehicles[i].way = 1;
        west_side.vehicles[i].priority = generatePriority();
        west_side.vehicles[i].speed = 50 + i; // Ejemplo, hay que arreglarlo
        pthread_create(&west_side.threads[i], NULL, carEntering, &west_side.vehicles[i]);
        pthread_mutex_lock(&west_side.size_mutex);
        west_side.size++;
        pthread_mutex_unlock(&west_side.size_mutex);
        //Tiempo de espera
        creationSleep(&west_side.exp_mean);
    }
    for (i = 0; i < west_max_cars; i++) {
        pthread_join(west_side.threads[i], NULL);
    }
    return NULL;
}



int main() {
    srand(time(NULL)); //Plantar la semilla para generar números aleatorios
    int west_max_cars = 2;
    int east_max_cars = 2;
    //Setear los parámetros de cada lado del puente
    initializeBridgeSide();
    //Inicializar el puente
    initializeBridge(bridgeLength);
    //Threads para generar vehículos
    pthread_t generate_west;
    pthread_t generate_east;
    pthread_create(&generate_west, NULL, createWestVehicles, &west_max_cars);
    pthread_create(&generate_east, NULL, createEastVehicles, &east_max_cars);
    pthread_join(generate_west, NULL);
    pthread_join(generate_east, NULL);
    printf("Fin \n");
    return 0;
}

