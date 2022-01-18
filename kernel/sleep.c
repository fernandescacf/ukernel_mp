/**
 * @file        sleep.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        September 18, 2016
 * @brief       Sleep Implementation Header File
*/

/* Includes ----------------------------------------------- */

#include <glist.h>
#include <scheduler.h>
#include <sleep.h>

/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */



/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */

struct
{
	glist_t	list;
}Sleep_Handler;


/* Private function prototypes ---------------------------- */

int32_t SleepStort(glistNode_t *current, glistNode_t *task)
{
    task_t *c = GLISTNODE2TYPE(current, task_t, timeout.node);
    task_t *t = GLISTNODE2TYPE(task, task_t, timeout.node);

    if(t->timeout.pendTime >= c->timeout.pendTime)
    {
        t->timeout.pendTime -= c->timeout.pendTime;
        return 1;
    }
    else
    {
        c->timeout.pendTime -= t->timeout.pendTime;
        return 0;
    }
}

int32_t SleepMatch(glistNode_t *gnode, void *id)
{
    task_t *task = GLISTNODE2TYPE(gnode, task_t, timeout.node);

    if(task->tid == (uint32_t)id)
    {
        task_t *tnext = GLIST_NEXT(&task->timeout.node, task_t, timeout.node);

        if(tnext)
        {
            tnext->timeout.pendTime += task->timeout.pendTime;
        }
        return 0;
    }
    else
    {
        return 1;
    }
}


/* Private functions -------------------------------------- */


uint32_t SleepInit()
{
	GlistInitialize(&Sleep_Handler.list, GList);
	(void)GlistSetSort(&Sleep_Handler.list, SleepStort);
	(void)GlistSetCmp(&Sleep_Handler.list, SleepMatch);
	return E_OK;
}



void SleepInsert(uint32_t time)
{
	task_t *task = SchedGetRunningTask();
//	task->info.subState = SLEEPING;
	task->timeout.pendTime = time;
	GlistInsertObject(&Sleep_Handler.list, &task->timeout.node);
	SchedStopRunningTask(BLOCKED, SLEEPING);
}

void TimeoutSet(uint32_t time, int32_t type)
{
	task_t* task = SchedGetRunningTask();
	task->timeout.waitTime = time;
	task->timeout.set = TRUE;
	task->timeout.type = ((type == TIMER_AUTO_RELOAD) ? (TIMER_AUTO_RELOAD) : (TIMER_NO_RELOAD));
}

void TimeoutUnset()
{
	task_t* task = SchedGetRunningTask();
	task->timeout.waitTime = 0;
	task->timeout.set = FALSE;
	task->timeout.type = TIMER_NO_RELOAD;
}

void TimerSet(task_t* task, void (*handler)(void*,task_t*), void* arg)
{
	task->timeout.handler = handler;
	task->timeout.arg = arg;
	task->timeout.pendTime = task->timeout.waitTime;
	GlistInsertObject(&Sleep_Handler.list, &task->timeout.node);
}

void TimerStop(task_t* task)
{
	SleepRemove(task);
	if(task->timeout.type == TIMER_NO_RELOAD)
	{
		task->timeout.waitTime = 0;
		task->timeout.set = FALSE;
	}
}

void SleepRemove(task_t *task)
{
    task_t *tnext = GLIST_NEXT(&task->timeout.node, task_t, timeout.node);
    GlistRemoveSpecific(&task->timeout.node);
    if(tnext)
    {
        tnext->timeout.pendTime += task->timeout.pendTime;
    }
}

void SleepUpdate()
{
    task_t *task = GLIST_FIRST(&Sleep_Handler.list, task_t, timeout.node);
    if(task)
    {
        task->timeout.pendTime--;
    }
    else
    {
        return;
    }

    while(task && !task->timeout.pendTime)
    {
    	GlistRemoveSpecific(&task->timeout.node);

    	// If sub state was not Sleeping it was a time out so we need to call the timeout handler
    	if(task->subState != SLEEPING)
    	{
    		task->timeout.handler(task->timeout.arg, task);
    		if(task->timeout.type == TIMER_NO_RELOAD)
    		{
    			task->timeout.set = FALSE;
    		}
    	}
    	else
    	{
    		SchedAddTask(task);
    	}

        task = GLIST_FIRST(&Sleep_Handler.list, task_t, timeout.node);
    }
}
