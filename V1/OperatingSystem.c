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

	// Create all user processes from the information given in the command line
	//Ejercicio 15 V1
	int ProcesosLYS = OperatingSystem_LongTermScheduler();
	if (ProcesosLYS == 1)
	{
		OperatingSystem_ReadyToShutdown();
	}
	if (strcmp(programList[processTable[sipID].programListIndex]->executableName, "SystemIdleProcess"))
	{
		// Show red message "FATAL ERROR: Missing SIP program!\n"
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

	int PID, i,
		numberOfSuccessfullyCreatedProcesses = 0;

	for (i = 0; programList[i] != NULL && i < PROGRAMSMAXNUMBER; i++)
	{
		PID = OperatingSystem_CreateProcess(i);
		switch (PID)
		{
		case NOFREEENTRY:
			ComputerSystem_DebugMessage(103, ERROR, programList[i]->executableName);
			continue;
		case PROGRAMDOESNOTEXIST:
			ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "it does not exist");
			continue;
		case PROGRAMNOTVALID:
			ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "invalid priority or size");
			continue;
		case TOOBIGPROCESS:
			ComputerSystem_DebugMessage(105, ERROR, programList[i]->executableName);
			continue;
		default:
			numberOfSuccessfullyCreatedProcesses++;
			if (programList[i]->type == USERPROGRAM)
			{
				numberOfNotTerminatedUserProcesses++;
			}
			else
			{
				//numberOfReadyToRunProcesses[DAEMONSQUEUE]++;
			}

			// Move process to the ready state
			OperatingSystem_MoveToTheREADYState(PID);
			continue;
		}
	}

	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}

// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram)
{

	int PID;
	int processSize;
	int loadingPhysicalAddress;
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
	loadingPhysicalAddress = OperatingSystem_ObtainMainMemory(processSize, PID);
	if (loadingPhysicalAddress == TOOBIGPROCESS)
	{
		return TOOBIGPROCESS;
	}
	// Load program in the allocated memory
	int Mierda;
	Mierda = OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize);
	if (Mierda == TOOBIGPROCESS)
	{
		return TOOBIGPROCESS;
	}
	// PCB initialization
	OperatingSystem_PCBInitialization(PID, loadingPhysicalAddress, processSize, priority, indexOfExecutableProgram);
	Change_State(PID, 0, -1);
	// Show message "Process [PID] created from program [executableName]\n"
	ComputerSystem_DebugMessage(70, INIT, PID, executableProgram->executableName);

	return PID;
}

// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID)
{

	if (processSize > MAINMEMORYSECTIONSIZE)
		return TOOBIGPROCESS;

	return PID * MAINMEMORYSECTIONSIZE;
}

// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex)
{

	processTable[PID].busy = 1;
	processTable[PID].initialPhysicalAddress = initialPhysicalAddress;
	processTable[PID].processSize = processSize;
	processTable[PID].state = NEW;
	processTable[PID].priority = priority;
	processTable[PID].programListIndex = processPLIndex;
	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM)
	{
		processTable[PID].copyOfPCRegister = initialPhysicalAddress;
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
	if (Heap_add(PID, readyToRunQueue[processTable[PID].queueID], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[processTable[PID].queueID], PROCESSTABLEMAXSIZE) >= 0)
	{
		processTable[PID].state = READY;
		Change_State(PID, 0, 1);
	}
	OperatingSystem_PrintReadyToRunQueue();
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
	else
	{
		OperatingSystem_ReadyToShutdown();
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
	Change_State(PID, 1, 2);
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
	processTable[PID].copyOfAccumulator = Processor_GetAccumulator();
}

// Exception management routine
void OperatingSystem_HandleException()
{
	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	ComputerSystem_DebugMessage(71, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);

	OperatingSystem_TerminateProcess();
}

// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess()
{

	int selectedProcess;
	Change_State(executingProcessID, 2, 4);
	processTable[executingProcessID].state = EXIT;

	if (programList[processTable[executingProcessID].programListIndex]->type == USERPROGRAM)
	{
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;
		//numberOfReadyToRunProcesses[USERPROCESSQUEUE]--;
	}
	else if (programList[processTable[executingProcessID].programListIndex]->type == DAEMONPROGRAM)
	{
		//numberOfReadyToRunProcesses[DAEMONSQUEUE]--;
	}

	if (numberOfNotTerminatedUserProcesses == 0)
	{
		if (executingProcessID == sipID)
		{
			// finishing sipID, change PC to address of OS HALT instruction
			Processor_CopyInSystemStack(MAINMEMORYSIZE - 1, OS_address_base + 1);
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
		ComputerSystem_DebugMessage(72, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
		break;

	case SYSCALL_END:
		// Show message: "Process [executingProcessID] has requested to terminate\n"
		ComputerSystem_DebugMessage(73, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);

		OperatingSystem_TerminateProcess();
		break;
	case SYSCALL_YIELD: //v1 E12
		la_Magia_Del_Yield(executingProcessID);
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
	}
}

void OperatingSystem_PrintReadyToRunQueue()
{
	ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE, "");
	int cuentaColas;
	for (cuentaColas = 0; cuentaColas < NUMBEROFQUEUES; cuentaColas++)
	{
		int CuentaMierda;
		ComputerSystem_DebugMessage(113, SHORTTERMSCHEDULE, queueNames[cuentaColas]);
		for (CuentaMierda = 0; CuentaMierda < numberOfReadyToRunProcesses[cuentaColas]; CuentaMierda++)
		{
			int proceso = readyToRunQueue[cuentaColas][CuentaMierda].info;//Proceso es el PID
			ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE,proceso, processTable[proceso].priority);
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
		ComputerSystem_DebugMessage(111, SYSPROC, PID1, programList[processTable[PID1].programListIndex]->executableName, statesNames[antiguo]);
	}
	else
	{
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
		if (prioridadEjecutando == prioridadCandidato)
		{
			ceder_voluntariamente_el_control_del_procesador(executingProcessID, cadidatoOoOoOo);
		}
	}
}
void ceder_voluntariamente_el_control_del_procesador(executingProcessID, cadidatoOoOoOo)
{
	ComputerSystem_DebugMessage(115, SHORTTERMSCHEDULE, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, cadidatoOoOoOo, programList[processTable[cadidatoOoOoOo].programListIndex]->executableName);
	OperatingSystem_PreemptRunningProcess();
}