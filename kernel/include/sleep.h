/**
 * @file        sleep.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        September 18, 2016
 * @brief       Sleep Definition Header File
*/

#ifndef _SLEEP_H
#define _SLEEP_H


/* Includes ----------------------------------------------- */
#include <types.h>
#include <task.h>


/* Exported constants ------------------------------------- */
#define TIMER_NO_RELOAD		(0x0)
#define TIMER_AUTO_RELOAD   (0x1)


/* Exported types ----------------------------------------- */



/* Exported macros ---------------------------------------- */



/* Exported functions ------------------------------------- */

/*
 * @brief   Initializes the Sleep manager
 * @param   None
 * @retval  Success
 */
uint32_t SleepInit();

/*
 * @brief   Routine to put task to sleep
 * @param   time - sleep time
 * @retval  No return
 */
void SleepInsert(uint32_t time);

/*
 * @brief   Removes a task from the sleeping queue
 * @param   task - task to be removed
 * @retval  No return
 */
void SleepRemove(task_t *task);

/*
 * @brief   Routine to update the sleeping time
 * @param   No arguments
 * @retval  No return
 */
void SleepUpdate();

void TimeoutSet(uint32_t time, int32_t type);

void TimerSet(task_t* task, void (*handler)(void*,task_t*), void* arg);

void TimerStop(task_t* task);

#endif /* _SLEEP_H */
