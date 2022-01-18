/**
 * @file        semaphohre.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        04 July, 2020
 * @brief       Kernel Semaphores Definition Header File
*/

#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_


/* Includes ----------------------------------------------- */
#include <types.h>
#include <glist.h>
#include <spinlock.h>

#include <task.h>


/* Exported types ----------------------------------------- */

typedef struct
{
	uint32_t    magic;
	glistNode_t node;
	int32_t	    counter;
	spinlock_t	spinLock;
	glist_t		lockQueue;
}sem_t;


/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */


/* Private functions -------------------------------------- */


/* Exported functions ------------------------------------- */

sem_t *SemCreate(uint32_t value);

int32_t SemInit(sem_t **sem, int32_t pshared, uint32_t value);

int32_t SemWait(sem_t *sem);

int32_t SemPost(sem_t *sem);

int32_t SemDestroy(sem_t *sem);

#endif /* _SEMAPHORE_H_ */
