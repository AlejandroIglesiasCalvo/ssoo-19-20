#ifndef OPERATINGSYSTEM_H
#define OPERATINGSYSTEM_H

#include "ComputerSystem.h"
#include <stdio.h>

#define SUCCESS 1
#define PROGRAMDOESNOTEXIST -1
#define PROGRAMNOTVALID -2

// In this version, every process occupies a 60 positions main memory chunk
// so we can use 60 positions for OS code and the system stack
#define MAINMEMORYSECTIONSIZE (MAINMEMORYSIZE / (PROCESSTABLEMAXSIZE + 1))

#define NOFREEENTRY -3
#define TOOBIGPROCESS -4

#define NOPROCESS -1
#define SLEEPINGQUEUE

// Partitions configuration file name definition
#define MEMCONFIG "MemConfig" // in OperatingSystem.h 
#define MEMORYFULL -5 // In OperatingSystem.h
// Contains the possible type of programs
enum ProgramTypes
{
	USERPROGRAM,
	DAEMONPROGRAM
};

// Enumerated type containing all the possible process states
enum ProcessStates
{
	NEW,
	READY,
	EXECUTING,
	BLOCKED,
	EXIT
};

// Enumerated type containing the list of system calls and their numeric identifiers
enum SystemCallIdentifiers
{
	SYSCALL_END = 3,
	SYSCALL_YIELD = 4,
	SYSCALL_PRINTEXECPID = 5,
	SYSCALL_SLEEP=7
};

// Examen-Mayo 2020 
// A PCB contains all of the information about a process that is needed by the OS
typedef struct
{
	int busy;
	int initialPhysicalAddress;
	int processSize;
	int state;
	int priority;
	int copyOfPCRegister;
	unsigned int copyOfPSWRegister;
	int programListIndex;
	int queueID;
	int copyOfAccumulator;//Ejercicio 13 V1
	int whenToWakeUp; // Exercise 5-a of V2
	int particion;
	int TiempoEspera;//Para guardar el tiempo de espera

} PCB;

// These "extern" declaration enables other source code files to gain access
// to the variable listed
extern PCB processTable[];
extern int OS_address_base;
extern int sipID;

// Functions prototypes
void OperatingSystem_Initialize();
void OperatingSystem_InterruptLogic(int);
//Funciones mias
void OperatingSystem_PrintReadyToRunQueue(); //V1-E9
void Change_State(int, int, int);
void la_Magia_Del_Yield(int);
void ceder_voluntariamente_el_control_del_procesador(int, int);
// In OperatingSystem.c Exercise 2-b of V2
void OperatingSystem_HandleClockInterrupt();

//V2 E4
int numberOfClockInterrupts;
//V2 E5
void a_dormir_ostia(int);
//V2 E6
void VAMOS_PANDA_DE_VAGOS();
void procesoAlfa();
//V3 E1
int OperatingSystem_GetExecutingProcessID(int);
//V3 E2
int llegasTarde();
void apagarPorLaFuerza();
//V4 E6
int elegir_Zapatos(int processSize);
void OperatingSystem_ReleaseMainMemory();
// Examen-Mayo 2020 
void Lentos();
#endif
