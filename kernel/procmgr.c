/**
 * @file        procmgr.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        26 August, 2018
 * @brief       Process Manager implementation
*/

/* Includes ----------------------------------------------- */
#include <procmgr.h>
#include <process.h>
#include <vector.h>

#include <kheap.h>
#include <string.h>
#include <arch.h>
#include <board.h>

#include <loader.h>
#include <task.h>

#include <scheduler.h>

/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */



/* Private macros ----------------------------------------- */


/* Private variables -------------------------------------- */

static struct
{
	uint32_t	runningProcs;
	vector_t	processes;
	struct
	{
		size_t		virtualSpaceSize;
		size_t		taskStackSize;
		size_t		heapSize;
		uint32_t	maxTasks;
	}parameters;
}ProcMgr;

/* Private function prototypes ---------------------------- */

/*
 * @brief   Routine to compute the maximum tasks a process can have
 *
 * @param   vsSize - Virtual space size
 * 			execMaxAddr - Top address of the process sections
 * 			heapSize - Max heap size
 * 			taskStackSize - tasks stack size
 *
 * @retval  Maximum tasks allowed
 */
inline static uint32_t GetMaxTasks(size_t vsSize, vaddr_t execMaxAddr, size_t heapSize, size_t taskStackSize)
{
	return ((vsSize - (ALIGN_UP(((uint32_t)execMaxAddr), 0x00100000) + heapSize)) / taskStackSize);
}

/*
 * @brief   System Call to create a new task in the running process
 *
 * @param   tid - variable to return the task identifier
 * 			taskAttr - task creation attributes
 * 			arg - argument to pass to the task routine
 * 			start_routine - task routine entry point
 *
 * @retval  Returns Success
 */
//int32_t ProcTaskCreate(uint32_t *tid, taskAttr_t *attr, void *(*start_routine) (void *), void *arg);
int32_t ProcTaskCreate(uint32_t* tid, taskAttr_t* attr, void* (*start_routine)(void*), void* (*exit_routine)(void*), void* arg);

/*
 * @brief   Task join system call
 *
 * @param   tid - task that the running task wants to join
 * 			value_ptr - variable to pass the return of the joined task
 *
 * @retval  Returns Success
 */
int32_t ProcTaskJoin(uint32_t tid, void **value_ptr);

/*
 * @brief   Task exit system call
 *
 * @param   ret - return value
 *
 * @retval  No return
 */
void ProcTaskExit(void *ret);

/*
 * @brief   Routine to clean the process allocated memory.
 * 			May have to be called using a kernel stack
 *
 * @param   process - process being terminated
 *
 * @retval  No return
 */
void ProcProcessSafeClean(process_t *process);

/*
 * @brief   Routine to terminate the whole process execution
 *
 * @param   process - process being terminated
 *
 * @retval  No return
 */
void ProcProcessTerminate(process_t *process);

/*
 * @brief   System call to terminate a specified process
 *
 * @param   pid - pid of the process to be terminated
 *
 * @retval  No return
 */
void ProcProcessKill(pid_t pid);

void ProcTaskClean(process_t *process, task_t *task)
{
	// Clean task allocated memory
	TaskClean(task);

	// Free task handler structure
	AllocatorFree(&process->tasksPool, task);
}

/* Private functions -------------------------------------- */

uint32_t ProcProcessesRunning()
{
	return ProcMgr.runningProcs;
}

/**
 * ProcManagerInit Implementation (See header file for description)
*/
int32_t ProcManagerInit(void)
{
	ProcMgr.runningProcs = 0;
	VectorInit(&ProcMgr.processes, 0);
	// Reserve index zero
	VectorInsertAt(&ProcMgr.processes, NULL, 0);

	//TODO: ProcMgr: get virtual space size from arch
	ProcMgr.parameters.virtualSpaceSize = 0x80000000;
	ProcMgr.parameters.taskStackSize = 0x00800000;
	ProcMgr.parameters.heapSize = 0x20000000;
	ProcMgr.parameters.maxTasks = PARAM_UNDEFINED;

	int32_t i = BoardGetCpus();

	for(; i > 0; --i)
	{
		//Create Idle Task
		task_t *task = TaskCreateIdle();
		SchedAddTask(task);
	}

	return E_OK;
}


/**
 * ProcSpawn Implementation (See header file for description)
*/
pid_t ProcSpawn(void *elf, char *cmd, spawn_attr_t* sattr, uint32_t fd_count, int32_t* fd_map)
{
	process_t *parent = SchedGetRunningProcess();

	process_t *proc = (process_t*)kmalloc(sizeof(process_t));
	memset(proc, 0x0, sizeof(process_t));

	if(LoaderLoadElf(elf, cmd, &proc->exec) != E_OK)
	{
		kfree(proc, sizeof(*proc));
		return (pid_t)-1;
	}

	proc->pid = VectorInsert(&ProcMgr.processes, proc);

	procAttr_t attr;
	if(sattr != NULL)
	{
		attr.detached = sattr->detached;
		attr.priority = sattr->priority;
		attr.privilege = sattr->privilege;
		attr.heritage = sattr->heritage;
	}
	else
	{
		attr.detached = FALSE;
		attr.priority = 10;
		attr.privilege = 1;
		attr.heritage = TRUE;
	}
	attr.stacksSize = ProcMgr.parameters.taskStackSize;
	attr.heapSize = ProcMgr.parameters.heapSize;
	attr.virtualSpaceSize = ProcMgr.parameters.virtualSpaceSize;
	attr.maxTasks = GetMaxTasks(attr.virtualSpaceSize, proc->exec.load->topAddr, attr.heapSize, attr.stacksSize);

	ProcessInit(proc, &attr, cmd);

	if(parent != NULL)
	{
		if(attr.heritage == TRUE)
		{
			ProcessCopyConnectionsRange(parent, proc, fd_count, fd_map);
		}

		if( attr.detached != TRUE)
		{
			ProcessAddChild(parent, proc);
		}
	}

	ProcMgr.runningProcs++;

	pid_t pid = proc->pid;

	ProcessStart(proc);

	return pid;
}

process_t *ProcGetProcess(pid_t pid)
{
	return (process_t *)VectorPeek(&ProcMgr.processes, (uint32_t)pid);
}

/**
 * ProcProcessSafeClean Implementation (See Private function prototypes file for description)
*/
void ProcProcessSafeClean(process_t *process)
{
	// Clean Process Memory
	ProcessMemoryClean(process);

	// Only now it is safe to release the process pid
	(void)VectorRemove(&ProcMgr.processes, process->pid);
	// Update number of running processes
	ProcMgr.runningProcs--;

	// Free process handler
	kfree(process, sizeof(*process));

	// Invalidate ASID --> REMOVE FROM HEHRE!!!!
	asm volatile ("mcr p15, 0, %0,c8, c3, 2" : : "r" (process->pid));
}

/**
 * ProcProcessTerminate Implementation (See Private function prototypes file for description)
*/
void ProcProcessTerminate(process_t *process)
{
	// Remove from parent
	GlistRemoveSpecific(&process->siblings);

	// Remove from whatever may be tracking the process
	GlistRemoveSpecific(&process->node);

	// Terminate Process
	ProcessTerminate(process);

	// Make child processes orphan or kill them
	while(GLIST_EMPTY(&process->childprocs) == FALSE)
	{
		process_t *child = GLISTNODE2TYPE(GlistRemoveObject(&process->childprocs, NULL), process_t, siblings);
		child->parent = NULL;
		// TODO: Should we kill the children? :-)
	}

	// Unload Elf
	LoaderUnloadElf(&process->exec);

	_TerminateRunningProcess();

	// Will never get here!
}

/**
 * ProcProcessKill Implementation (See Private function prototypes file for description)
*/
void ProcProcessKill(pid_t pid)
{
	process_t *process = (process_t*)VectorPeek(&ProcMgr.processes, pid);

	if(process == NULL || process == SchedGetRunningProcess())
	{
		return;
	}

	// Remove from parent
	GlistRemoveSpecific(&process->siblings);

	// Remove from whatever may be tracking the process
	GlistRemoveSpecific(&process->node);

	// Terminate Process
	ProcessTerminate(process);

	// With the process terminated it is safe to remove the process
	(void)VectorRemove(&ProcMgr.processes, pid);

	// Update number of running processes
	ProcMgr.runningProcs--;

	// Make child processes orphan or kill them
	while(GLIST_EMPTY(&process->childprocs) == FALSE)
	{
		process_t *child = GLISTNODE2TYPE(GlistRemoveObject(&process->childprocs, NULL), process_t, siblings);
		child->parent = NULL;
		// TODO: Should we kill the children? :-)
	}

	// Unload Elf
	LoaderUnloadElf(&process->exec);

	// Clean Process Memory
	ProcessMemoryClean(process);

	// Free process handler
	kfree(process, sizeof(*process));
}

int32_t ProcWaitPid(pid_t pid)
{
	task_t *task = SchedGetRunningTask();
	process_t *process = task->parent;

	// Is pid a child from process?
	process_t *child = (process_t*)VectorPeek(&ProcMgr.processes, pid);

	if((child == NULL) || (child->parent != process))
	{
		return E_INVAL;
	}

	// TODO: Add to pending list (will be changed)
	GlistInsertObject(&child->pendingTasks, &task->node);

	return SchedStopRunningTask(BLOCKED, SIGNAL_PENDING);

//	return (int32_t)task->blocked.data;
}

/**
 * ProcTaskCreate Implementation (See Private function prototypes file for description)
*/
int32_t ProcTaskCreate(uint32_t* tid, taskAttr_t* attr, void* (*start_routine)(void*), void* (*exit_routine)(void*), void* arg)
{
	if(tid == NULL || start_routine == NULL)
	{
		return E_INVAL;
	}

	task_t* task = NULL;

	if(attr == NULL)
	{
		taskAttr_t attr2;
		attr2.detached = FALSE;
		attr2.priority = SchedGetRunningTask()->real_prio;
		attr2.stackSize = SchedGetRunningTask()->memory.spMaxSize;

		task = ProcessTaskCreate(SchedGetRunningProcess(), &attr2, arg, start_routine, exit_routine, FALSE);
	}
	else
	{
		task = ProcessTaskCreate(SchedGetRunningProcess(), attr, arg, start_routine, exit_routine, FALSE);
	}

	if(task == NULL)
	{
		return E_ERROR;
	}

	*tid = task->tid;

	SchedAddTask(task);

	return E_OK;
}

/**
 * ProcTaskJoin Implementation (See Private function prototypes file for description)
*/
int32_t ProcTaskJoin(uint32_t tid, void** value_ptr)
{
	process_t* process = SchedGetRunningProcess();

	task_t* joined = AllocatorToAddr(&process->tasksPool, (tid & 0xFFFF));

	if(joined == NULL || joined->tid != tid)
	{
		return E_INVAL;
	}

	task_t* joining = SchedGetRunningTask();

	GlistInsertObject(&joined->joined, &joining->node);

	SchedStopRunningTask(BLOCKED, JOINED);

	if(value_ptr)
	{
		*value_ptr = joining->data.join.value_ptr;
	}

	return E_OK;
}

/**
 * ProcTaskExit Implementation (See Private function prototypes file for description)
*/
void ProcTaskExit(void *ret)
{
	task_t *task = SchedGetRunningTask();

	// TODO: Should we disable preemption for the task termination?

	if(ProcessIsMainTask(task->tid))
	{
		ProcProcessTerminate(task->parent);

		// Will never get here!
	}

	GlistRemoveSpecific(&task->siblings);

	TaskTerminate(task, ret, FALSE);

	_TerminateRunningTask();

	// Will never get here!
}

/**
 * ProcTaskCancel Implementation (See Private function prototypes file for description)
*/
int32_t ProcTaskCancel(uint32_t tid)
{
	process_t* process = SchedGetRunningProcess();

	task_t* task = AllocatorToAddr(&process->tasksPool, (tid & 0xFFFF));

	if(task == NULL)
	{
		return E_SRCH;
	}

	if(task == SchedGetRunningTask())
	{
		return E_INVAL;
	}

	task->memory.tls->flags |= TASK_CANCEL_PENDING;

	if((task->memory.tls->flags & TASK_CANCEL_TYPE_MASK) == TASK_CANCEL_DISABLE)
	{
		return E_OK;
	}

	//SchedulerDisablePreemption();

	// If task is currently blocked
	// TODO: This does not cover all cases maybe add a cancelable flag

	if(task->subState == JOINED)
	{
		// Set task to ready to be resumed for cancellation
		GlistRemoveSpecific(&task->node);

		// Set resume point to exit handler
		_TaskSetParameters(task->memory.registers, TASK_CANCELED_RET, 0x0, 0x0);
		_TaskSetEntry(task->memory.registers, task->memory.exit);
		_TaskSetUserMode(task->memory.registers);

		SchedAddTask(task);
	}

	if(task->state == READY)
	{
		// TODO: In cases like when the task is resumed from IPC SEND there will memory being lost!!!!
		// Set resume point to exit handler
//		_TaskSetParameters(task->memory.registers, TASK_CANCELED_RET, 0x0, 0x0);
//		_TaskSetEntry(task->memory.registers, task->memory.exit);
//		_TaskSetUserMode(task->memory.registers);
	}

	//SchedulerEnablePreemption();

	return E_OK;
}
