/**
 * @file        task.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        23 September, 2018
 * @brief       Task Manager implementation
*/

/* Includes ----------------------------------------------- */

#include <task.h>
#include <kheap.h>
#include <string.h>
#include <memmgr.h>

#include <kvspace.h>

#include <scheduler.h>
#include <ipc_2.h>
#include <isr.h>
#include <sleep.h>
#include <mutex.h>

#include <arch.h>


/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */


/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */

/* @brief	Routine to initialize the task local storage
 *
 * @param	sp_kptr  - Stack pointer (using the kernel virtual space)
 * 			id - task id (tid)
 *
 * @retval	Return offset to apply to the stack pointer
 */
int32_t TaskTlsInit(vaddr_t sp_kptr, uint32_t id);

/* @brief	Routine to set the main task stack, copy argv and adjust stack pointer.
 *
 * @param	sp_kptr  - Stack pointer (using the kernel virtual space)
 * 			sp_Size  - Stack size
 * 			sp_vTop  - Stack Top user virtual address
 * 			argv     - Command used to start the process
 * 			registers- Pointer to the task tcb
 *
 * @retval	New top stack address (user virtual address)
 */
vaddr_t TaskMainStackInit(paddr_t sp_kptr, size_t sp_Size, vaddr_t sp_vTop, const char *argv, void *registers);


/* Private functions -------------------------------------- */

/**
 * TaskInit Implementation (See header file for description)
*/
int32_t TaskInit(task_t *task, taskAttr_t *attr, taskParam_t *param)
{
	// Set task info
	task->state = READY;
	task->active_prio = task->real_prio = attr->priority;
	if(attr->detached) task->flags |= TASK_DETACHED;

	// We do not set any privilege level at task creation because all tasks are
	// created without privileges enabled.
	// Privileges can later be enable for the task if process level privileges
	// allow it.
//	task->info.privilege = PRIV_NONE;
	task->flags |= TASK_PRIV_NONE;

	// No CPU affinity is used by default.
	// Can be set later using the SetAffinity system call
//	task->info.cpuLocked = FALSE;

	// Save task priority in blocked structured
//	task->blocked.priority = task->info.priority;

	// Set task memory
	task->memory.spMaxSize = attr->stackSize;
	task->memory.entry = param->entry;
	task->memory.exit = param->exit;
	task->memory.registers = _TaskAllocTCB();
	_TaskSetParameters(task->memory.registers, param->arg, NULL, NULL);
	_TaskSetEntry(task->memory.registers, task->memory.entry);
	_TaskSetExit(task->memory.registers, task->memory.exit);

	// Initialize Joined tasks list
	(void)GlistInitialize(&task->joined, GFifo);
	(void)GlistInitialize(&task->owned_mutexs, GList);
	GlistSetSort(&task->owned_mutexs, MutexListSort);

	// Set interrupt info to invalid
	task->interrupt.id  = INTERRUPT_INVALID;
	task->interrupt.irq = INTERRUPT_INVALID;

	return E_OK;
}

/**
 * TaskCreate Implementation (See header file for description)
*/
task_t *TaskCreateIdle()
{
	task_t *task = (task_t*)kmalloc(sizeof(task_t));

	if(NULL == task)
	{
		return NULL;
	}

	memset(task, 0x0, sizeof(task_t));

	// Set task info
	task->tid = 0;
	task->parent = NULL;
	task->state = READY;
	task->real_prio = task->active_prio = 0;
	task->flags |= (TASK_DETACHED | TASK_PRIV_IO);

	// No CPU affinity is used by default.
//	task->info.cpuLocked = FALSE;

	// Set task memory
	task->memory.spMaxSize = (16 * 4);
	task->memory.entry = _IdleTask;
	task->memory.exit = NULL;
	task->memory.registers = _TaskAllocTCB();
	_TaskSetParameters(task->memory.registers, NULL, NULL, NULL);
	_TaskSetEntry(task->memory.registers, task->memory.entry);
	_TaskSetExit(task->memory.registers, task->memory.exit);

	// Set Idle Task stack
	task->memory.sp = kmalloc(task->memory.spMaxSize);
	_TaskSetSP(task->memory.registers, task->memory.sp);
	_TaskSetPriligeMode(task->memory.registers);

	// Set interrupt info to invalid
	task->interrupt.id  = INTERRUPT_INVALID;
	task->interrupt.irq = INTERRUPT_INVALID;

	return task;
}

/**
 * TaskSetId Implementation (See header file for description)
*/
void TaskSetId(task_t *task, uint32_t id)
{
	task->tid = ((task->parent->pid << 16) | id);
}

/**
 * TaskSetStack Implementation (See header file for description)
*/
int32_t TaskSetStack(sManager_t *stackManager, size_t size, task_t *task, const char *argv)
{
	// Allocate a stack frame for the task
	task->memory.stack = vStackGet(stackManager, task->memory.spMaxSize);
	// Get memory to initialize the stack
	paddr_t spPaddr = (paddr_t)MemoryGet(size, ZONE_INDIRECT);
	// Map stack memory in the process virtual space
	vaddr_t sp = task->memory.sp = vStackMap(task->memory.stack, spPaddr, size, PAGE_USER_DATA);
	task->memory.spSize = size;

	// Map stack memory in kernel virtual space
	char *sp_kbase = (char *)VirtualSpaceMmap(spPaddr, size);

	// Get pointer to stack top (on the kernel virtual space)
	char *sp_kptr = (sp_kbase + size);

	// Initialize TLS
	int32_t offset = TaskTlsInit(sp_kptr, task->tid);
	sp_kptr -= offset;
	sp = ((char*)(sp) - offset);
	task->memory.tls = (tls_t *)sp;

	// If argv is defined it means that this is the main task
	if(argv != NULL)
	{
		sp = TaskMainStackInit(sp_kptr, size, sp, argv, task->memory.registers);
	}

	// Unmap stack memory from kernel virtual space
	VirtualSpaceUnmmap(sp_kbase);

	// Set stack pointer
	_TaskSetSP(task->memory.registers, sp);

	return E_OK;
}

/**
 * TaskClean Implementation (See header file for description)
*/
void TaskClean(task_t *task)
{
	// Free task stack (memory + virtual space)
	vStackFree(task->memory.stack);

	// Free task TCB
	_TaskDeallocTCB(task->memory.registers);
}

/**
 * TaskTerminate Implementation (See header file for description)
*/
void TaskTerminate(task_t *task, void *ret, bool_t killed)
{
	// Remove task from pending list
	if(task->state == BLOCKED)
	{
		// Resolve IPC
		if(task->subState == IPC_SEND)
		{
			IpcSendCancel(task);
		}
		else if(task->subState == IPC_REPLY)
		{
			IpcReplyCancel(task);
		}
		else if(task->subState == IPC_RECEIVE)
		{
			IpcReceiveCancel(task);
		}
		else if((task->subState == SLEEPING) || (task->timeout.set == TRUE))
		{
			SleepRemove(task);
		}
		// In case we missed the actual pending list
		GlistRemoveSpecific(&task->node);
		// Set task state to DEAD
		task->state = DEAD;
	}

	// If killed is TRUE we are in a dying process so we can exit here
	if(killed == TRUE)
	{
		return;
	}

	// For now we will use the same behavior that is defined in posix
	// If a task is cancelled holding a mutex the mutex will not be free

	// Clean any interrupt attached to this task
	InterruptClean(task);

	// Resolve joined tasks
	while(GLIST_EMPTY(&task->joined) == FALSE)
	{
		task_t *joined = GLISTNODE2TYPE(GlistRemoveObject(&task->joined, NULL), task_t, node);
		joined->data.join.value_ptr = ret;
		SchedAddTask(joined);
	}
}

/**
 * TaskTlsInit Implementation (See Private function prototypes for description)
*/
int32_t TaskTlsInit(vaddr_t sp_kptr, uint32_t id)
{
	tls_t *tls = (tls_t*)((char*)(sp_kptr) - sizeof(tls_t));
	tls->id = id;
	tls->flags = 0;
	tls->errno = 0;
	tls->keysSize = 0;
	tls->keys = NULL;
	tls->cleanup = NULL;

	return sizeof(tls_t);
}

/**
 * TaskMainStackInit Implementation (See Private function prototypes for description)
*/
vaddr_t TaskMainStackInit(vaddr_t sp_kptr, size_t sp_Size, vaddr_t sp_vTop, const char *argv, void *registers)
{
	// Get argv string size
	int32_t len = strlen(argv);
	// Get pointer to stack top, we will copy the argv from top to bottom
	char *str = (sp_kptr - 1);
	// Get address to write the pointer to the strings
	char **ptrs = (char**)ALIGN_DOWN((uint32_t)(sp_kptr - ((len + 1) + sizeof (char **))), 0x4);

	// Perform copy to stack
	int32_t argc = 1;
	uint32_t offset = 0;

	while(len >= 0)
	{
		if(argv[len] == ' ')
		{
			*str = '\0';
			*ptrs = (char*)sp_vTop - offset;
			ptrs--;
			argc++;
		}
		else
		{
			*str = argv[len];
		}

		str--;
		len--;
		offset++;
	}
	*ptrs = (char*)sp_vTop - offset;

	vaddr_t sp_base = (vaddr_t)((char*)sp_vTop - ((uint32_t)(sp_kptr) - (uint32_t)(ptrs)));

	_TaskSetParameters(registers, (void*)argc, (void*)sp_base, NULL);

	return sp_base;
}

/**
 * TaskExpandStack Implementation (See Private function prototypes for description)
*/
int32_t TaskExpandStack(task_t *task, uint32_t size)
{
	size = ROUND_UP(size, PAGE_SIZE);

	if(task->memory.spSize + size > task->memory.spMaxSize)
	{
		return E_ERROR;
	}

	// Get memory to expand stack
	paddr_t spPaddr = (paddr_t)MemoryGet(size, ZONE_INDIRECT);

	if(spPaddr == NULL)
	{
		return E_NO_MEMORY;
	}

	if(vStackMap(task->memory.stack, spPaddr, size, PAGE_USER_DATA) == NULL)
	{
		MemoryFree(spPaddr, size);
		return E_ERROR;
	}

	return E_OK;
}
