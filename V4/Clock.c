#include "Clock.h"
#include "Processor.h"
#include "ComputerSystemBase.h"
#include "OperatingSystemBase.h"
int tics=0;


void Clock_Update() {
	tics++;
    // ComputerSystem_DebugMessage(97,CLOCK,tics);
	if(tics%intervalBetweenInterrupts==0){
		Processor_RaiseInterrupt(CLOCKINT_BIT);
	}
	//Para establecer una pausa en la depuracion cuando se quiera
	int puntoDePausa=146;
	if(tics == puntoDePausa){
		puntoDePausa++;
	}
}


int Clock_GetTime() {

	return tics;
}
