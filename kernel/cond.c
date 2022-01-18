/**
 * @file        cond.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        04 October, 2020
 * @brief       Conditional variables implementation
*/

/* Includes ----------------------------------------------- */
#include <cond.h>
#include <scheduler.h>
#include <kheap.h>
#include <sleep.h>
#include <atomic.h>


/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */
#define COND_MAGIC			(0xAAAADEAD)



/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */

extern int ReadyListSort(glistNode_t *current, glistNode_t *newtask);

//static process_t *GetParentProces(mutex_t *mutex)
//{
//	return GLISTNODE2TYPE(mutex->node.owner, process_t, mutexs);
//}

cond_t* GetCondPtr(cond_t* cond)
{
	if(NULL == cond)
	{
		return NULL;
	}

	if(cond->magic == COND_INITIALIZER)
	{
		if(CondInit((cond_t**)cond) != E_OK)
		{
			return NULL;
		}

		return *((cond_t**)cond);
	}

	if(cond->magic != COND_MAGIC)
	{
		cond_t* mx = *((cond_t**)cond);

		if((mx == NULL) || (mx->magic != COND_MAGIC))
		{
			return NULL;
		}

		cond = mx;
	}

	// Highly unlikely but is better to be sure
//	if(SchedulerGetRunningProcess() == GetParentProces(cond))
//	{
//		return cond;
//	}

	return cond;
}

void CondResumeTimeout(void* cond, task_t* task)
{
	(void)cond;

	if(GlistRemoveSpecific(&task->node) != E_OK)
	{
		// We were no longer in the queue so this timeout has no effect
		return;
	}

	task->ret = E_TIMED_OUT;

	SchedAddTask(task);
}

/* Private functions -------------------------------------- */

cond_t* CondCreate()
{
	cond_t* cond = (cond_t*)kmalloc(sizeof(cond_t));

	cond->magic = COND_MAGIC;
	cond->mutex = NULL;

	GlistInitialize(&cond->queue, GList);
	GlistSetSort(&cond->queue, ReadyListSort);

//	GlistInsertObject(&SchedulerGetRunningProcess()->conds, &cond->node);

	return cond;
}

int32_t CondInit(cond_t** cond)
{
	if(cond == NULL)
	{
		return E_INVAL;
	}

	*cond = CondCreate();

	if(*cond == NULL)
	{
		return E_ERROR;
	}

	return E_OK;
}

int32_t CondWait(cond_t* cond, mutex_t** mutex)
{
	cond = GetCondPtr(cond);

	if(cond == NULL || (cond->mutex != NULL && cond->mutex != *mutex))
	{
		return E_INVAL;
	}

	task_t* task = SchedGetRunningTask();

	if(task != (*mutex)->owner)
	{
		return E_INVAL;
	}

	if(cond->mutex == NULL && atomic_cmp_set((uint32_t*)&cond->mutex, NULL, *((uint32_t*)mutex)) != E_OK)
	{
		return E_ERROR;
	}

	GlistInsertObject(&cond->queue, &task->node);

	if(task->timeout.set == TRUE)
	{
		TimerSet(task, CondResumeTimeout, cond);
	}

	uint32_t status;
	SchedLock(&status);

	MutexUnlock((*mutex));

	int32_t ret = SchedStopRunningTask(BLOCKED, COND);

	critical_unlock(&status);

	if(task->timeout.set == TRUE)
	{
		TimerStop(task);
	}

	if(ret != E_OK)
	{
		return ret;
	}

	ret = MutexLock((*mutex));

	if(GLIST_EMPTY(&cond->queue))
	{
		cond->mutex = NULL;
	}

	return ret;

}

int32_t CondSignal(cond_t* cond, int32_t count)
{
	cond = GetCondPtr(cond);

	if(cond == NULL || count < 0 || MutexLock(cond->mutex) != E_OK)
	{
		return E_INVAL;
	}

	if((count == 0) || (count > cond->queue.count))
	{
		count = cond->queue.count;
	}

	for(; count > 0; --count)
	{
		task_t* task = GLISTNODE2TYPE(GlistRemoveFirst(&cond->queue), task_t, node);
		task->ret = E_OK;
		SchedAddTask(task);
	}

	MutexUnlock(cond->mutex);

	// Are we still the highest priority task
	SchedYield();

	return E_OK;
}

int32_t CondDestroy(cond_t* cond)
{
	cond = GetCondPtr(cond);

	if(cond == NULL)
	{
		return E_INVAL;
	}

	if(cond->queue.count)
	{
		return E_BUSY;
	}

	// Invalidate conditional variable
	cond->magic = 0x0;

	while(!GLIST_EMPTY(&cond->queue))
	{
		task_t* task = GLISTNODE2TYPE(GlistRemoveFirst(&cond->queue), task_t, node);
		task->ret = E_ERROR;
		SchedAddTask(task);
	}

//	GlistRemoveSpecific(&cond->node);

	kfree(cond, sizeof(*cond));

	// Are we still the highest priority task
	SchedYield();

	return E_OK;
}
