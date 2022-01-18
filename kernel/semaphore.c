/**
 * @file        sempahore.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        04 July, 2020
 * @brief       Kernel Semaphore implementation
*/

/* Includes ----------------------------------------------- */
#include <semaphore.h>
#include <scheduler.h>
#include <kheap.h>
#include <atomic.h>
#include <sleep.h>


/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */
#define SEM_MAGIC			(0xAAAADEAD)


/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */

extern int ReadyListSort(glistNode_t *current, glistNode_t *newtask);

//static process_t *GetParentProces(sem_t *sem)
//{
//	return GLISTNODE2TYPE(sem->node.owner, process_t, mutexs);
//}

sem_t *GetSemPtr(sem_t *sem)
{
	if(NULL == sem)
	{
		return NULL;
	}

	if(sem->magic != SEM_MAGIC)
	{
		sem_t *mx = *((sem_t **)sem);

		if((mx == NULL) || (mx->magic != SEM_MAGIC))
		{
			return NULL;
		}

		sem = mx;
	}

	// Highly unlikely but is better to be sure
//	if(SchedulerGetRunningProcess() == GetParentProces(sem))
//	{
//		return sem;
//	}

	return sem;
}

void SemResumeTimeout(void* semaphore, task_t* task)
{
	sem_t* sem = (sem_t*)semaphore;

    uint32_t state = 0;
	spinlock_irq(&sem->spinLock, &state);

	if(GlistRemoveSpecific(&task->node) != E_OK)
	{
		// We were no longer in the queue so this timeout has no effect
		spinunlock_irq(&sem->spinLock, &state);
		return;
	}

	sem->counter += 1;

	spinunlock_irq(&sem->spinLock, &state);

	task->ret = E_TIMED_OUT;

	SchedAddTask(task);
}

/* Private functions -------------------------------------- */

sem_t *SemCreate(uint32_t value)
{
	sem_t *sem = (sem_t*)kmalloc(sizeof(sem_t));

	sem->magic   = SEM_MAGIC;
	sem->counter = (int32_t)value;
	spinlock_init(&sem->spinLock);

	GlistInitialize(&sem->lockQueue, GList);
	GlistSetSort(&sem->lockQueue, ReadyListSort);

//	GlistInsertObject(&SchedulerGetRunningProcess()->semaphores, &sem->node);

	return sem;
}

int32_t SemInit(sem_t **sem, int32_t pshared, uint32_t value)
{
	// pshared is ignored
	(void)pshared;

	if(sem == NULL)
	{
		return E_INVAL;
	}

	*sem = SemCreate(value);

	if(*sem == NULL)
	{
		return E_ERROR;
	}

	return E_OK;
}

int32_t SemWait(sem_t *sem)
{
	sem = GetSemPtr(sem);

	if(sem == NULL)
	{
		return E_INVAL;
	}

    uint32_t state = 0;
	spinlock_irq(&sem->spinLock, &state);

	if(atomic_dec(&sem->counter) >= 0)
	{
		spinunlock_irq(&sem->spinLock, &state);
		return E_OK;
	}

	if(sem->counter < 0)
	{
		task_t *task = SchedGetRunningTask();

		GlistInsertObject(&sem->lockQueue, &task->node);

		if(task->timeout.set == TRUE)
		{
			TimerSet(task, SemResumeTimeout, sem);
		}

		// Lock scheduler to suspend task
		SchedLock(NULL);

		// Free semaphore lock. We are done with the semaphore handling
		spinunlock(&sem->spinLock);

		int32_t ret = SchedStopRunningTask(BLOCKED, SEMAPHORE);

		// We still have interrupts disable for this task so we have to enable them
		critical_unlock(&state);

		// Clean time out handling
		if(task->timeout.set == TRUE)
		{
			TimerStop(task);
		}

		return ret;
	}

	spinunlock_irq(&sem->spinLock, &state);

	return E_OK;
}

int32_t SemPost(sem_t *sem)
{
	sem = GetSemPtr(sem);

	if(sem == NULL)
	{
		return E_INVAL;
	}

    uint32_t state = 0;
	spinlock_irq(&sem->spinLock, &state);

	if(++sem->counter < 1)
	{
		SchedAddTask(GLISTNODE2TYPE(GlistRemoveFirst(&sem->lockQueue), task_t, node));
	}

	spinunlock_irq(&sem->spinLock, &state);

	return E_OK;
}

int32_t SemDestroy(sem_t *sem)
{
	sem = GetSemPtr(sem);

	if(sem == NULL)
	{
		return E_INVAL;
	}

	sem->magic = 0x0;

	while(sem->counter++ < 0)
	{
		SchedAddTask(GLISTNODE2TYPE(GlistRemoveFirst(&sem->lockQueue), task_t, node));
	}

	kfree(sem, sizeof(*sem));

	return E_OK;
}
