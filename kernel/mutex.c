/**
 * @file        mutex.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        07 November, 2018
 * @brief       Inside kernel Mutexs implementation
*/

/* Includes ----------------------------------------------- */
#include <mutex.h>
#include <scheduler.h>
#include <kheap.h>
#include <sleep.h>


/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */
#define MUTEX_MAGIC			(0xAAAADEAD)


#define MUTEX_UNLOCK		(0)
#define MUTEX_LOCK			(1)

/* Private macros ----------------------------------------- */
#define MUTEX_PRIO_CELLING(mutex)	\
		(((mutex)->lockQueue.count == 0) ? (0) : (GLISTNODE2TYPE(GlistGetFirst(&(mutex)->lockQueue), task_t, node)->active_prio))


/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */

extern int ReadyListSort(glistNode_t *current, glistNode_t *newtask);

static process_t *GetParentProces(mutex_t *mutex)
{
	return GLISTNODE2TYPE(mutex->pnode.owner, process_t, mutexs);
}

mutex_t *GetMutexPtr(mutex_t *mutex)
{
	if(NULL == mutex)
	{
		return NULL;
	}

	if(mutex->magic == MUTEX_INITIALIZER)
	{
		if(MutexInit((mutex_t**)mutex) != E_OK)
		{
			return NULL;
		}

		return *((mutex_t**)mutex);
	}

	if(mutex->magic != MUTEX_MAGIC)
	{
		mutex_t *mx = *((mutex_t**)mutex);

		if((mx == NULL) || (mx->magic != MUTEX_MAGIC))
		{
			return NULL;
		}

		mutex = mx;
	}

	// Highly unlikely but is better to be sure
	if(SchedGetRunningProcess() == GetParentProces(mutex))
	{
		return mutex;
	}

	return NULL;
}

void MutexResumeTimeout(void* mutex, task_t* task)
{
	uint32_t state = 0;
	bool_t  restoreprio = FALSE;

	mutex_t* mx = (mutex_t*)mutex;

	spinlock_irq(&mx->spinLock, &state);

	if(task == GLIST_FIRST(&mx->lockQueue, task_t, node))
	{
		if(mx->owner->active_prio != mx->owner->real_prio)
		{
			mx->owner->active_prio = mx->owner->real_prio;
			restoreprio = TRUE;
		}
	}

	if(GlistRemoveSpecific(&task->node) != E_OK)
	{
		// We were no longer in the queue so this timeout has no effect
		spinunlock_irq(&mx->spinLock, &state);

		return;
	}


	task_t* head = GLIST_FIRST(&mx->lockQueue, task_t, node);
	if(restoreprio == TRUE && head != NULL && head->active_prio > mx->owner->real_prio)
	{
		mx->owner->active_prio = head->active_prio;
	}

	task->ret = E_TIMED_OUT;

	spinunlock_irq(&mx->spinLock, &state);

	SchedAddTask(task);
}

/* Private functions -------------------------------------- */

int32_t MutexListSort(glistNode_t* current, glistNode_t* newmutex)
{
	mutex_t* currentMutex = GLISTNODE2TYPE(current, mutex_t, tnode);
	mutex_t* mutex = GLISTNODE2TYPE(newmutex, mutex_t, tnode);

	return (MUTEX_PRIO_CELLING(currentMutex) - MUTEX_PRIO_CELLING(mutex));
}


mutex_t *MutexCreate()
{
	mutex_t *mutex = (mutex_t*)kmalloc(sizeof(mutex_t));

	mutex->magic = MUTEX_MAGIC;
	mutex->lock = MUTEX_UNLOCK;
	mutex->owner = NULL;

	spinlock_init(&mutex->spinLock);

	GlistInitialize(&mutex->lockQueue, GList);
	GlistSetSort(&mutex->lockQueue, ReadyListSort);

	GlistInsertObject(&SchedGetRunningProcess()->mutexs, &mutex->pnode);

	return mutex;
}

int32_t MutexInit(mutex_t **mutex)
{
	if(mutex == NULL)
	{
		return E_INVAL;
	}

	*mutex = MutexCreate();

	if(*mutex == NULL)
	{
		return E_ERROR;
	}

	return E_OK;
}

int32_t MutexLock(mutex_t *mutex)
{
	mutex = GetMutexPtr(mutex);

	if(mutex == NULL)
	{
		return E_INVAL;
	}

	uint32_t state = 0;

	task_t *task = SchedGetRunningTask();

	if(task == mutex->owner)
	{
		return E_INVAL;
	}

	spinlock_irq(&mutex->spinLock, &state);

	if(MUTEX_LOCK == mutex->lock)
	{
		GlistInsertObject(&mutex->lockQueue, &task->node);

		if(mutex->owner->active_prio < task->active_prio)
		{
			// TODO: Resolve priority!!!!
			// Solve priority inversion
			mutex->owner->active_prio = task->active_prio;

			if(mutex->owner->state != RUNNING)
			{
				// Remove task from pending list and reinsert it with new priority
				glist_t *owner = (glist_t *)mutex->owner->node.owner;
				GlistRemoveSpecific(&mutex->owner->node);
				GlistInsertObject(owner, &mutex->owner->node);
			}
		}

		if(task->timeout.set == TRUE)
		{
			TimerSet(task, MutexResumeTimeout, mutex);
		}

		// Lock scheduler to suspend task
		SchedLock(NULL);

		// Free mutex lock. We are done with the mutex handling
		spinunlock(&mutex->spinLock);

		int32_t ret = SchedStopRunningTask(BLOCKED, MUTEX);

		// We still have interrupts disable for this task so we have to enable them
		critical_unlock(&state);

		return ret;
	}

	mutex->lock = MUTEX_LOCK;
	mutex->owner = task;
	GlistInsertObject(&task->owned_mutexs, &mutex->tnode);
	spinunlock_irq(&mutex->spinLock, &state);

	return E_OK;
}

int32_t MutexUnlock(mutex_t *mutex)
{
	mutex = GetMutexPtr(mutex);

	if(mutex == NULL)
	{
		return E_INVAL;
	}

	task_t *task = SchedGetRunningTask();

	if(MUTEX_LOCK != mutex->lock || mutex->owner != task)
	{
		return E_ERROR;
	}

	uint32_t state = 0;

	spinlock_irq(&mutex->spinLock, &state);

	// Remove mutex from task owned mutexs
	GlistRemoveSpecific(&mutex->tnode);

	task_t* nextTask = GLISTNODE2TYPE(GlistRemoveFirst(&mutex->lockQueue), task_t, node);

	if(NULL != nextTask)
	{
		mutex->owner = nextTask;

		if(nextTask->timeout.set == TRUE)
		{
			TimerStop(nextTask);
		}

		GlistInsertObject(&nextTask->owned_mutexs, &mutex->tnode);

		spinunlock_irq(&mutex->spinLock, &state);

		SchedAddTask(nextTask);
	}
	else
	{
		mutex->lock = MUTEX_UNLOCK;
		mutex->owner = NULL;
		spinunlock_irq(&mutex->spinLock, &state);
	}

	// TODO: Resolve priority
//	uint16_t next_prio = task->real_prio;

//	if(task->owned_mutexs.count > 0)
//	{
//		uint16_t pend_prio = MUTEX_PRIO_CELLING(GLISTNODE2TYPE(GlistGetFirst(&task->owned_mutexs), mutex_t, tnode));

//		if(pend_prio > next_prio) next_prio = pend_prio;
//	}

//	if(next_prio != task->active_prio)
//	{
//		PriorityAdjust(mutex->owner, next_prio);
//	}

	if(task->real_prio < task->active_prio)
	{
		task->active_prio = task->real_prio;
	}

	// In case the priority changed trigger the scheduler
	SchedYield();

	return E_OK;
}

int32_t MutexTrylock(mutex_t *mutex)
{
	mutex = GetMutexPtr(mutex);

	if(mutex == NULL)
	{
		return E_INVAL;
	}

	uint32_t state = 0;

	spinlock_irq(&mutex->spinLock, &state);

	if(MUTEX_LOCK != mutex->lock)
	{
		mutex->lock = MUTEX_LOCK;
		mutex->owner = SchedGetRunningTask();
		GlistInsertObject(&mutex->owner->owned_mutexs, &mutex->tnode);

		spinunlock_irq(&mutex->spinLock, &state);

		return E_OK;
	}

	spinunlock_irq(&mutex->spinLock, &state);

	return E_BUSY;
}

int32_t MutexDestroy(mutex_t *mutex)
{
	mutex_t* original = mutex;
	mutex = GetMutexPtr(mutex);

	if(mutex == NULL)
	{
		return E_INVAL;
	}

	uint32_t state = 0;

	spinlock_irq(&mutex->spinLock, &state);

	if(mutex->lock == MUTEX_LOCK)
	{
		spinunlock_irq(&mutex->spinLock, &state);

		return E_BUSY;
	}

	// TODO: Under Test!!!!
	if(original->magic == MUTEX_MAGIC)
	{
		original->magic = 0x0;
	}
	else
	{
		(*(mutex_t**)original) = 0x0;
	}

	GlistRemoveSpecific(&mutex->pnode);
	GlistRemoveSpecific(&mutex->tnode);

	kfree(mutex, sizeof(*mutex));

	return E_OK;
}


void MutexPriorityAdjust(task_t* task, uint16_t prio)
{
	uint32_t state = 0;
	mutex_t* mutex = (mutex_t*)task->block_on;

	spinlock_irq(&mutex->spinLock, &state);

	GlistRemoveSpecific(&task->node);
	task->active_prio = prio;
	GlistInsertObject(&mutex->lockQueue, &task->node);

	// Reinsert mutex on owner list
	GlistRemoveSpecific(&mutex->tnode);
	GlistInsertObject(&mutex->owner->owned_mutexs, &mutex->tnode);

	spinunlock_irq(&mutex->spinLock, &state);

	// Do we need to adjust the mutex owner priority?
	if(mutex->owner->active_prio < prio)
	{
//		PriorityAdjust(mutex->owner, prio);
	}
}

void MutexPriorityResolve(mutex_t* mutex, task_t* task, uint16_t prio)
{
    // Lock mutex
	uint32_t status;
	spinlock_irq(&mutex->spinLock, &status);
	// Reinsert task in mutex locked list
	if(GlistRemoveSpecific(&task->node) != E_OK)
	{
		// Did the task became the mutex owner
		if(mutex->owner == task)
		{
			return;
		}
	}
	GlistInsertObject(&mutex->lockQueue, &task->node);

//    if(mutex->priority < prio)
//    {
//    	PriorityResolve(mutex->owner, prio);
//    	mutex->priority = prio;
//    }

    spinunlock_irq(&mutex->spinLock, &status);
}
