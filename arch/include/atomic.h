/**
 * @file        atomic.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        9 August, 2016
 * @brief       Atomic operations functions
*/

#ifndef _ATOMIC_H_
#define _ATOMIC_H_

#ifdef __cplusplus
    extern "C" {
#endif

/* Includes ----------------------------------------------- */
#include <types.h>


/* Exported types ----------------------------------------- */


/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */

/*
 * @brief   Increments in an atomic way the specified variable
 * @param   data - pointer to the variable being incremented
 * @retval  Result
 */
int32_t atomic_inc(int32_t* data);

/*
 * @brief   Decrements in an atomic way the specified variable
 * @param   data - pointer to the variable being decremented
 * @retval  Result
 */
int32_t atomic_dec(int32_t* data);

/*
 * @brief   If cmp is equal to *dst set the *dst with the specified value
 * @param   dst 	- pointer to the variable
 * 			cmp		- comparison value
 * 			value	- set value
 * @retval  0 - if the set is performed
 * 			1 - if the set isn't performed
 */
int32_t atomic_cmp_set(uint32_t* dst, uint32_t cmp, uint32_t value);

/*
 * @brief   Exchanges the value of *p1 an *p2 in an atomic manner
 * @param   location 	- pointer to the atomic variable
 * 			value		- has the new data to be written to the atomic variable and
 * 						  than saves the old value of the atomic variable
 * @retval  No value returned
 */
void atomic_exchange(uint32_t* p1, uint32_t* p2);

#ifdef __cplusplus
    }
#endif

#endif /* _ATOMIC_H_ */
