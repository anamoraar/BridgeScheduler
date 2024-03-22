#ifndef VEHICLE_H
#define VEHICLE_H

typedef struct Vehicle {
	int way; //Sale del oeste: 1 y sale del este: 2
	int priority; // Prioridad: 1 para ambulancia, 2 para carro
	double speed;
} Vehicle;

#endif 

