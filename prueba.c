#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <math.h> //Para usar log(r) hay que linkear con -lm
#include "vehicle.h" 
#include "semaphore.h"
#include "bridgeSide.h" 

#define MAX_SIZE 100
#define K_CONST 50
//Para poder imprimir si soy ambulancia o carro en terminal
#define VEHICLE_TYPE (vehicle->priority==1 ? "\033[31;1mAmbulancia\033[0m" : "Carro") 
//Para poder imprimir la luz que tiene un semáforo
#define WEST_SEMAPHORE (west_semaphore.light ? "\033[32;1mVerde\033[0m":"\033[31;1mRojo\033[0m")
#define EAST_SEMAPHORE (east_semaphore.light ? "\033[32;1mVerde\033[0m":"\033[31;1mRojo\033[0m")
//Para llamar a la función de entrar al puente que corresponde al modo
#define ENTER_MODE (admin_mode==1 ? enterFIFO : enterSemaphore)

//Modo de administrar el puente -> 1: FIFO, 2: Semáforos, 2: Oficiales de tránsito
int admin_mode;
//Cantidad total de carros que va a tener la simulación
int total_cars;
pthread_mutex_t total_cars_mutex = PTHREAD_MUTEX_INITIALIZER; //Mutex para proteger el total_cars
//Sentido actual del puente
int current_way; 
//Sentido real del puente
int actual_way = 0; 
pthread_mutex_t actual_way_mutex = PTHREAD_MUTEX_INITIALIZER; //Mutex para proteger el actual_way

//Lados del puente
BridgeSide west_side;
BridgeSide east_side;
//Cantidad de carros por cada lado
int west_max_cars, east_max_cars;

//Puente
pthread_mutex_t bridge[MAX_SIZE];
int bridgeLength;

//Semáforos
Semaphore west_semaphore;
Semaphore east_semaphore;

//Contador para determinar la cantidad de carros en el puente
int cars_crossing = 0;
pthread_mutex_t cars_crossing_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex para proteger cars_crossing

//Variable de condición para entrar al puente
pthread_cond_t enter_bridge_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t enter_bridge_mutex = PTHREAD_MUTEX_INITIALIZER;


//Función para cambiar las luces de ambos semáforos
void * changeLight(){
    double west_sleep_time = west_semaphore.time; 
    double east_sleep_time = east_semaphore.time; 
    struct timespec west_delay = {west_sleep_time, 0};
    struct timespec east_delay = {east_sleep_time, 0};
    while(total_cars){   //Se usa la cantidad de carros totales de la simulación para poder determinar hasta cuando se paran los semáforos
        printf("Luz de semáforo oeste es %s\n",WEST_SEMAPHORE);
        printf("Luz de semáforo este es %s\n",EAST_SEMAPHORE);
        nanosleep(&west_delay, NULL);   //Se duerme el hilo 
        pthread_cond_broadcast(&enter_bridge_cond); //Avisa al resto de los hilos que pueden pasar el puente
        west_semaphore.light=!west_semaphore.light;
        east_semaphore.light=!east_semaphore.light;
        printf("Luz de semáforo oeste es %s\n",WEST_SEMAPHORE);
        printf("Luz de semáforo este es %s\n",EAST_SEMAPHORE);
        nanosleep(&east_delay, NULL);
        pthread_cond_broadcast(&enter_bridge_cond);
        west_semaphore.light=!west_semaphore.light;
        east_semaphore.light=!east_semaphore.light;
    }
    pthread_exit(NULL);
}


void addCarsCrossing(){
    pthread_mutex_lock(&cars_crossing_mutex);
        cars_crossing++;
    pthread_mutex_unlock(&cars_crossing_mutex);
}


//Función para restar en las variables correspondientes porque un vehículo salió del puente
void carExiting(){
    pthread_mutex_lock(&cars_crossing_mutex);
    cars_crossing--;
    pthread_mutex_unlock(&cars_crossing_mutex);
    pthread_mutex_lock(&total_cars_mutex);
    total_cars--;
    pthread_mutex_unlock(&total_cars_mutex);
   //Despertar hilos que están esperando si no hay más carros cruzando
    if (admin_mode != 2 && cars_crossing == 0) {
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

//Función para que un vehículo entre (o no) al puente en el modo de semáforos
void* enterSemaphore(void* arg) {
    Vehicle* vehicle = (Vehicle*) arg;
    int ambulance_is_waiting=0; //Variable para determinar si hay una ambulancia esperando en cualquier lado
    pthread_mutex_lock(&enter_bridge_mutex); //Bloquear el acceso a la variable de condición para entrar al puente
    //El thread entra al puente hasta que no haya vehiculos en el puente o estén yendo en la misma dirección o no haya 
    //ambulancias esperando en el lado opuesto si este no es ambulancia o si el semaforo de mi lado esta en rojo
    while ((vehicle->way==1 && vehicle->priority !=1 && west_semaphore.light==0) || (vehicle->way==2 && vehicle->priority !=1 && east_semaphore.light==0) || (cars_crossing != 0 && actual_way != vehicle->way) || (vehicle->priority!=1 && ambulance_is_waiting)) {
        if(vehicle->priority==1){
            ambulance_is_waiting=1; //Si soy ambulancia marco que estoy esperando 
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
    else if(actual_way == 2) travelEastToWest(vehicle);
    pthread_exit(NULL);
}

//Función para determinar que un vehículo entre al puente en el modo FIFO
void* enterFIFO(void* arg) {
    Vehicle* vehicle = (Vehicle*) arg;
    int ambulance_is_waiting=0; //Variable para determinar si hay una ambulancia esperando en cualquier lado
    pthread_mutex_lock(&enter_bridge_mutex); //Bloquear el acceso a la variable de condición para entrar al puente
    //El thread entra al puente hasta que no haya vehiculos en el puente o estén yendo en la misma dirección o no haya 
    //ambulancias esperando en el lado opuesto si este no es ambulancia
    while ((cars_crossing != 0 && actual_way != vehicle->way) || (vehicle->priority!=1 && ambulance_is_waiting)) {
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


//Dormir entre la creación de los vehículos según la media de la dist. exponencial
void* creationSleep(void* mean) {
    double exp_mean = *((double*) mean); 
    double r = (double) rand() / RAND_MAX; //r debe estar en el intervalo [0, 1[
    double sleep_time = -exp_mean * log(1-r); //Tiempo entre creación de vehículos
    struct timespec delay = {sleep_time, 0};
    nanosleep(&delay, NULL); //nanosleep es más preciso que sleep
    return NULL;
}

//Retorna 2 si el vehículo es carro y 1 si es ambulancia, es mucho más probable que sea carro
int generatePriority() {
    int random_number = rand() % 100; //Generar un numero random entre 0 y 99
    if (random_number < 20) return 1; //Hay un 20% de probabilidad de que sea ambulancia
    return 2;
}

double generateSpeed(int i, BridgeSide* side) {  
    //Generar un double random en el intervalo deseado
    double random_speed = ((double)rand()/RAND_MAX)*(side->max_speed-side->min_speed) + side->min_speed;
    if (i < side->max_cars-1) {
        //El valor más grande que le puede quedar a total_speed (si los vehículos que quedan tienen la mínima velocidad)
        double max_total_speed = side->total_speed - (side->max_cars - (i+1)) * side->min_speed;
        //El valor más pequeño que le puede quedar a total_speed (si los vehículos que quedan tienen la máxima velocidad)
        double min_total_speed = side->total_speed - (side->max_cars - (i+1)) * side->max_speed;
        //Si la velocidad generada está fuera del rango [min_total_speed, max_total_speed], se debe ajustar para que la 
        //suma de las velocidades que se están asignando sea max_cars*speed_mean
        if (random_speed < min_total_speed) random_speed = min_total_speed; 
        else if (random_speed > max_total_speed) random_speed = max_total_speed;
    }
    else random_speed = side->total_speed; //Al último carro se le asigna lo que queda
    side->total_speed -= random_speed;
    return random_speed;
}

//Creación de los vehículos en el lado este
void* createEastVehicles(void* size) {
    int east_max_cars = *((int*) size);
    double total_speed = east_max_cars * east_side.speed_mean; //Suma de las velocidades para tener la media indicada
    int i;
    // Inicializar los vehículos y crear los hilos
    for (i = 0; i < east_max_cars; i++) {   
        east_side.vehicles[i].way = 2;
        east_side.vehicles[i].priority = generatePriority();
        east_side.vehicles[i].speed = generateSpeed(i, &east_side);
        pthread_create(&east_side.threads[i], NULL, ENTER_MODE, &east_side.vehicles[i]);
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
        west_side.vehicles[i].speed = generateSpeed(i, &west_side);
        pthread_create(&west_side.threads[i], NULL, ENTER_MODE, &west_side.vehicles[i]);
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

//Inicializar los mutex del puente
void initializeBridge(int size){
    int i;
    for(i = 0; i<size; i++){
        pthread_mutex_init(&bridge[i], NULL);
    }
}


/*Función que se encarga de leer los parámetros del modo semáforo u oficiales de tránsito, además de crear los hilos correspondientes.
Para semáforo:
west_time east_time starts_on (1 si el semáforo oeste inicia en verde o 0 si inicia en rojo)
Para oficiales:
k_west k_east
*/
void otherParameters() {
    //Semáforos
    if(admin_mode == 2) {
       int starts_on;
       double west_time, east_time;
       scanf("%lf %lf %d", &west_time, &east_time, &starts_on);
       west_semaphore.light = starts_on;
       west_semaphore.time = west_time;
       east_semaphore.light = !starts_on;
       east_semaphore.time = east_time;
    }
    //Oficiales
    else {
       int k_west, k_east;
    	scanf("%d %d", &k_west, &k_east);
    	printf("%d %d\n", k_west, k_east);
    }
}

/*Leer los valores necesarios para las simulaciones en el formato:
admin_mode bridgeLength
west_max_cars west_exp_mean west_min_speed west_max_speed west_speed_mean
east_max_cars east_exp_mean east_min_speed east_max_speed east_speed_mean
*/
void readAndSetParameters() {
    scanf("%d %d", &admin_mode, &bridgeLength);
    //Lado oeste del puente
    double west_exp_mean, west_min_speed, west_max_speed, west_speed_mean;
    scanf("%d %lf %lf %lf %lf", &west_max_cars, &west_exp_mean, &west_min_speed, &west_max_speed, &west_speed_mean);
    west_side.size = 0;
    west_side.max_cars = west_max_cars;
    west_side.exp_mean = west_exp_mean;
    west_side.min_speed = west_min_speed;
    west_side.max_speed = west_max_speed;
    west_side.speed_mean = west_speed_mean;
    west_side.total_speed = west_speed_mean*west_max_cars;
    pthread_mutex_init(&(west_side.size_mutex), NULL);
    //Lado este
    double east_exp_mean, east_min_speed, east_max_speed, east_speed_mean;
    scanf("%d %lf %lf %lf %lf", &east_max_cars, &east_exp_mean, &east_min_speed, &east_max_speed, &east_speed_mean);
    east_side.size = 0;
    east_side.max_cars = east_max_cars;
    east_side.exp_mean = east_exp_mean;
    east_side.speed_mean = east_speed_mean;
    east_side.min_speed = east_min_speed;
    east_side.max_speed = east_max_speed;
    east_side.total_speed = east_speed_mean*east_max_cars;
    pthread_mutex_init(&(east_side.size_mutex), NULL);
    total_cars= west_max_cars + east_max_cars;
    //En caso de que se haya elegido semáforos u oficiales de tránsito, se leen los datos correspondientes
    if(admin_mode != 1) otherParameters();
}



int main() {
    srand(time(NULL)); //Plantar la semilla para generar números aleatorios
    //Leer los parámetros generales de la simulación
    readAndSetParameters();
    //Inicializar el puente
    initializeBridge(bridgeLength);
    //Hilos para generar los carros e iniciar la simulación
    pthread_t generate_west;
    pthread_t generate_east;
    pthread_t semaphores;
    if(admin_mode == 2) pthread_create(&semaphores, NULL, changeLight, NULL);
    pthread_create(&generate_west, NULL, createWestVehicles, &west_max_cars);
    pthread_create(&generate_east, NULL, createEastVehicles, &east_max_cars);
    pthread_join(generate_west, NULL);
    pthread_join(generate_east, NULL);
    if(admin_mode == 2) pthread_join(semaphores,NULL);
    printf("East total vehicles: %d\n", east_side.size);
    printf("West total vehicles: %d\n", west_side.size);
    return 0;
}

