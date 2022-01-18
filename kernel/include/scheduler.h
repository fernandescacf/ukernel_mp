/**
 * @file        scheduler.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        10 April, 2020
 * @brief       Scheduler Manager Definition Header File
*/

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_


/* Includes ----------------------------------------------- */
#include <proctypes.h>
#include <arch.h>


/* Exported types ----------------------------------------- */


/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */



/* Private functions -------------------------------------- */



/* Exported functions ------------------------------------- */

/*
 * @brief   Initialize the scheduler. It will not put the scheduler running.
 * 			After initialization the scheduler is disabled and needs to be
 * 			explicit enabled using Scheduler Resume
 *
 * @param   No parameters
 *
 * @retval  No return
 */
int32_t SchedulerInit(uint32_t schedHz);

void SchedulerStart();

void SchedLock(uint32_t* status);

void SchedUnlock(uint32_t* status);

/*
 * @brief   Routine to get the  current running task
 *
 * @param   No parameters
 *
 * @retval  Pointer to running task
 */
task_t* SchedGetRunningTask();

/*
 * @brief   Routine to get the  current running process
 *
 * @param   No parameters
 *
 * @retval  Pointer to running process
 */
process_t* SchedGetRunningProcess();

/*
 * @brief   Routine to trigger the scheduler to put the highest priority ready
 * 			task to run. If scheduler is not enabled or preemption is disabled
 * 			this call will have no effect
 *
 * @param   No parameters
 *
 * @retval  No return
 */
void* Schedule(void *arg, uint32_t irq);

/*
 * @brief   Routine to add a new task to the scheduler ready list
 *
 * @param   task - task to be added
 *
 * @retval  No return
 */
void SchedAddTask(task_t* task);

/*
 * @brief   Routine to suspend the current running task (will not be added
 * 			to the scheduler ready list) and get a new ready task to be
 * 			executed
 *
 * @param   No parameters
 *
 * @retval  In case an error occurred (e.g. timeout) an error is returned
 */
int32_t SchedStopRunningTask(uint8_t state, uint8_t substate);

void SchedKillProcessTasks(process_t* process);

/*
 * @brief   System Call to give the opportunity for other ready task to be put
 * 			to run
 *
 * @param   No parameters
 *
 * @retval  No return
 */
void SchedYield();

/*
 * @brief   Call back from the kernel system timer. Used to implement the time slicing
 *
 * @param   arg -
 * 			intr -
 *
 * @retval  No return
 */
void* SystemTick(void* arg, uint32_t intr);

/*
 * @brief   Routine to set the suspend the scheduler when in attending an interrupt
 *
 * @param   No Parameters
 *
 * @retval  Pointer to the running task TCB area
 */
void* SchedIrqAttend();

void* SchedIrqExit();

void* SchedTerminateRunningTask();

void PriorityResolve(task_t* task, uint16_t prio);

#endif /* _SCHEDULER_H_ */
