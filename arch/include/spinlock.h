/**
 * @file        spinlock.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        11 September, 2016
 * @brief       Spin Lock functions
*/

#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#ifdef __cplusplus
    extern "C" {
#endif

/* Includes ----------------------------------------------- */
#include <types.h>


/* Exported types ----------------------------------------- */
typedef uint32_t spinlock_t;

/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */

/*
 * @brief   Starts a critical region (only disables interrupts)
 * @param   state - used to save the old interrupt state
 * @retval  No return
 */
void critical_lock(uint32_t* state);

/*
 * @brief   Ends a critical region and restores the old interrupt state
 * @param   state - old interrupt state
 * @retval  No return
 */
void critical_unlock(uint32_t* state);

/*
 * @brief   Initializes the spin lock
 * @param   lock - the spin lock
 * @retval  No value returned
 */
void spinlock_init(spinlock_t* lock);

/*
 * @brief   Acquires the specified spin lock
 * @param   lock - the spin lock
 * @retval  No value returned
 */
void spinlock(spinlock_t* lock);

/*
 * @brief   Releases the specified spin lock
 * @param   lock - the spin lock
 * @retval  No value returned
 */
void spinunlock(spinlock_t* lock);

/*
 * @brief   Acquire spin lock and disables interrupts
 * 			and saves the old interrupt state
 * @param   lock - the spin lock
 * 			state - saves the old interrupt state
 * @retval  No value returned
 */
void spinlock_irq(spinlock_t* lock, uint32_t* state);

/*
 * @brief   Release spin lock and restores old interrupt state
 * @param   lock - the spin lock
 * 			state - old interrupt state
 * @retval  No value returned
 */
void spinunlock_irq(spinlock_t* lock, uint32_t* state);


#ifdef __cplusplus
    }
#endif

#endif /* _SPINLOCK_H_ */
