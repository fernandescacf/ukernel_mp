/**
 * @file        klock.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        30 October, 2020
 * @brief       In Kernel Lock Definition Header File
*/

#ifndef _KLOCK_H_
#define _KLOCK_H_


/* Includes ----------------------------------------------- */
#include <types.h>


/* Exported types ----------------------------------------- */
typedef struct
{
    uint32_t count;
    uint32_t owner;
}klock_t;


/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */



/* Exported functions ------------------------------------- */

/*
 * @brief   Initialize the in kernel lock
 *
 * @param   lock - in kernel lock
 *
 * @retval  No return
 */
void KlockInit(klock_t* lock);

/*
 * @brief   Lock the specified in kernel lock.
 *          Will ensure that interrupts are disabled and if requested save
 *          the previous interrupt status.
 *          NOTE: This lock is recursive, so lock attempts by the same cpu
 *          will increase the internal counter and be granted access
 *
 * @param   lock - in kernel lock
 * 			status - variable to save current interrupt status
 *
 * @retval  No return
 */
void Klock(klock_t* lock, uint32_t* status);

/*
 * @brief   Unlock the in kernel lock.
 *          The unlock will only occur if the calling cpu owns the lock
 *          and count has reach zero.
 *          If a saved status is provided it will be restored otherwise
 *          interrupts will remain disabled
 *
 * @param   lock - in kernel lock
 * 			status - variable with the saved interrupt status
 *
 * @retval  No return
 */
void Kunlock(klock_t* lock, uint32_t* status);

/*
 * @brief   Only obtain the lock (increment the counter) if calling cpu
 * 			does not own the lock.
 * 			If not locked will act has the Klock call
 *
 * @param   lock - in kernel lock
 * 			status - variable to save current interrupt status
 *
 * @retval  No return
 */
void KlockEnsure(klock_t* lock, uint32_t* status);


#endif /* _KLOCK_H_ */
