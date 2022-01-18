/**
 * @file        scheduler.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        10 April, 2020
 * @brief       Scheduler Manager implementation
*/

/* Includes ----------------------------------------------- */
#include <scheduler.h>
#include <process.h>
#include <memmgr.h>
#include <arch.h>
#include <spinlock.h>
#include <atomic.h>
#include <kheap.h>
#include <interrupt.h>
#include <isr.h>
#include <board.h>
#include <klock.h>

#include <sleep.h>
#include <systimer.h>

/* Private types ------------------------------------------ */
typedef struct
{
    uint16_t   id;
    uint16_t   prio;
    void*      sp;
    uint32_t   irqlevel;
    uint32_t   tslice;
    task_t*    task;
    process_t* process;
}cpu_t;

typedef struct
{
    klock_t    lock;
    uint32_t   flags;
    uint16_t   cpus;
    uint16_t   lprio;
    uint32_t   tslice;
    cpu_t*     lcpu;
    glist_t    tasks;
}sched_t;


void** kernekStacks = NULL;

/* Private constants -------------------------------------- */
#define KERNEL_STACK_SIZE		4096


/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */

// cpu handler structures (one for each core)
static cpu_t* CPUS;
// Scheduler handler
static sched_t sched;


/* Private function prototypes ---------------------------- */

int32_t ReadyListSort(glistNode_t *current, glistNode_t *newtask)
{
	task_t *currentTask = GLISTNODE2TYPE(current, task_t, node);
	task_t *task = GLISTNODE2TYPE(newtask, task_t, node);

	// Task being inserted has less priority then other ready tasks already waiting
	uint16_t prio = ((task->active_prio > 1) ? (task->active_prio - 1) : (task->active_prio));

    return currentTask->active_prio - prio;
}

int32_t ReadyListCmp(glistNode_t *current, void *matchId)
{
	task_t *currentTask = GLISTNODE2TYPE(current, task_t, node);
	uint32_t tid = (uint32_t)matchId;

	return (currentTask->tid != tid);
}

void SchedListInit()
{
	(void)GlistInitialize(&sched.tasks, GList);
	(void)GlistSetSort(&sched.tasks, ReadyListSort);
	(void)GlistSetCmp(&sched.tasks, ReadyListCmp);
}

void SchedSortLowPriority()
{
	uint32_t cpu;

	if(sched.lcpu->id == RUNNING_CPU)
	{
		sched.lprio = 0xFFFF;
	}

	for(cpu = 0; cpu < sched.cpus; ++cpu)
	{
		if(sched.lprio > CPUS[cpu].prio)
		{
			sched.lprio = CPUS[cpu].prio;
			sched.lcpu = &CPUS[cpu];
		}
	}
}

uint16_t SchedPendingPrio()
{
    task_t* task = GLIST_FIRST(&sched.tasks, task_t, node);

    return ((task != NULL) ? (task->active_prio) : (0));
}

task_t* SchedGetNext2Run()
{
    return GLISTNODE2TYPE(GlistRemoveFirst(&sched.tasks), task_t, node);
}

void SchedHoldTask(task_t* task)
{
	if(task->state == DEAD) return;

	task->state = READY;
	task->subState = NONE;
	GlistInsertObject(&sched.tasks, &task->node);
}

void SchedTrigger(uint32_t cpu)
{
	if(cpu == CPUS[RUNNING_CPU].id)
	{
		InterruptGenerateSelf(SCHEDULER_IRQ);
	}
	else
	{
		InterruptGenerate(SCHEDULER_IRQ,cpu);
	}
}

void SchedEnsureLock(uint32_t* status)
{
	KlockEnsure(&sched.lock, status);
}

/* Private functions -------------------------------------- */

/**
 * SchedulerInit Implementation (See header file for description)
*/
int32_t SchedulerInit(uint32_t schedHz)
{
    sched.cpus = BoardGetCpus();
    CPUS = (cpu_t*)kmalloc(sizeof(cpu_t) * sched.cpus);
    kernekStacks = (void**)kmalloc(sizeof(void*) * sched.cpus);

    uint32_t i = 0;
    for(; i < sched.cpus; i++)
    {
        CPUS[i].id = i;
        CPUS[i].prio = 0xFFFF;
        kernekStacks[i] = CPUS[i].sp = ((i == 0) ? (_BoardGetBaseStack()) : (void*)((uint32_t)MemoryGet(KERNEL_STACK_SIZE, ZONE_DIRECT) + KERNEL_STACK_SIZE));
        CPUS[i].irqlevel = 0;
        CPUS[i].tslice = 0;
        CPUS[i].task = NULL;
        CPUS[i].process = NULL;
    }

    sched.lprio = 0xFFFF;
    sched.tslice = ((100 * schedHz) / 1000);
    sched.lcpu = &CPUS[0];

    SchedListInit();

    KlockInit(&sched.lock);
    SchedLock(NULL);

    // At this point it should be safe to resume the other cores since they will block when trying to get a task
    cpus_set_stacks(kernekStacks);
    cpu_boot_finish();

    return E_OK;
}

void SchedulerStart()
{
	// Temporary for debug
	void* abt_stack = (void*)((uint32_t)MemoryGet(KERNEL_STACK_SIZE, ZONE_DIRECT) + KERNEL_STACK_SIZE);

	asm volatile(
			"cps #0x17     \n\t"
			"mov sp, %[sp] \n\t"
			"cps #0x1B     \n\t"
			"mov sp, %[sp] \n\t"
			"cps #0x13     \n\t"
			: : [sp] "r" (abt_stack));

    SchedEnsureLock(NULL);

    // Install Scheduler interrupt
    InterruptAttach(SCHEDULER_IRQ, 10, Schedule, NULL);

    cpu_t* cpu = &CPUS[RUNNING_CPU];

    if(cpu->id == 0)
    {
    	SystemTickStart(sched.tslice, SystemTick);
    }

    cpu->task = SchedGetNext2Run();
    cpu->prio = cpu->task->active_prio;
    cpu->task->state = RUNNING;
    cpu->process = cpu->task->parent;
    cpu->tslice = sched.tslice;

    SchedSortLowPriority();

    SchedUnlock(NULL);

    _TaskSetTls(cpu->task->memory.tls);

    if(cpu->process != NULL)
    {
        _VirtualSpaceSet(
            NULL,
            (pgt_t)MemoryL2P((ptr_t)cpu->process->Memory.pgt),
            cpu->process->pid
        );
    }

    if(cpu->process) atomic_inc(&cpu->process->tasksRunning);

    _SchedulerStart(cpu->task->memory.registers, cpu->sp);
}

void SchedLock(uint32_t* status)
{
	Klock(&sched.lock, status);
}

void SchedUnlock(uint32_t* status)
{
	Kunlock(&sched.lock, status);
}

void* SchedIrqAttend()
{
    return ((++CPUS[RUNNING_CPU].irqlevel > 1) ? (NULL) : (CPUS[RUNNING_CPU].task->memory.registers));
}

void* SchedIrqExit()
{
    return ((--CPUS[RUNNING_CPU].irqlevel) ? (NULL) : (CPUS[RUNNING_CPU].task->memory.registers));
}

void* Schedule(void *arg, uint32_t irq)
{
	(void)arg; (void)irq;

    uint32_t state;
    SchedLock(&state);

    cpu_t* cpu = &CPUS[RUNNING_CPU];

    if(cpu->task->state == DEAD)
    {
    	atomic_dec(&cpu->process->tasksRunning);
    	_cpus_signal();
		// Invalidate running process
		cpu->process = NULL;
    }
    else
    {
    	cpu->task->on_time += (sched.tslice - cpu->tslice);
		cpu->tslice = sched.tslice;

		if(cpu->prio > SchedPendingPrio())
		{
			// Running task still has highest priority
		    SchedUnlock(&state);

		    return NULL;
		}

		// Put running task on ready list
		if(cpu->process) atomic_dec(&cpu->process->tasksRunning);

		SchedHoldTask(cpu->task);
    }

    // Put new task running
    cpu->task = SchedGetNext2Run();
	cpu->prio = cpu->task->active_prio;;
	cpu->task->state = RUNNING;

	_TaskSetTls(cpu->task->memory.tls);

	cpu->process = cpu->task->parent;

	if(cpu->process) atomic_inc(&cpu->process->tasksRunning);

	SchedSortLowPriority();

    SchedUnlock(&state);

    return NULL;
}


/**
 * SchedGetRunningTask Implementation (See header file for description)
*/
task_t* SchedGetRunningTask()
{
	uint32_t state;
	critical_lock(&state);
	task_t* task = CPUS[RUNNING_CPU].task;
	critical_unlock(&state);

	return task;
}

void* SchedGetRunningTaskTcb()
{
	return CPUS[RUNNING_CPU].task->memory.registers;
}

/**
 * SchedGetRunningProcess Implementation (See header file for description)
*/
process_t* SchedGetRunningProcess()
{
	uint32_t state;
	critical_lock(&state);
	process_t* process = CPUS[RUNNING_CPU].process;
	critical_unlock(&state);

	return process;
}

void SchedKillProcessTasks(process_t* process)
{
	uint32_t status;
	SchedLock(&status);

	// Remove ready tasks from scheduler queue
	task_t *task = GLIST_FIRST(&process->tasks, task_t, siblings);
	while(task)
	{
		if(task->state == READY)
		{
			GlistRemoveSpecific(&task->node);
		}

		task = GLIST_NEXT(&task->siblings, task_t, siblings);
	}

	// Stop running tasks but not the current one
	uint32_t cpu;
	for(cpu = 0; cpu < sched.cpus; ++cpu)
	{
		if(cpu != RUNNING_CPU && CPUS[cpu].process == process)
		{
			CPUS[cpu].task->state = DEAD;
			SchedTrigger(cpu);
		}
	}

	SchedUnlock(&status);
}

void* SchedTerminateRunningTask(bool_t proc_death)
{
	SchedEnsureLock(NULL);

	cpu_t* cpu = &CPUS[RUNNING_CPU];
	process_t* process = cpu->process;

	if(proc_death != TRUE && cpu->process) atomic_dec(&cpu->process->tasksRunning);

	// Resume a new task
	cpu->task = SchedGetNext2Run();
	cpu->task->state = RUNNING;
//	cpu->task->subState = NONE;
	cpu->prio = cpu->task->active_prio;
	cpu->process = cpu->task->parent;

	SchedSortLowPriority();

	// We want to keep interrupts disabled
	SchedUnlock(NULL);

	_TaskSetTls(cpu->task->memory.tls);

	if(cpu->process != NULL && (process == NULL || cpu->process != process))
	{
		// Change process virtual space
		_VirtualSpaceSet(
			NULL,
			(pgt_t)MemoryL2P((ptr_t)cpu->process->Memory.pgt),
			cpu->process->pid
		);
	}

	if(cpu->process) atomic_inc(&cpu->process->tasksRunning);

	// Return task tcb to be resumed
	return cpu->task->memory.registers;
}

int32_t SchedStopRunningTask(uint8_t state, uint8_t substate)
{
    volatile bool_t resume = FALSE;

    // We don't want inc the lock count only ensure that it is locked
    uint32_t status;
    SchedEnsureLock(&status);

    cpu_t* cpu = &CPUS[RUNNING_CPU];
	task_t* task = cpu->task;
	process_t* prev_process = cpu->process;

	if(task->state != DEAD) task->state = state;
	task->subState = substate;
    task->on_time += (sched.tslice - cpu->tslice);

	// Restore timeslice
	cpu->tslice = sched.tslice;

	// Clean task return
	task->ret = 0;

	_TaskSave(task->memory.registers);

	if(resume)
	{
		critical_unlock(&status);	// Restore interrupt status
		return task->ret;			// Caller return point
	}
	else resume = TRUE;

    // Resume a new task
    cpu->task = SchedGetNext2Run();
    cpu->task->state = RUNNING;
//    cpu->task->subState = NONE;
    cpu->prio = cpu->task->active_prio;
    cpu->process = cpu->task->parent;

    SchedSortLowPriority();

    // Before we unlock the scheduler do we have to put stopped task in ready queue
    if(task->state == READY)
    {
        SchedHoldTask(task);
    }

    _TaskSetTls(cpu->task->memory.tls);

    if(cpu->process) atomic_inc(&cpu->process->tasksRunning);

    // Only signal the stop now to avoid problems when the process is terminating
    if(prev_process) atomic_dec(&prev_process->tasksRunning);

    if(cpu->process != NULL)
    {
    	_SchedResumeTask(cpu->task->memory.registers, (pgt_t)MemoryL2P((ptr_t)cpu->process->Memory.pgt), cpu->process->pid);
    }
    else
    {
    	_SchedResumeTask(cpu->task->memory.registers, NULL, 0);
    }

    // Will not return
    return 0;
}


void SchedAddTask(task_t* task)
{
    uint32_t state;
    SchedLock(&state);

    // By default insert task in ready list
    SchedHoldTask(task);

    // We always check priority against the lowest priority running task
    if(task->active_prio > sched.lprio)
    {
        // Avoid scheduler trigger from time slice
        if(sched.lcpu->tslice == 1) sched.lcpu->tslice += 1;
        // Trigger reschedule of the cpu
        SchedTrigger(sched.lcpu->id);
    }

    SchedUnlock(&state);
}


void SchedYield()
{
    cpu_t* cpu = &CPUS[RUNNING_CPU];

    uint32_t state;
    SchedLock(&state);

    if(cpu->prio <= SchedPendingPrio())
    {
        SchedStopRunningTask(READY, NONE);
        // Restore interrupts status before lock
        critical_unlock(&state);
    }
    else
    {
        SchedUnlock(&state);
    }
}

void* SystemTick(void* arg, uint32_t intr)
{
	(void)arg; (void)intr;

	SystemTimerHandler();

    // Send tick to all sleeping tasks
    SleepUpdate();

    // Lock scheduler to ensure that time slices are not corrupted
    uint32_t state;
    SchedLock(&state);

    // Update cpus time slices
    uint32_t cpu;
    for(cpu = 0; cpu < sched.cpus; ++cpu)
    {
        if(--CPUS[cpu].tslice == 0)
        {
            SchedTrigger(CPUS[cpu].id);
        }
    }

    SchedUnlock(&state);

    return NULL;
}

void SchedPriorityResolve(task_t* task, uint16_t prio)
{
//	uint32_t status;
//	SchedLock(&status);

	if(task->state == RUNNING)
	{
		if(sched.lcpu->id == RUNNING_CPU && sched.lprio <= prio)
		{
			SchedSortLowPriority();
		}
	}
	else
	{
		GlistRemoveSpecific(&task->node);
		SchedAddTask(task);
	}

//	SchedUnlock(&status);
}

#include <mutex.h>
#include <ipc_2.h>

void PriorityResolve(task_t* task, uint16_t prio)
{
    if(task->active_prio >= prio)
    {
        return;
    }

    task->active_prio = prio;

    if(task->state == BLOCKED)
    {
        if(task->subState == MUTEX)
        {
//            MutexPriorityResolve(NULL, task, prio);
        }
        else if(task->subState == IPC_SEND || task->subState == IPC_REPLY)
        {
            ChannelPriorityResolve(NULL, task, prio);
        }
    }
    else if(task->state == RUNNING || task->state == READY)
    {
//        SchedPriorityResolve(task, prio);
    }
}

void PriorityAdjust(task_t* task, uint16_t prio)
{
	uint32_t status;
	SchedLock(&status);

	if(task->state == RUNNING || task->state == READY)
	{
		SchedPriorityResolve(task, prio);
	}
	else if(task->state == BLOCKED)
	{
		switch(task->subState)
		{
		case MUTEX:
			MutexPriorityAdjust(task, prio);
			break;
		case COND:
		case SEMAPHORE:
		{
			glist_t* list = (glist_t*)task->node.owner;
			GlistRemoveSpecific(&task->node);
			task->active_prio = prio;
			GlistInsertObject(list, &task->node);
			break;
		}
		case IPC_REPLY:
		case IPC_SEND:
			ChannelPriorityAdjust(task, prio);
			break;
		default:
			task->active_prio = prio;
			break;
		}
	}

	SchedUnlock(&status);
}
