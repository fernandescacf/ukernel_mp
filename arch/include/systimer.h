/**
 * @file        systimer.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        17 November, 2018
 * @brief       ukernel system timer header file
*/

#ifndef _SYSTIMER_H_
#define _SYSTIMER_H_

#ifdef __cplusplus
    extern "C" {
#endif


/* Includes ----------------------------------------------- */
#include <types.h>


/* Exported types ----------------------------------------- */



/* Exported constants ------------------------------------- */

/*
 * Timer mode macro. Used as a parameter for timer_init()
 * Used to set the local timer to auto reload mode.
 */
#define AUTO_RELOAD_TIMER  		0

/*
 * Timer mode macro. Used as a parameter for timer_init()
 * Used to set the local timer to single shot mode.
 */
#define ONE_SHOT_TIMER     		1

/*
 * Timer mode macro. Used as a parameter for timer_init()
 * Used to disable the timer.
 */
#define DISABLE_TIMER      		2


/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */

void SystemTickStart(uint32_t usec, void* (*handler)(void*, uint32_t));

void SytemTimerInit(uint32_t mode, uint32_t u_sec);

void *SytemTimerReset();

void SystemTimerHandler();

int32_t SystemTimerIrq();

#ifdef __cplusplus
    }
#endif

#endif /* _SYSTIMER_H_ */
