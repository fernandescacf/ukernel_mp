/**
 * @file        rwlock.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        8 December, 2020
 * @brief       Kernel Read/Write Lock Definition Header File
*/

#ifndef _RWLOCK_H_
#define _RWLOCK_H_


/* Includes ----------------------------------------------- */
#include <types.h>


/* Exported types ----------------------------------------- */
typedef struct
{
    int32_t  readers;
    uint32_t writer;
}rwlock_t;


/* Exported constants ------------------------------------- */
#define NO_WRITER   ((uint32_t)(-1))


/* Exported macros ---------------------------------------- */



/* Exported functions ------------------------------------- */

/*
 * @brief   Initialize the in read/write lock
 *
 * @param   lock
 *
 * @retval  No return
 */
void RWlockInit(rwlock_t* lock);

/*
 * @brief   Acquire read access
 *
 * @param   lock
 *
 * @retval  No return
 */
void ReadLock(rwlock_t* lock);

/*
 * @brief   Release read access
 *
 * @param   lock
 *
 * @retval  No return
 */
void ReadUnlock(rwlock_t* lock);

/*
 * @brief   Acquire write access.
 * 			Interrupts will be disabled
 *
 * @param   lock
 * 			status
 *
 * @retval  No return
 */
void WriteLock(rwlock_t* lock, uint32_t* status);

/*
 * @brief   Release write access.
 * 			Interrupts will be restored
 *
 * @param   lock
 * 			status
 *
 * @retval  No return
 */
void WriteUnlock(rwlock_t* lock, uint32_t* status);


#endif /* _RWLOCK_H_ */
