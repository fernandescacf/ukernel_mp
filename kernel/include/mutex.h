/**
 * @file        mutex.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        07 November, 2018
 * @brief       Inside kernel Mutexs Definition Header File
*/

#ifndef _MUTEX_H_
#define _MUTEX_H_


/* Includes ----------------------------------------------- */
#include <types.h>
#include <glist.h>
#include <spinlock.h>

#include <task.h>


/* Exported types ----------------------------------------- */

typedef struct
{
	uint32_t    magic;
	glistNode_t pnode;
	uint32_t	lock;
	spinlock_t	spinLock;
	glist_t		lockQueue;
	task_t		*owner;
	glistNode_t tnode;
}mutex_t;


/* Exported constants ------------------------------------- */
#define MUTEX_INITIALIZER	0x10101010


/* Exported macros ---------------------------------------- */


/* Private functions -------------------------------------- */


/* Exported functions ------------------------------------- */

mutex_t *MutexCreate( void );

int32_t MutexInit(mutex_t **mutex);

int32_t MutexLock(mutex_t *mutex);

int32_t MutexUnlock(mutex_t *mutex);

int32_t MutexTrylock(mutex_t *mutex);

int32_t MutexListSort(glistNode_t* current, glistNode_t* newmutex);

void MutexPriorityResolve(mutex_t* mutex, task_t* task, uint16_t prio);

void MutexPriorityAdjust(task_t* task, uint16_t prio);

#endif /* _MUTEX_H_ */
