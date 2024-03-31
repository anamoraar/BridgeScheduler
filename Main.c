#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <math.h> //Para usar log(r) hay que linkear con -lm
#include "vehicle.h" 
#include "bridgeSide.h" 
#include "semaphore.h"
#include "officer.h"
#include<unistd.h>

#define MAX_SIZE 100
//Constante para calcular el tiempo que dormir cada vehículo
#define SPEED_CONST 70 
//Para imprimir si soy ambulancia o carro en terminal
#define VEHICLE_TYPE (vehicle->priority==1 ? "\033[31;1mAmbulancia\033[0m" : "Carro") 
//Para imprimir la luz que tiene un semáforo
#define WEST_SEMAPHORE (west_semaphore.light ? "\033[32;1mVerde\033[0m":"\033[31;1mRojo\033[0m")
#define EAST_SEMAPHORE (east_semaphore.light ? "\033[32;1mVerde\033[0m":"\033[31;1mRojo\033[0m")
//Para llamar a la función de entrar al puente que corresponde al modo de administración
#define ENTER_MODE (admin_mode == 1 ? enterCarnage : (admin_mode == 2 ? enterSemaphore : enterOfficers))

//Modo de administrar el puente -> 1: FIFO, 2: Semáforos, 3: Oficiales de tránsito
int admin_mode;
//Cantidad total de carros que va a tener la simulación
int total_cars;
//Sentido del puente
int current_way = 0; 
pthread_mutex_t current_way_mutex = PTHREAD_MUTEX_INITIALIZER;

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

//Oficiales
Officer west_officer;
Officer east_officer;

//Contador para determinar la cantidad de carros en el puente
int cars_crossing = 0;
pthread_mutex_t cars_crossing_mutex = PTHREAD_MUTEX_INITIALIZER; 

//Variable de condición para que los vehículos entren al puente
pthread_cond_t enter_bridge_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t enter_bridge_mutex = PTHREAD_MUTEX_INITIALIZER;

//Variable para establecer el sentido del puente según quién llega primero en el modo oficiales de tránsito
int arrived_first = 0;

//Función para que los oficiales dejen pasar vehículos y actualizarlos de ser necesario
void* letCarsPass(){
    while(total_cars){
        if(current_way == 1) { //Oeste
            while(west_officer.k_counter && west_side.current_cars){ 
              pthread_cond_broadcast(&enter_bridge_cond); //Avisar para que vehículos revisen si pueden pasar 
              sleep(1);       
            }
            if ((!west_officer.k_counter || !west_side.current_cars) && !cars_crossing) { //Si ya no tengo carros esperando ni hay carros cruzando le ceda al otro oficial
                pthread_mutex_lock(&current_way_mutex);
                if(east_side.current_cars) current_way = 2; //Si no hay vehículos esperando en el este, que sigan pasando los de mi lado 
                west_officer.k_counter = west_officer.k; //Restablecer cuenta
                pthread_mutex_unlock(&current_way_mutex);
            }
        }   
        if(current_way == 2){ //Este
            while(east_officer.k_counter && east_side.current_cars){ 
              pthread_cond_broadcast(&enter_bridge_cond); //Avisar para que vehículos revisen si pueden pasar 
              sleep(1); 
            }
            if ((!east_officer.k_counter || !east_side.current_cars) && !cars_crossing) { //Si ya no tengo carros esperando ni hay carros cruzando le ceda al otro oficial
                pthread_mutex_lock(&current_way_mutex);
                if(west_side.current_cars) current_way = 1; //Se cambia el sentido si hay vehículos esperando del otro lado
                east_officer.k_counter = east_officer.k; //Restablecer cuenta
                pthread_mutex_unlock(&current_way_mutex);
            }
        }
    }
    pthread_exit(NULL);
}

//Función para cambiar las luces de ambos semáforos
void * changeLight(){
    struct timespec west_sleep;
    west_sleep.tv_sec = (time_t) west_semaphore.time;
    west_sleep.tv_nsec = (long) ((west_semaphore.time - west_sleep.tv_sec)* 1e9);
    struct timespec east_sleep;
    east_sleep.tv_sec = (time_t) east_semaphore.time;
    east_sleep.tv_nsec = (long) ((east_semaphore.time - east_sleep.tv_sec)* 1e9);
    //Se usa la cantidad de carros totales de la simulación para poder determinar hasta cuando se paran los semáforos
    while(total_cars){  
        printf("Luz de semáforo oeste es %s\n",WEST_SEMAPHORE);
        printf("Luz de semáforo este es %s\n",EAST_SEMAPHORE);
        pthread_cond_broadcast(&enter_bridge_cond); //Avisar a los hilos que revisen si pueden pasar el puente
        //Dormir
        if(west_semaphore.light) nanosleep(&west_sleep, NULL);
        else nanosleep(&east_sleep, NULL);
        west_semaphore.light=!west_semaphore.light;
        east_semaphore.light=!east_semaphore.light;
        printf("Luz de semáforo oeste es %s\n",WEST_SEMAPHORE);
        printf("Luz de semáforo este es %s\n",EAST_SEMAPHORE);
        pthread_cond_broadcast(&enter_bridge_cond);
        //Dormir
        if(west_semaphore.light) nanosleep(&west_sleep, NULL);
        else nanosleep(&east_sleep, NULL);
        west_semaphore.light=!west_semaphore.light;
        east_semaphore.light=!east_semaphore.light;
    }
    pthread_exit(NULL);
}

//Función para restar en las variables correspondientes porque un vehículo salió del puente
void carExiting(){
    pthread_mutex_lock(&cars_crossing_mutex);
    cars_crossing--;
    total_cars--;
    pthread_mutex_unlock(&cars_crossing_mutex);
   //Despertar hilos que están esperando si no hay más carros cruzando (excepto en modo semáforos)
    if (admin_mode != 2 && cars_crossing == 0) {
        pthread_cond_broadcast(&enter_bridge_cond);
    }
    //Estado actual: sentido del puente, k_counter y cantidad de carros en lado oeste y este
    if(admin_mode != 2) printf("\033[36;1mVehículos esperando en oeste: %d, \033[35;1mVehículos esperando en este: %d\033[0m\n\n", 
    west_side.current_cars, east_side.current_cars);
    
}

//Función para aumentar la cantidad de vehículos en el puente
void addCarsCrossing(int way){
    pthread_mutex_lock(&cars_crossing_mutex);
        cars_crossing++;
    pthread_mutex_unlock(&cars_crossing_mutex);
    //Si hay un vehículo más en el puente, hay uno menos esperando en su lado
    if(way == 1) {
        pthread_mutex_lock(&west_side.current_cars_mutex);
        west_side.current_cars--;
        if(admin_mode == 3) west_officer.k_counter--; //Un oficial deja pasar desde el oeste
        pthread_mutex_unlock(&west_side.current_cars_mutex);
    }
    else {
        pthread_mutex_lock(&east_side.current_cars_mutex);
        east_side.current_cars--;
        if(admin_mode == 3) east_officer.k_counter--; //Un oficial deja pasar desde el este
        pthread_mutex_unlock(&east_side.current_cars_mutex);    
    }
    
}

//Función para que los carros del este pasen
void* travelEastToWest(void* arg){
    Vehicle* vehicle = (Vehicle*)arg;
    double sleep_time = SPEED_CONST/(vehicle->speed);
    struct timespec sleep;
    sleep.tv_sec = (time_t) sleep_time; //Parte entera
    sleep.tv_nsec = (long) ((sleep_time - sleep.tv_sec) * 1e9); //Decimales en nanosegundos
    pthread_mutex_lock(&bridge[bridgeLength-1]); //Hace lock de la primera posición de este a oeste (la última)
    printf("\033[35;1m<-\033[0m %s %ld entró al puente \n", VEHICLE_TYPE,pthread_self());
    for(int i = bridgeLength-2; i >= 0; i--){
        printf("\033[35;1m<-\033[0m %s %ld está cruzando sección %d \n", VEHICLE_TYPE,pthread_self(), i+1); //La posición de la que ya tiene el lock
        nanosleep(&sleep, NULL);
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
    double sleep_time = SPEED_CONST/(vehicle->speed);
    struct timespec sleep;
    sleep.tv_sec = (time_t) sleep_time; //Parte entera
    sleep.tv_nsec = (long) ((sleep_time - sleep.tv_sec) * 1e9); //Decimales en nanosegundos
    pthread_mutex_lock(&bridge[0]); //Hace lock de la primera posición de oeste a este 
    printf("\033[36;1m->\033[0m %s %ld entró al puente \n",VEHICLE_TYPE ,pthread_self());
    for(int i = 1; i < bridgeLength; i++){
        printf("\033[36;1m->\033[0m %s %ld está cruzando sección %d \n",VEHICLE_TYPE ,pthread_self(), i-1); //Posición de la que se tiene lock actualmente
        nanosleep(&sleep, NULL);
        pthread_mutex_lock(&bridge[i]); //Se intenta hacer lock de la siguiente posición
        pthread_mutex_unlock(&bridge[i-1]); //Se libera la anterior
    }
    printf("\033[36;1m->\033[0m %s %ld está cruzando sección %d \n",VEHICLE_TYPE, pthread_self(), bridgeLength-1);
    pthread_mutex_unlock(&bridge[bridgeLength-1]);
    printf("\033[36;1m->\033[0m %s %ld salió del puente \n", VEHICLE_TYPE,pthread_self());
    carExiting(); //hay un vehículo menos
}

//Función para dejar que un vehículo entre al puente en el modo oficiales 
void* enterOfficers(void* arg) {
    Vehicle* vehicle = (Vehicle*) arg;
    int ambulance_is_waiting = 0; //Variable para determinar si hay una ambulancia esperando en cualquier lado
    pthread_mutex_lock(&enter_bridge_mutex); 
    //Se esperan si el oficial no debe dejar pasar más vehículos, si el sentido es diferente al mío o hay una ambulancia esperando
    while ((vehicle->way == 1 ? !west_officer.k_counter : !east_officer.k_counter) || vehicle->way != current_way 
    || (vehicle->priority != 1 && ambulance_is_waiting)) {
        if(vehicle->priority == 1) ambulance_is_waiting = 1; //Si soy ambulancia marco que estoy esperando 
        if(!arrived_first) { //El primer vehículo determina cuál oficial deja pasar primero
        	pthread_mutex_lock(&current_way_mutex);
        	arrived_first = 1;
        	current_way = vehicle->way;
        	pthread_mutex_unlock(&current_way_mutex);
        }
        pthread_cond_wait(&enter_bridge_cond, &enter_bridge_mutex); //Esperar a que un oficial me deje pasar
    }
    addCarsCrossing(vehicle->way); //Un vehículo más en el puente
    if(vehicle->priority == 1){ 
        ambulance_is_waiting = 0;
        pthread_mutex_lock(&current_way_mutex);
        current_way = vehicle->way;
        pthread_mutex_unlock(&current_way_mutex);
    }
    pthread_mutex_unlock(&enter_bridge_mutex); // Desbloquea el mutex de la cond cuando entra al puente
    //Pasar el puente en la dirección que corresponda 
    if(current_way == 1) travelWestToEast(vehicle);
    else travelEastToWest(vehicle);
    pthread_exit(NULL);
} 


//Función para que un vehículo entre (o no) al puente en el modo de semáforos
void* enterSemaphore(void* arg) {
    Vehicle* vehicle = (Vehicle*) arg;
    int ambulance_is_waiting=0; //Variable para determinar si hay una ambulancia esperando en cualquier lado
    pthread_mutex_lock(&enter_bridge_mutex); //Bloquear el acceso a la variable de condición para entrar al puente
    //El thread entra al puente hasta que no haya vehiculos en el puente o estén yendo en la misma dirección o no haya 
    //ambulancias esperando en el lado opuesto si este no es ambulancia o si el semaforo de mi lado esta en rojo
    while ((vehicle->way==1 && vehicle->priority !=1 && west_semaphore.light==0) || (vehicle->way==2 && vehicle->priority !=1 && east_semaphore.light==0) || (cars_crossing != 0 && current_way != vehicle->way) || (vehicle->priority!=1 && ambulance_is_waiting)) {
        if(vehicle->priority==1){
            ambulance_is_waiting=1; //Si soy ambulancia marco que estoy esperando 
        }
        pthread_cond_wait(&enter_bridge_cond, &enter_bridge_mutex); //Esperar en la variable de condición
    }
    //Puede entrar
    addCarsCrossing(vehicle->way); //Un vehículo más en el puente
    if(current_way != vehicle->way || vehicle->priority==1) { //Actualiza el sentido del puente en caso de ser necesario
        pthread_mutex_lock(&current_way_mutex);
        current_way = vehicle->way;
        pthread_mutex_unlock(&current_way_mutex);
    }
    if(vehicle->priority==1){ 
        ambulance_is_waiting=0;
        pthread_cond_broadcast(&enter_bridge_cond);  //Despierta a los hilos que estaban esperando
    }
    pthread_mutex_unlock(&enter_bridge_mutex); // Desbloquea el mutex antes de salir de la función
    //Pasar el puente en la dirección que corresponda 
    if(current_way == 1) travelWestToEast(vehicle);
    else if(current_way == 2) travelEastToWest(vehicle);
    pthread_exit(NULL);
}

//Función para determinar que un vehículo entre al puente en el modo FIFO
void* enterCarnage(void* arg) {
    Vehicle* vehicle = (Vehicle*) arg;
    int ambulance_is_waiting = 0; //Variable para determinar si hay una ambulancia esperando en cualquier lado
    pthread_mutex_lock(&enter_bridge_mutex); //Bloquear el acceso a la variable de condición para entrar al puente
    //El thread entra al puente hasta que no haya vehiculos en el puente o estén yendo en la misma dirección o no haya 
    //ambulancias esperando en el lado opuesto si este no es ambulancia
    while ((cars_crossing != 0 && current_way != vehicle->way) || (vehicle->priority != 1 && ambulance_is_waiting)) {
        if(vehicle->priority == 1) ambulance_is_waiting = 1; //Si soy ambulancia marco que estoy esperando 
        pthread_cond_wait(&enter_bridge_cond, &enter_bridge_mutex); //Esperar en la variable de condición
    }
    //Puede entrar
    addCarsCrossing(vehicle->way); //Un vehículo más en el puente
    if(current_way != vehicle->way || vehicle->priority == 1) { //Actualiza el sentido del puente en caso de ser necesario
        pthread_mutex_lock(&current_way_mutex);
        current_way = vehicle->way;
        pthread_mutex_unlock(&current_way_mutex);
    }
    if(vehicle->priority == 1){ 
        ambulance_is_waiting = 0;
        pthread_cond_broadcast(&enter_bridge_cond);  //Despierta a los hilos que estaban esperando
    }
    pthread_mutex_unlock(&enter_bridge_mutex); // Desbloquea el mutex antes de entrar al puente
    //Pasar el puente en la dirección que corresponda 
    if(current_way == 1) travelWestToEast(vehicle);
    else travelEastToWest(vehicle);
    pthread_exit(NULL);
}


//Dormir entre la creación de los vehículos según la media de la distribución exponencial dada
void* creationSleep(double exp_mean) {
    double r = (double) rand() / RAND_MAX; //r debe estar en el intervalo [0, 1[
    double sleep_time = -exp_mean * log(1-r); //Tiempo entre creación de vehículos
    struct timespec sleep;
    sleep.tv_sec = (time_t) sleep_time; //Parte entera
    sleep.tv_nsec = (long) ((sleep_time - sleep.tv_sec) * 1e9); //Nanosegundos
    nanosleep(&sleep, NULL); //Dormir
    return NULL;
}
//Retorna 2 si el vehículo es carro y 1 si es ambulancia
int generatePriority() {
    int random_number = rand() % 100; //Generar un número random entre 0 y 99
    if (random_number < 15) return 1; //Hay un 15% de probabilidad de que sea ambulancia
    return 2;
}

//Función que genera las velocidades de los vehículos según el rango y la media dada
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


//Creación de los vehículos 
void* createVehicles(void* sidePtr) {
    BridgeSide* side = (BridgeSide*) sidePtr;
    int side_max_cars = side->max_cars;
    // Inicializar los vehículos y crear los hilos
    for (int i = 0; i < side_max_cars; i++) {   
        //Tiempo de espera
        creationSleep(side->exp_mean);
        side->vehicles[i].way = side->way;
        side->vehicles[i].priority = generatePriority();
        side->vehicles[i].speed = generateSpeed(i, side);
        pthread_create(&side->threads[i], NULL, ENTER_MODE, &side->vehicles[i]);
        pthread_mutex_lock(&side->current_cars_mutex);
        side->current_cars++;
        pthread_mutex_unlock(&side->current_cars_mutex);
    }
    for (int i = 0; i < side_max_cars; i++) {
        pthread_join(side->threads[i], NULL);
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
    	west_officer.k = west_officer.k_counter = k_west;
    	east_officer.k = east_officer.k_counter = k_east;
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
    west_side.way = 1;
    west_side.max_cars = west_max_cars;
    west_side.exp_mean = west_exp_mean;
    west_side.min_speed = west_min_speed;
    west_side.max_speed = west_max_speed;
    west_side.speed_mean = west_speed_mean;
    west_side.total_speed = west_speed_mean*west_max_cars;
    pthread_mutex_init(&(west_side.current_cars_mutex), NULL);
    //Lado este
    double east_exp_mean, east_min_speed, east_max_speed, east_speed_mean;
    scanf("%d %lf %lf %lf %lf", &east_max_cars, &east_exp_mean, &east_min_speed, &east_max_speed, &east_speed_mean);
    east_side.way = 2;
    east_side.max_cars = east_max_cars;
    east_side.exp_mean = east_exp_mean;
    east_side.speed_mean = east_speed_mean;
    east_side.min_speed = east_min_speed;
    east_side.max_speed = east_max_speed;
    east_side.total_speed = east_speed_mean*east_max_cars;
    pthread_mutex_init(&(east_side.current_cars_mutex), NULL);
    total_cars = west_max_cars + east_max_cars;
    west_side.current_cars = east_side.current_cars = 0;
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
    pthread_t officers;
    if(admin_mode == 2) pthread_create(&semaphores, NULL, changeLight, NULL);
    if(admin_mode == 3) pthread_create(&officers, NULL, letCarsPass, NULL);
    pthread_create(&generate_west, NULL, createVehicles, &west_side);
    pthread_create(&generate_east, NULL, createVehicles, &east_side);
    pthread_join(generate_west, NULL);
    pthread_join(generate_east, NULL);
    if(admin_mode == 2) pthread_join(semaphores,NULL);
    if(admin_mode == 3) pthread_join(officers, NULL);
    printf("----Fin----\n");
    return 0;
}

