/**
 * @file        systimer.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        17 November, 2018
 * @brief		Cortex a9 private timer implementation
*/

/* Includes ----------------------------------------------- */
#include <systimer.h>
#include <board.h>
#include <isr.h>


/* Private types ------------------------------------------ */

typedef struct
{
	volatile uint32_t pt_load_reg;
	volatile uint32_t pt_counter_reg;
	volatile uint32_t pt_control_reg;
	volatile uint32_t pt_interrupt_status_reg;
}timer_t;


/* Private constants -------------------------------------- */

#define NUM_TIMERS 				2

#define TIMER_INTERRUPT 		29

#define TIMER_PRESCALE_SHIFT  	8
#define TIMER_WD_MODE         	8
#define TIMER_IT_ENABLE       	(1 << 2)
#define TIMER_AUTO_RELOAD     	(1 << 1)
#define TIMER_ENABLE          	(1 << 0)
#define TIMER_INTERRUPT_CLEAR 	(1 << 0)


/* Private macros ----------------------------------------- */


/* Private variables -------------------------------------- */


/* Private function prototypes ---------------------------- */

extern vaddr_t BoardPrivateTimers();


/* Private functions -------------------------------------- */

void SystemTickStart(uint32_t usec, void* (*handler)(void*, uint32_t))
{
	SytemTimerInit(AUTO_RELOAD_TIMER, usec);
	InterruptAttach(SystemTimerIrq(), 10, handler, NULL);
}

void SytemTimerInit(uint32_t mode, uint32_t u_sec)
{
	timer_t *timer = (timer_t*)BoardPrivateTimers();

	uint32_t prescale = 0xFF;
	timer->pt_load_reg = 0;
	timer->pt_counter_reg = 0;
	timer->pt_interrupt_status_reg = TIMER_INTERRUPT_CLEAR;	// The event flag is cleared when written to 1.

	switch (mode)
	{
		case AUTO_RELOAD_TIMER :
			timer->pt_control_reg = (prescale << TIMER_PRESCALE_SHIFT) | TIMER_IT_ENABLE | TIMER_AUTO_RELOAD;
			timer->pt_load_reg = u_sec + 1;
			timer->pt_control_reg |= TIMER_ENABLE;
			break;

		case ONE_SHOT_TIMER :
			timer->pt_control_reg = (prescale << TIMER_PRESCALE_SHIFT) | TIMER_IT_ENABLE;
			timer->pt_load_reg = u_sec + 1;
			timer->pt_control_reg |= TIMER_ENABLE;
			break;

		case DISABLE_TIMER :
		default:
			timer->pt_control_reg &= ~TIMER_ENABLE;
	}
}

void *SytemTimerReset()
{
	timer_t *timer = (timer_t*)BoardPrivateTimers();

	timer->pt_interrupt_status_reg = TIMER_INTERRUPT_CLEAR;

	return NULL;
}

void SystemTimerHandler()
{
	(void)SytemTimerReset();
}

int32_t SystemTimerIrq()
{
	return TIMER_INTERRUPT;
}
