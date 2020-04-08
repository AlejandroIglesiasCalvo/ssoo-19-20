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
// V1 Ejercicio11c
int OperatingSystem_ExtractFromReadyToRun(int);
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();

// V1 Ejercicio 9
void OperatingSystem_PrintReadyToRunQueue();

// V1 Ejercicio11b
void OperatingSystem_PrintQueue(int);

// In OperatingSystem.c Exercise 2-b of V2
void OperatingSystem_HandleClockInterrupt();

void OperatingSystem_ChangeProcess(int);

// V1 Ejercicio 10a
char *statesNames[5] = {"NEW", "READY", "EXECUTING", "BLOCKED", "EXIT"};

// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID = NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Begin indes for daemons in programList
int baseDaemonsInProgramList;

// V1 Ejercicio 11a
// In OperatingSystem.c
int readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
int numberOfReadyToRunProcesses[NUMBEROFQUEUES] = {0, 0};
char *queueNames[NUMBEROFQUEUES] = {"USER", "DAEMONS"};

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses = 0;

// V2 Ejercicio 4
int numberOfClockInterrupts = 0;

// In OperatingSystem.c Exercise 5-b of V2
// Heap with blocked processes sort by when to wakeup
int sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfSleepingProcesses = 0;

// V2 Ejercicio 5d
void OperatingSystem_MoveTotheBlockedState(int);

// V2 Ejercicio 6d
int OperatingSystem_ExtractFromBlockedToReady();

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
		processTable[i].busy = 0;

	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base + 1);

	// Create all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);

	// V3 Ejercicio 0c
	ComputerSystem_FillInArrivalTimeQueue();

	// V3 Ejercicio 0d
	OperatingSystem_PrintStatus();

	// Create all user processes from the information given in the command line
	OperatingSystem_LongTermScheduler();

	// V1 Ejercicio 14
	// V3 Ejercicio 04a
	if (numberOfNotTerminatedUserProcesses == 0 && numberOfProgramsInArrivalTimeQueue == 0)
	{
		OperatingSystem_ReadyToShutdown();
	}

	if (strcmp(programList[processTable[sipID].programListIndex]->executableName, "SystemIdleProcess"))
	{
		// V2 Ejercicio 1
		OperatingSystem_ShowTime(SHUTDOWN);
		// Show message "ERROR: Missing SIP program!\n"
		ComputerSystem_DebugMessage(21, SHUTDOWN);
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

	sipID = INITIALPID % PROCESSTABLEMAXSIZE; // first PID for sipID

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

	// V3 Ejercicio 02
	while (OperatingSystem_IsThereANewProgram() == 1)
	{
		// V3 Ejercicio 02
		i = Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
		PID = OperatingSystem_CreateProcess(i);
		switch (PID)
		{
		// V1 Ejercicio 4b
		case NOFREEENTRY:
			// V2 Ejercicio 1
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(103, ERROR, programList[i]->executableName);
			continue;
		// V1 Ejercicio 5b
		case PROGRAMDOESNOTEXIST:
			// V2 Ejercicio 1
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName,
										"it does not exist");
			continue;
		// V1 Ejercicio 5c
		case PROGRAMNOTVALID:
			// V2 Ejercicio 1
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName,
										"invalid priority or size");
			continue;
		// V1 Ejercicio 6
		case TOOBIGPROCESS:
			// V2 Ejercicio 1
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(105, ERROR, programList[i]->executableName);
			continue;
		default:
			numberOfSuccessfullyCreatedProcesses++;
			if (programList[i]->type == USERPROGRAM)
				numberOfNotTerminatedUserProcesses++;
			// Move process to the ready state
			OperatingSystem_MoveToTheREADYState(PID);
			break;
		}
	}

	// V2 Ejercicio 7d
	if (numberOfSuccessfullyCreatedProcesses > 0)
		OperatingSystem_PrintStatus();

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

	// V1 Ejercicio 4a
	if (PID < 0) // NOFREEENTRY == -3
	{
		return PID;
	}

	// Obtain the memory requirements of the program
	processSize = OperatingSystem_ObtainProgramSize(&programFile, executableProgram->executableName);

	// V1 Ejercicio 5a
	if (processSize < 0) // PROGRAMDOESNOTEXIST == -1 ||  PROGRAMNOTVALID == -2
	{
		return processSize;
	}

	// Obtain the priority for the process
	priority = OperatingSystem_ObtainPriority(programFile);

	// V1 Ejercicio 5a
	if (priority < 0) // PROGRAMNOTVALID == -2
	{
		return priority;
	}

	// Obtain enough memory space
	loadingPhysicalAddress = OperatingSystem_ObtainMainMemory(processSize, PID);

	// V1 Ejercicio 6a
	if (loadingPhysicalAddress < 0) // TOOBIGPROCESS == -4
	{
		return loadingPhysicalAddress;
	}

	// Load program in the allocated memory
	int address = OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize);

	// V1 Ejercicio 7a
	if (address < 0) // TOOBIGPROCESS == -4
	{
		return address;
	}

	// PCB initialization
	OperatingSystem_PCBInitialization(PID, loadingPhysicalAddress, processSize, priority, indexOfExecutableProgram);

	// V2 Ejercicio 1
	OperatingSystem_ShowTime(INIT);

	// Show message "Process [PID] created from program [executableName]\n"
	ComputerSystem_DebugMessage(22, INIT, PID, executableProgram->executableName);

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
	// V1 Ejercicio 10
	// Guardo el nombre del programa dentro del proceso para simplicarme la vida
	processTable[PID].name = programList[processPLIndex]->executableName;
	processTable[PID].busy = 1;
	processTable[PID].initialPhysicalAddress = initialPhysicalAddress;
	processTable[PID].processSize = processSize;
	processTable[PID].state = NEW;

	// V2 Ejercicio 1
	OperatingSystem_ShowTime(SYSPROC);

	// V1 Ejercicio 10b
	ComputerSystem_DebugMessage(111, SYSPROC, PID, processTable[PID].name, statesNames[NEW]);
	processTable[PID].priority = priority;
	processTable[PID].programListIndex = processPLIndex;
	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM)
	{
		processTable[PID].copyOfPCRegister = initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister = ((unsigned int)1) << EXECUTION_MODE_BIT;
		processTable[PID].queueID = DAEMONSQUEUE; // V1 Ejercicio 11c
	}
	else
	{
		processTable[PID].copyOfPCRegister = 0;
		processTable[PID].copyOfPSWRegister = 0;
		processTable[PID].queueID = USERPROCESSQUEUE; // V1 Ejercicio 11c
	}

	// V1 Ejercicio13
	processTable[PID].copyOfAccumulator = 0;

	// V2 Ejercicio 5a
	processTable[PID].whenToWakeUp = 0;
}

// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID)
{
	// V1 Ejercicio11c
	int queueID = processTable[PID].queueID;
	if (Heap_add(PID, readyToRunQueue[queueID], QUEUE_PRIORITY,
				 &numberOfReadyToRunProcesses[queueID], PROCESSTABLEMAXSIZE) >= 0)
	{
		// V2 Ejercicio 1
		OperatingSystem_ShowTime(SYSPROC);
		// V1 Ejercicio 10b
		ComputerSystem_DebugMessage(110, SYSPROC, PID, processTable[PID].name,
									statesNames[processTable[PID].state], statesNames[READY]);
		processTable[PID].state = READY;
		// V1 Ejercicio9b
		// OperatingSystem_PrintReadyToRunQueue(); V2 Ejercicio 8
	}
}

// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
int OperatingSystem_ShortTermScheduler()
{

	int selectedProcess;

	// V1 Ejercicio11c
	int queueID;
	if (numberOfReadyToRunProcesses[USERPROCESSQUEUE] > 0)
	{
		queueID = USERPROCESSQUEUE;
	}
	else
	{
		queueID = DAEMONSQUEUE;
	}

	selectedProcess = OperatingSystem_ExtractFromReadyToRun(queueID);

	return selectedProcess;
}

// Return PID of more priority process in the READY queue
// V1 Ejercicio11c
int OperatingSystem_ExtractFromReadyToRun(int queueID)
{

	int selectedProcess = NOPROCESS;

	// V1 Ejercicio11c
	selectedProcess = Heap_poll(readyToRunQueue[queueID], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[queueID]);

	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess;
}

// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID)
{

	// The process identified by PID becomes the current executing process
	executingProcessID = PID;

	// V2 Ejercicio 1
	OperatingSystem_ShowTime(SYSPROC);

	// V1 Ejercicio 10b
	ComputerSystem_DebugMessage(110, SYSPROC, PID, processTable[PID].name,
								statesNames[processTable[PID].state], statesNames[EXECUTING]);

	// Change the process' state
	processTable[PID].state = EXECUTING;
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}

// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID)
{
	// V1 Ejercicio13
	Processor_SetAccumulator(processTable[PID].copyOfAccumulator);

	// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE - 1, processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE - 2, processTable[PID].copyOfPSWRegister);

	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
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
	// V1 Ejercicio13
	processTable[PID].copyOfAccumulator = Processor_GetAccumulator();

	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister = Processor_CopyFromSystemStack(MAINMEMORYSIZE - 1);

	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister = Processor_CopyFromSystemStack(MAINMEMORYSIZE - 2);
}

// Exception management routine
void OperatingSystem_HandleException()
{

	// V2 Ejercicio 1
	OperatingSystem_ShowTime(SYSPROC);

	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	ComputerSystem_DebugMessage(23, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);

	OperatingSystem_TerminateProcess();

	// V2 Ejercicio 7c
	OperatingSystem_PrintStatus();
}

// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess()
{

	int selectedProcess;

	// V2 Ejercicio 1
	OperatingSystem_ShowTime(SYSPROC);

	// V1 Ejercicio 10b
	ComputerSystem_DebugMessage(110, SYSPROC, executingProcessID,
								processTable[executingProcessID].name,
								statesNames[processTable[executingProcessID].state], statesNames[EXIT]);

	processTable[executingProcessID].state = EXIT;

	// V3
	// processTable[executingProcessID].busy = 0;

	if (programList[processTable[executingProcessID].programListIndex]->type == USERPROGRAM)
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;

	// V3 Ejercicio 04c
	if (numberOfNotTerminatedUserProcesses == 0 && numberOfProgramsInArrivalTimeQueue == 0)
	{
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

	// V1 Ejercicio 12
	int queueExecuting = processTable[executingProcessID].queueID;
	int firstPid;

	switch (systemCallID)
	{
	case SYSCALL_PRINTEXECPID:
		// V2 Ejercicio 1
		OperatingSystem_ShowTime(SYSPROC);

		// Show message: "Process [executingProcessID] has the processor assigned\n"
		ComputerSystem_DebugMessage(24, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
		break;

	case SYSCALL_END:
		// V2 Ejercicio 1
		OperatingSystem_ShowTime(SYSPROC);

		// Show message: "Process [executingProcessID] has requested to terminate\n"
		ComputerSystem_DebugMessage(25, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
		OperatingSystem_TerminateProcess();
		// V2 Ejercicio 7b
		OperatingSystem_PrintStatus();
		break;
	// V1 Ejercicio 12
	case SYSCALL_YIELD:
		// Compruebo que hay más procesos en la cola
		if (numberOfReadyToRunProcesses[queueExecuting] > 0)
		{
			// Selecciona el primero de la cola, él más prioritatio
			firstPid = Heap_getFirst(readyToRunQueue[queueExecuting], numberOfReadyToRunProcesses[queueExecuting]);
			// Compruebo que tiene la misma prioridad que el proceso que esta ejecutando
			if (processTable[executingProcessID].priority == processTable[firstPid].priority)
			{
				// V2 Ejercicio 1
				OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
				// Imprimo mensaje
				ComputerSystem_DebugMessage(115, SHORTTERMSCHEDULE, executingProcessID,
											processTable[executingProcessID].name,
											firstPid, processTable[firstPid].name);
				// Saco el proceso que se ejecutando en el sistema operativo
				OperatingSystem_PreemptRunningProcess();
				// Preparo el primer proceso de la cola
				firstPid = OperatingSystem_ExtractFromReadyToRun(queueExecuting);
				// Empiezo la ejecucion del nuevo proceso
				OperatingSystem_Dispatch(firstPid);
				// V2 Ejercicio 7a
				OperatingSystem_PrintStatus();
			}
		}
		break;
	// v2 Ejercicio 5d
	case SYSCALL_SLEEP:
		// Guardas el estado del proceso
		OperatingSystem_SaveContext(executingProcessID);
		// Cambias al estado Bloquedo y lo mueves a la cola de processos dormidos
		OperatingSystem_MoveTotheBlockedState(executingProcessID);
		// Deasignas el proceso actual
		executingProcessID = NOPROCESS;
		// Lanzas otro proceso
		OperatingSystem_Dispatch(OperatingSystem_ShortTermScheduler());
		// V2 Ejercicio 5e
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
	// V2 Ejercicio 2c
	case CLOCKINT_BIT: // CLOCKINT_BIT = 9
		OperatingSystem_HandleClockInterrupt();
		break;
	}
}

// V1 Ejercicio 11b
void OperatingSystem_PrintReadyToRunQueue()
{
	// V2 Ejercicio 1
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE);
	int i;

	for (i = 0; i < NUMBEROFQUEUES; i++)
	{
		OperatingSystem_PrintQueue(i);
	}
}

// V1 Ejercicio 11b
void OperatingSystem_PrintQueue(int queue)
{
	int i, PID, priority;
	int size = numberOfReadyToRunProcesses[queue];
	if (size == 0)
	{
		ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE, queueNames[queue], "\n");
	}
	else
	{
		ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE, queueNames[queue], "");
		for (i = 0; i < size; i++)
		{
			// Cogemos su PID del array que contiene los procesos
			// que están listos para entrar en el procesador
			PID = readyToRunQueue[queue][i];
			// Coge la prioridad la prioridad del proceso de la procesos
			// global del sistema operativo
			priority = processTable[PID].priority;
			if (i == 0)
			{
				if (size == 1)
				{
					ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, PID,
												priority, "\n");
				}
				else
				{
					ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, PID,
												priority, ", ");
				}
			}
			else if (i == size - 1)
			{
				ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, PID,
											priority, "\n");
			}
			else
			{
				ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, PID,
											priority, ", ");
			}
		}
	}
}

// In OperatingSystem.c Exercise 2-b of V2
void OperatingSystem_HandleClockInterrupt()
{
	// V2 Ejercicio 4
	numberOfClockInterrupts++;
	OperatingSystem_ShowTime(INTERRUPT);
	ComputerSystem_DebugMessage(120, INTERRUPT, numberOfClockInterrupts);

	// V2 Ejercicio06a
	int numWakeUpProcess = 0;
	// Mientras haya procesos bloqueados y
	// El primer elemento de cola de prioridad de procesos bloqueados tiene el mismo
	// ciclo de reloj para despertar que el ciclo de reloj actual
	while (numberOfSleepingProcesses > 0 && processTable[sleepingProcessesQueue[0]].whenToWakeUp == numberOfClockInterrupts)
	{
		int selectedProcess = OperatingSystem_ExtractFromBlockedToReady();
		OperatingSystem_MoveToTheREADYState(selectedProcess);
		numWakeUpProcess++;
	}

	// V3 Ejercicio 03a
	int newProcess = OperatingSystem_LongTermScheduler();

	// V3 Ejercicio04b
	if (numberOfNotTerminatedUserProcesses == 0 && numberOfProgramsInArrivalTimeQueue == 0)
	{
		OperatingSystem_ReadyToShutdown();
	}

	// V2 Ejercicio06b
	// V3 Ejercicio 03b
	if (numWakeUpProcess > 0 || newProcess > 0)
	{
		if (numWakeUpProcess)
		{
			OperatingSystem_PrintStatus();
		}
		// V2 Ejercicio06c
		// Compruebo si es un demonio del sistema
		if (processTable[executingProcessID].queueID == DAEMONSQUEUE)
		{
			// Compruebo si no hay procesos de usuario
			if (numberOfReadyToRunProcesses[USERPROCESSQUEUE] > 0)
				OperatingSystem_ChangeProcess(USERPROCESSQUEUE);
			// Compruebo si hay más demonios y su prioridad
			else if (numberOfReadyToRunProcesses[DAEMONSQUEUE] > 0 && processTable[readyToRunQueue[DAEMONSQUEUE][0]].priority < processTable[executingProcessID].priority)
				OperatingSystem_ChangeProcess(DAEMONSQUEUE);
		} // Compruebo si es un proceso de usuario
		else if (processTable[executingProcessID].queueID == USERPROCESSQUEUE)
		{
			// Compruebo si hay más proceso de usuario y su prioridad
			if (numberOfReadyToRunProcesses[USERPROCESSQUEUE] > 0 && processTable[readyToRunQueue[USERPROCESSQUEUE][0]].priority < processTable[executingProcessID].priority)
				OperatingSystem_ChangeProcess(USERPROCESSQUEUE);
		}
	}
	return;
}

// V2 Ejercicio 5d
void OperatingSystem_MoveTotheBlockedState(int PID)
{
	// se obtendrá sumando el valor absoluto del valor
	// actual del registro acumulador al número de interrupciones
	// de reloj que se hayan producido hasta el momento, más una unidad adicional
	int whenToWakeUp = abs(Processor_GetAccumulator()) + numberOfClockInterrupts + 1;
	processTable[PID].whenToWakeUp = whenToWakeUp;
	if (Heap_add(PID, sleepingProcessesQueue, QUEUE_WAKEUP,
				 &numberOfSleepingProcesses, PROCESSTABLEMAXSIZE) >= 0)
	{
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(
			110, SYSPROC, executingProcessID,
			processTable[executingProcessID].name,
			statesNames[processTable[executingProcessID].state],
			statesNames[BLOCKED]);
		processTable[PID].state = BLOCKED;
	}
}

// V2 Ejercicio 6d
int OperatingSystem_ExtractFromBlockedToReady()
{
	return Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP,
					 &numberOfSleepingProcesses);
}

// V2 Ejercicio06
void OperatingSystem_ChangeProcess(int queue)
{
	// Hago una copia del proceso que se ejecutando
	int copyOfExecutingProcessID = executingProcessID;
	// Selecciono el siguiente proceso que se va ejecutar
	int selectedProcess = readyToRunQueue[queue][0];
	// Imprimo el mensaje
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(
		121, SHORTTERMSCHEDULE,
		copyOfExecutingProcessID, processTable[copyOfExecutingProcessID].name,
		selectedProcess, processTable[selectedProcess].name);
	// Saco el proceso que se ejecutando
	OperatingSystem_PreemptRunningProcess();
	// Empiezo a ejecutar el nuevo proceso
	OperatingSystem_Dispatch(selectedProcess);
	OperatingSystem_PrintStatus();
}

// V3 Ejercicio 1
int OperatingSystem_GetExecutingProcessID()
{
	return executingProcessID;
}
