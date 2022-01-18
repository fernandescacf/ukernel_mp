/**
 * @file        cond.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        04 October, 2020
 * @brief       Conditional variables Definition Header File
*/

#ifndef _COND_H_
#define _COND_H_


/* Includes ----------------------------------------------- */
#include <types.h>
#include <glist.h>
#include <mutex.h>
#include <task.h>


/* Exported types ----------------------------------------- */


typedef struct
{
	uint32_t    magic;
	glistNode_t node;
	mutex_t*	mutex;
	glist_t		queue;
}cond_t;


/* Exported constants ------------------------------------- */
#define COND_INITIALIZER	0x01010101


/* Exported macros ---------------------------------------- */


/* Private functions -------------------------------------- */


/* Exported functions ------------------------------------- */

cond_t* CondCreate();

int32_t CondInit(cond_t** cond);

int32_t CondWait(cond_t* cond, mutex_t** mutex);

int32_t CondSignal(cond_t* cond, int32_t count);

int32_t CondDestroy(cond_t* cond);

#endif /* _COND_H_ */
