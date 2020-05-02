#include "OperatingSystem.h"
#include "OperatingSystemBase.h"
#include "MMU.h"
#include "Processor.h"
#include "Buses.h"
#include "Heap.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// Functions prototypes
void OperatingSystem_PrepareDaemons();
void OperatingSystem_PCBInitialization(int, int, int, int, int);
void OperatingSystem_MoveToTheREADYState(int);
void OperatingSystem_Dispatch(int);
void OperatingSystem_RestoreContext(int);
void OperatingSystem_SaveContext(int);
void OperatingSystem_TerminateProcess();
int OperatingSystem_LongTermScheduler();
void OperatingSystem_PreemptRunningProcess();
int OperatingSystem_CreateProcess(int);
int OperatingSystem_ObtainMainMemory(int, int);
int OperatingSystem_ShortTermScheduler();
int OperatingSystem_ExtractFromReadyToRun();
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();

// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID = NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Initial PID for assignation
int initialPID = PROCESSTABLEMAXSIZE - 1;
// Esta movida es para el ejercicio 8,
//tiene que ser el ultimo de la tabla de procesos

// Begin indes for daemons in programList
int baseDaemonsInProgramList;

// Array that contains the identifiers of the READY processes
//heapItem readyToRunQueue[PROCESSTABLEMAXSIZE];
//int numberOfReadyToRunProcesses = 0;

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses = 0;

//Ejercicio 10 V1
char *statesNames[5] = {"NEW", "READY", "EXECUTING", "BLOCKED", "EXIT"};
//Ejercicio 11 V1
// In OperatingSystem.h
#define NUMBEROFQUEUES 2
enum TypeOfReadyToRunProcessQueues
{
	USERPROCESSQUEUE,
	DAEMONSQUEUE
};
// In OperatingSystem.c
heapItem readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
int numberOfReadyToRunProcesses[NUMBEROFQUEUES] = {0, 0};
char *queueNames[NUMBEROFQUEUES] = {"USER", "DAEMONS"};

// In OperatingSystem.c Exercise 5-b of V2
// Heap with blocked processes sort by when to wakeup
heapItem sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfSleepingProcesses = 0;
//V3 E1
int PID_para_Procesador = -99;
int NumeroDeParticionesCreadas;
// Initial set of tasks of the OS
void OperatingSystem_Initialize(int daemonsIndex)
{

	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code

	// Obtain the memory requirements of the program
	int processSize = OperatingSystem_ObtainProgramSize(&programFile, "OperatingSystemCode");

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);

	// Process table initialization (all entries are free)
	for (i = 0; i < PROCESSTABLEMAXSIZE; i++)
	{
		processTable[i].busy = 0;
	}
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base + 2);

	// Include in program list  all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);
	//V3 E1
	ComputerSystem_FillInArrivalTimeQueue();
	OperatingSystem_PrintStatus();

	NumeroDeParticionesCreadas = OperatingSystem_InitializePartitionTable();
	// Create all user processes from the information given in the command line
	//Ejercicio 15 V1
	int creados = OperatingSystem_LongTermScheduler();
	if (creados == 1 && OperatingSystem_IsThereANewProgram() == EMPTYQUEUE)
	{
		OperatingSystem_ReadyToShutdown();
	}
	if (strcmp(programList[processTable[sipID].programListIndex]->executableName, "SystemIdleProcess"))
	{
		// Show red message "FATAL ERROR: Missing SIP program!\n"
		OperatingSystem_ShowTime(SHUTDOWN);
		ComputerSystem_DebugMessage(99, SHUTDOWN, "FATAL ERROR: Missing SIP program!\n");
		exit(1);
	}

	// At least, one user process has been created
	// Select the first process that is going to use the processor
	selectedProcess = OperatingSystem_ShortTermScheduler();

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);
}

// Daemon processes are system processes, that is, they work together with the OS.
// The System Idle Process uses the CPU whenever a user process is able to use it
void OperatingSystem_PrepareDaemons(int programListDaemonsBase)
{

	// Include a entry for SystemIdleProcess at 0 position
	programList[0] = (PROGRAMS_DATA *)malloc(sizeof(PROGRAMS_DATA));

	programList[0]->executableName = "SystemIdleProcess";
	programList[0]->arrivalTime = 0;
	programList[0]->type = DAEMONPROGRAM; // daemon program

	sipID = initialPID % PROCESSTABLEMAXSIZE; // first PID for sipID

	// Prepare aditionals daemons here
	// index for aditionals daemons program in programList
	baseDaemonsInProgramList = programListDaemonsBase;
}

// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the
// 			command lineand daemons programs
int OperatingSystem_LongTermScheduler()
{

	int PID, nuevoProceso, numberOfSuccessfullyCreatedProcesses = 0;

	int existe = OperatingSystem_IsThereANewProgram();
	while (existe == YES)
	{
		nuevoProceso = llegasTarde();
		existe = OperatingSystem_IsThereANewProgram();
		PID = OperatingSystem_CreateProcess(nuevoProceso);
		switch (PID)
		{
		case NOFREEENTRY:
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(103, ERROR, programList[nuevoProceso]->executableName);
			continue;
		case PROGRAMDOESNOTEXIST:
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(104, ERROR, programList[nuevoProceso]->executableName, "it does not exist");
			continue;
		case PROGRAMNOTVALID:
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(104, ERROR, programList[nuevoProceso]->executableName, "invalid priority or size");
			continue;
		case TOOBIGPROCESS:
			OperatingSystem_ShowTime(ERROR);
			//ComputerSystem_DebugMessage(105, ERROR, programList[nuevoProceso]->executableName);
			ComputerSystem_DebugMessage(144, ERROR, "TOOBIGPROCESS");
			continue;
		case MEMORYFULL:
			OperatingSystem_ShowTime(ERROR);
			//ComputerSystem_DebugMessage(105, ERROR, programList[nuevoProceso]->executableName);
			ComputerSystem_DebugMessage(144, ERROR, "MEMORYFULL");
			continue;
		default:
			numberOfSuccessfullyCreatedProcesses++;
			if (programList[nuevoProceso]->type == USERPROGRAM)
			{
				numberOfNotTerminatedUserProcesses++;
			}
			OperatingSystem_MoveToTheREADYState(PID);
			break;
		}
	}
	// Return the number of succesfully created processes
	if (numberOfSuccessfullyCreatedProcesses > 0)
	{
		OperatingSystem_PrintStatus();
	}
	return numberOfSuccessfullyCreatedProcesses;
}
// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram)
{

	int PID;
	int processSize;
	int priority;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram = programList[indexOfExecutableProgram];

	// Obtain a process ID
	PID = OperatingSystem_ObtainAnEntryInTheProcessTable();
	if (PID == NOFREEENTRY)
	{
		return NOFREEENTRY;
	}
	// Obtain the memory requirements of the program
	processSize = OperatingSystem_ObtainProgramSize(&programFile, executableProgram->executableName);
	if (processSize == PROGRAMDOESNOTEXIST)
	{
		return PROGRAMDOESNOTEXIST;
	}
	// Obtain the priority for the process
	priority = OperatingSystem_ObtainPriority(programFile);
	if (priority == PROGRAMNOTVALID)
	{
		return PROGRAMNOTVALID;
	}
	// Obtain enough memory space
	ComputerSystem_DebugMessage(142, SYSMEM, PID, executableProgram->executableName, processSize);
	int particion = OperatingSystem_ObtainMainMemory(processSize, PID);
	if (particion == TOOBIGPROCESS)
	{
		return TOOBIGPROCESS;
	}
	if (particion == MEMORYFULL)
	{
		return MEMORYFULL;
	}

	// Load program in the allocated memory
	int Mierda;
	Mierda = OperatingSystem_LoadProgram(programFile, particion, processSize);
	if (Mierda == TOOBIGPROCESS)
	{
		return TOOBIGPROCESS;
	}
	// PCB initialization
	OperatingSystem_ShowPartitionTable("before allocating memory");
	OperatingSystem_PCBInitialization(PID, particion, processSize, priority, indexOfExecutableProgram);
	OperatingSystem_ShowPartitionTable("after allocating memory");
	Change_State(PID, NEW, -1);
	// Show message "Process [PID] created from program [executableName]\n"
	OperatingSystem_ShowTime(INIT);
	ComputerSystem_DebugMessage(143, SYSMEM, processTable[PID].particion, processTable[PID].initialPhysicalAddress, partitionsTable[processTable[PID].particion].size, PID, executableProgram->executableName);
	OperatingSystem_ShowTime(INIT);
	ComputerSystem_DebugMessage(70, INIT, PID, executableProgram->executableName);

	return PID;
}

// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID)
{

	// if (processSize > MAINMEMORYSECTIONSIZE)
	// 	return TOOBIGPROCESS;
	//llamada a la anueva funcion de ajuste de memoria
	//Pensare un nombre guay
	int posicion = elegir_Zapatos(processSize);
	return posicion;

	//return PID * MAINMEMORYSECTIONSIZE;
}

// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int particion, int processSize, int priority, int processPLIndex)
{

	processTable[PID].busy = 1;
	processTable[PID].initialPhysicalAddress = partitionsTable[particion].initAddress;
	processTable[PID].processSize = processSize;
	processTable[PID].state = NEW;
	processTable[PID].priority = priority;
	processTable[PID].programListIndex = processPLIndex;
	processTable[PID].whenToWakeUp = 0;
	processTable[PID].copyOfAccumulator = 0;
	processTable[PID].particion = particion;
	partitionsTable[particion].PID = PID;
	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM)
	{
		processTable[PID].copyOfPCRegister = partitionsTable[particion].initAddress;
		processTable[PID].copyOfPSWRegister = ((unsigned int)1) << EXECUTION_MODE_BIT;
		processTable[PID].queueID = DAEMONSQUEUE;
	}
	else
	{
		processTable[PID].copyOfPCRegister = 0;
		processTable[PID].copyOfPSWRegister = 0;
		processTable[PID].queueID = USERPROCESSQUEUE;
	}
}

// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
// Heap_add(int info, heapItem heap[], int queueType, int *numElem, int limit)
void OperatingSystem_MoveToTheREADYState(int PID)
{
	int estadoAntiguo = processTable[PID].state;
	if (Heap_add(PID, readyToRunQueue[processTable[PID].queueID], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[processTable[PID].queueID], PROCESSTABLEMAXSIZE) >= 0)
	{
		processTable[PID].state = READY;
		Change_State(PID, estadoAntiguo, READY);
	}
	//OperatingSystem_PrintReadyToRunQueue();
}

// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
int OperatingSystem_ShortTermScheduler()
{

	int selectedProcess;

	selectedProcess = OperatingSystem_ExtractFromReadyToRun();

	return selectedProcess;
}

// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromReadyToRun()
{

	int selectedProcess = NOPROCESS;

	//selectedProcess = Heap_poll(readyToRunQueue, QUEUE_PRIORITY, &numberOfReadyToRunProcesses);
	if (numberOfReadyToRunProcesses[USERPROCESSQUEUE] > 0)
	{
		selectedProcess = Heap_poll(readyToRunQueue[USERPROCESSQUEUE], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[USERPROCESSQUEUE]);
	}
	else if (numberOfReadyToRunProcesses[DAEMONSQUEUE] > 0)
	{
		selectedProcess = Heap_poll(readyToRunQueue[DAEMONSQUEUE], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[DAEMONSQUEUE]);
	}
	else if (selectedProcess == NOPROCESS)
	{
		selectedProcess = sipID;
		//OperatingSystem_ReadyToShutdown();
	}
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess;
}

// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID)
{

	// The process identified by PID becomes the current executing process
	executingProcessID = PID;
	// Change the process' state
	processTable[PID].state = EXECUTING;
	Change_State(PID, READY, EXECUTING);
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}

// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID)
{

	// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE - 1, processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE - 2, processTable[PID].copyOfPSWRegister);

	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);

	//Ejercicio 13 V1
	Processor_SetAccumulator(processTable[PID].copyOfAccumulator);
}

// Function invoked when the executing process leaves the CPU
void OperatingSystem_PreemptRunningProcess()
{

	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);
	// The processor is not assigned until the OS selects another process
	executingProcessID = NOPROCESS;
}

// Save in the process' PCB essential values stored in hardware registers and the system stack
void OperatingSystem_SaveContext(int PID)
{

	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister = Processor_CopyFromSystemStack(MAINMEMORYSIZE - 1);

	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister = Processor_CopyFromSystemStack(MAINMEMORYSIZE - 2);
	//Ejercicio 13 V1
	//processTable[PID].copyOfAccumulator = Processor_CopyFromSystemStack(MAINMEMORYSIZE - 3);
	processTable[PID].copyOfAccumulator = Processor_GetAccumulator();
}

// Exception management routine
void OperatingSystem_HandleException()
{

	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	OperatingSystem_ShowTime(SYSPROC);
	//ComputerSystem_DebugMessage(71, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
	//ComputerSystem_DebugMessage(140, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, getExcepcion());
	switch (getExcepcion())
	{
	case DIVISIONBYZERO:
		ComputerSystem_DebugMessage(140, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, "division by zero");
		break;
	case INVALIDPROCESSORMODE:
		ComputerSystem_DebugMessage(140, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, "invalid processor mode");
		break;
	case INVALIDADDRESS:
		ComputerSystem_DebugMessage(140, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, "invalid address");
		break;
	case INVALIDINSTRUCTION:
		ComputerSystem_DebugMessage(140, INTERRUPT, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, "invalid instruction");
		break;
	}

	OperatingSystem_TerminateProcess();
	OperatingSystem_PrintStatus();
}

// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess()
{

	int selectedProcess;
	Change_State(executingProcessID, EXECUTING, EXIT);
	processTable[executingProcessID].state = EXIT;
	//partitionsTable[processTable[executingProcessID].particion].PID = NOPROCESS;
	OperatingSystem_ReleaseMainMemory();
	if (programList[processTable[executingProcessID].programListIndex]->type == USERPROGRAM)
	{
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;
		//numberOfReadyToRunProcesses[USERPROCESSQUEUE]--;
	}

	// if (numberOfNotTerminatedUserProcesses <= 0 && OperatingSystem_IsThereANewProgram() == EMPTYQUEUE)
	if (numberOfNotTerminatedUserProcesses == 0)
	{
		if (executingProcessID == sipID)
		{
			// finishing sipID, change PC to address of OS HALT instruction
			//Processor_CopyInSystemStack(MAINMEMORYSIZE - 1, OS_address_base + 1);
			OperatingSystem_TerminatingSIP();
			OperatingSystem_ShowTime(SHUTDOWN);
			ComputerSystem_DebugMessage(99, SHUTDOWN, "The system will shut down now...\n");
			return; // Don't dispatch any process
		}
		// Simulation must finish, telling sipID to finish
		OperatingSystem_ReadyToShutdown();
	}

	// Select the next process to execute (sipID if no more user processes)
	selectedProcess = OperatingSystem_ShortTermScheduler();
	// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}

// System call management routine
void OperatingSystem_HandleSystemCall()
{

	int systemCallID;

	// Register A contains the identifier of the issued system call
	systemCallID = Processor_GetRegisterA();

	switch (systemCallID)
	{
	case SYSCALL_PRINTEXECPID:
		// Show message: "Process [executingProcessID] has the processor assigned\n"
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(72, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
		break;
	case SYSCALL_END:
		// Show message: "Process [executingProcessID] has requested to terminate\n"
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(73, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
		OperatingSystem_TerminateProcess();
		OperatingSystem_PrintStatus();
		break;
	case SYSCALL_YIELD:							  //v1 E12
		PID_para_Procesador = executingProcessID; //V3 E1
		la_Magia_Del_Yield(executingProcessID);
		break;
	case SYSCALL_SLEEP:							  //v2 E5
		PID_para_Procesador = executingProcessID; //V3 E1
		a_dormir_ostia(executingProcessID);
		OperatingSystem_PrintStatus();
		break;
	default:
		OperatingSystem_ShowTime(INTERRUPT);
		ComputerSystem_DebugMessage(141, INTERRUPT, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, systemCallID);
		OperatingSystem_TerminateProcess();
		OperatingSystem_PrintStatus();
		break;
	}
}

//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint)
{
	switch (entryPoint)
	{
	case SYSCALL_BIT: // SYSCALL_BIT=2
		OperatingSystem_HandleSystemCall();
		break;
	case EXCEPTION_BIT: // EXCEPTION_BIT=6
		OperatingSystem_HandleException();
		break;
	case CLOCKINT_BIT: //9
		OperatingSystem_HandleClockInterrupt();
		break;
	}
}

void OperatingSystem_PrintReadyToRunQueue()
{
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE, "");
	int cuentaColas;
	for (cuentaColas = 0; cuentaColas < NUMBEROFQUEUES; cuentaColas++)
	{
		int CuentaMierda;
		ComputerSystem_DebugMessage(113, SHORTTERMSCHEDULE, queueNames[cuentaColas]);
		for (CuentaMierda = 0; CuentaMierda < numberOfReadyToRunProcesses[cuentaColas]; CuentaMierda++)
		{
			int proceso = readyToRunQueue[cuentaColas][CuentaMierda].info; //Proceso es el PID
			ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE, proceso, processTable[proceso].priority);
			if (CuentaMierda == numberOfReadyToRunProcesses[cuentaColas] - 1)
			{
				//ComputerSystem_DebugMessage(109, SHORTTERMSCHEDULE);
			}
			else
			{
				ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE);
			}
		}
		ComputerSystem_DebugMessage(109, SHORTTERMSCHEDULE);
	}
}
void Change_State(PID1, antiguo, nuevo)
{

	if (statesNames[antiguo] == statesNames[0] && nuevo == -1)
	{
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(111, SYSPROC, PID1, programList[processTable[PID1].programListIndex]->executableName, statesNames[antiguo]);
	}
	else
	{
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(110, SYSPROC, PID1, programList[processTable[PID1].programListIndex]->executableName, statesNames[antiguo], statesNames[nuevo]);
	}
}

void la_Magia_Del_Yield(executingProcessID)
{
	int prioridadEjecutando = processTable[executingProcessID].priority;
	int colaEjecutando = processTable[executingProcessID].queueID;
	//Heap_getFirst(heapItem heap[], int numElem)
	//-1 si peta
	int cadidatoOoOoOo = Heap_getFirst(readyToRunQueue[colaEjecutando], numberOfReadyToRunProcesses[colaEjecutando]);
	if (cadidatoOoOoOo != -1)
	{
		int prioridadCandidato = processTable[cadidatoOoOoOo].priority;
		if (prioridadEjecutando == prioridadCandidato && executingProcessID != cadidatoOoOoOo)
		{
			ceder_voluntariamente_el_control_del_procesador(executingProcessID, cadidatoOoOoOo);
		}
	}
}
void ceder_voluntariamente_el_control_del_procesador(executingProcessID, cadidatoOoOoOo)
{
	int elNUevo;
	elNUevo = Heap_poll(readyToRunQueue[processTable[cadidatoOoOoOo].queueID], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[processTable[cadidatoOoOoOo].queueID]);
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(115, SHORTTERMSCHEDULE, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, elNUevo, programList[processTable[elNUevo].programListIndex]->executableName);
	OperatingSystem_PreemptRunningProcess();
	OperatingSystem_Dispatch(elNUevo);
	OperatingSystem_PrintStatus();
}

// In OperatingSystem.c Exercise 2-b of V2
void OperatingSystem_HandleClockInterrupt()
{
	numberOfClockInterrupts++;
	OperatingSystem_ShowTime(INTERRUPT);
	ComputerSystem_DebugMessage(120, INTERRUPT, numberOfClockInterrupts);
	VAMOS_PANDA_DE_VAGOS();
	OperatingSystem_LongTermScheduler(); //V3 E3
	procesoAlfa();						 //V3 E3b
	if ((numberOfNotTerminatedUserProcesses == 0 && OperatingSystem_IsThereANewProgram() == EMPTYQUEUE))
	{
		//apagarPorLaFuerza();
		OperatingSystem_ReadyToShutdown();
	}
}
void a_dormir_ostia(int PID)
{
	OperatingSystem_SaveContext(PID);
	//int Heap_add(int info, heapItem heap[], int queueType, int *numElem, int limit) {
	int acc = processTable[PID].copyOfAccumulator;
	processTable[PID].whenToWakeUp = abs(acc) + numberOfClockInterrupts + 1;

	if (Heap_add(PID, sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses, PROCESSTABLEMAXSIZE) >= 0)
	{
		processTable[PID].state = BLOCKED;
		Change_State(PID, EXECUTING, BLOCKED);
	}
	executingProcessID = NOPROCESS;
	OperatingSystem_Dispatch(OperatingSystem_ShortTermScheduler());
}
void VAMOS_PANDA_DE_VAGOS()
{
	int vanderaAaAaA = 0;
	int vagos;
	if(numberOfSleepingProcesses>0){
		int pausa=1;
	}
	for (vagos = 0; vagos < numberOfSleepingProcesses; vagos++)
	{
		int madrugador = Heap_getFirst(sleepingProcessesQueue, numberOfSleepingProcesses);
		if (processTable[madrugador].whenToWakeUp <= numberOfClockInterrupts)
		{
			vanderaAaAaA = -1;
			int levantado = Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses);
			OperatingSystem_MoveToTheREADYState(levantado);
		}
	}
	if (vanderaAaAaA != 0)
	{
		OperatingSystem_PrintStatus();
	}
}
void procesoAlfa()
{
	int actual = processTable[executingProcessID].priority;
	int posibleAlfa = Heap_getFirst(readyToRunQueue[processTable[executingProcessID].queueID], numberOfReadyToRunProcesses[processTable[executingProcessID].queueID]);
	int prioridadAlfa;
	if (posibleAlfa != -1)
	{
		prioridadAlfa = processTable[posibleAlfa].priority;
	}
	else
	{
		prioridadAlfa = -1;
	}

	if (prioridadAlfa > actual)
	{
		PID_para_Procesador = posibleAlfa; //Para el procesador
		OperatingSystem_ShowTime(INTERRUPT);
		ComputerSystem_DebugMessage(121, SHORTTERMSCHEDULE, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, posibleAlfa, programList[processTable[posibleAlfa].programListIndex]->executableName);
		OperatingSystem_PreemptRunningProcess(); //Se pira el actual
		//OperatingSystem_ShortTermScheduler();
		int NuevoAlfa = Heap_poll(readyToRunQueue[processTable[posibleAlfa].queueID], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[processTable[posibleAlfa].queueID]);
		OperatingSystem_Dispatch(NuevoAlfa);
		OperatingSystem_PrintStatus();
	}
	else if (executingProcessID == sipID && numberOfReadyToRunProcesses[USERPROCESSQUEUE] > 0)
	{
		int actual = executingProcessID;
		//int NuevoAlfa = Heap_poll(readyToRunQueue[processTable[posibleAlfa].queueID], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[processTable[posibleAlfa].queueID]);
		int NuevoAlfa = Heap_poll(readyToRunQueue[USERPROCESSQUEUE], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[USERPROCESSQUEUE]);
		PID_para_Procesador = NuevoAlfa; //Para el procesador
		OperatingSystem_ShowTime(INTERRUPT);
		ComputerSystem_DebugMessage(121, SHORTTERMSCHEDULE, actual, programList[processTable[actual].programListIndex]->executableName, NuevoAlfa, programList[processTable[NuevoAlfa].programListIndex]->executableName);
		OperatingSystem_PreemptRunningProcess(); //Se pira el actual
		//limpiarProcesador();
		OperatingSystem_Dispatch(NuevoAlfa);
		OperatingSystem_PrintStatus();
	}
	if (numberOfReadyToRunProcesses[DAEMONPROGRAM] > 0 && executingProcessID == sipID && prioridadAlfa < actual)
	{
		PID_para_Procesador = posibleAlfa; //Para el procesador
		OperatingSystem_ShowTime(INTERRUPT);
		ComputerSystem_DebugMessage(121, SHORTTERMSCHEDULE, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, posibleAlfa, programList[processTable[posibleAlfa].programListIndex]->executableName);
		OperatingSystem_PreemptRunningProcess(); //Se pira el actual
		//OperatingSystem_ShortTermScheduler();
		int NuevoAlfa = Heap_poll(readyToRunQueue[processTable[posibleAlfa].queueID], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[processTable[posibleAlfa].queueID]);
		OperatingSystem_Dispatch(NuevoAlfa);
		OperatingSystem_PrintStatus();
	}
}
int OperatingSystem_GetExecutingProcessID(operationCode)
{
	// int ReturnPID;
	// if (PID_para_Procesador != -99)
	// {
	// 	ReturnPID = PID_para_Procesador;
	// 	PID_para_Procesador = -99;
	// }
	// else
	// {
	// 	ReturnPID = executingProcessID;
	// }
	//return ReturnPID;
	return executingProcessID;
}
int llegasTarde()
{
	int lentoDeLosCojones;
	lentoDeLosCojones = Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
	return lentoDeLosCojones;
}
void apagarPorLaFuerza()
{
	int selectedProcess;
	// if (executingProcessID == sipID)
	// {
	// 	// finishing sipID, change PC to address of OS HALT instruction
	// 	Processor_CopyInSystemStack(MAINMEMORYSIZE - 1, OS_address_base + 1);
	// 	OperatingSystem_ShowTime(SHUTDOWN);
	// 	ComputerSystem_DebugMessage(99, SHUTDOWN, "The system will shut down now...\n");
	// 	return; // Don't dispatch any process
	// }
	// Simulation must finish, telling sipID to finish
	OperatingSystem_ReadyToShutdown();

	//Select the next process to execute (sipID if no more user processes)
	selectedProcess = OperatingSystem_ShortTermScheduler();
	// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}
int elegir_Zapatos(talla)
{

	int zapatito = -1;		   //El mas ajustado
	int barcaza = -1;		   //Entra, pero no es la mas ajustada
	int tendero;			   //El contador
	int noHayZapatos = -1;	   //Entradas libres en la tabla
	int tallaDeZapato = talla; //Tamaño del proceso
	for (tendero = 0; tendero < NumeroDeParticionesCreadas; tendero++)
	{
		int hayAlgoEnLaCaja = partitionsTable[tendero].PID;
		//Los zapatos estan, podemos probar a ver si valen
		//(en no subnormal, no hay procesos en la memoria, esta libre)
		if (hayAlgoEnLaCaja == NOPROCESS)
		{
			noHayZapatos = 1;
			int elDeLaCaja = partitionsTable[tendero].size; //Sacamos el zapato
			if (tallaDeZapato <= elDeLaCaja)				//Entra en el zapato
			{
				if (barcaza == -1) //Inicializamos en el que entra
				{
					barcaza = tendero; //El minimo en el que entra
				}
				else
				{
					barcaza = partitionsTable[zapatito].size; //Obtenemos el tamaño del que entra
					if (elDeLaCaja < barcaza)
					{ //Si el de la caja es mas pequeño que el actual, sera el nuevo zapatito
						zapatito = tendero;
					}
				}
				if (zapatito == -1 && barcaza != -1)
				{
					zapatito = barcaza;
				}
			}
		}
	}
	if (zapatito == -1)
	{
		return TOOBIGPROCESS;
	}
	if (noHayZapatos == -1)
	{
		return MEMORYFULL;
	}
	return zapatito;
}
void OperatingSystem_ReleaseMainMemory()
{
	OperatingSystem_ShowPartitionTable("before releasing memory");
	partitionsTable[processTable[executingProcessID].particion].PID = NOPROCESS;
	OperatingSystem_ShowPartitionTable("after releasing memory");
}