/**
 * @file        timer.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        18 December, 2018
 * @brief		Sunxi Systimer implementation
*/

/* Includes ----------------------------------------------- */
#include <systimer.h>
#include <board.h>
#include <isr.h>


/* Private types ------------------------------------------ */

typedef struct {
	volatile uint32_t irqen;				// 0x00 - Timer IRQ Enable Register
	volatile uint32_t irqsta;				// 0x04 - Timer Status Register
	volatile uint32_t reserved[2];
	struct{
		volatile uint32_t ctrl;				// Timer Control Register
		volatile uint32_t intv;				// Timer Interval Value Register
		volatile uint32_t cur;				// Timer 0 Current Value Register
		volatile uint32_t reserved;
	}timer[2];								// Timer 0 & Timer 1
}h3_timer_t;


/* Private constants -------------------------------------- */

#define NUM_TIMERS 				2
#define TIMER_0					0
#define TIMER_1					1

#define TIMER0_IRQ      		50
#define TIMER1_IRQ      		51

#define	CTRL_ENABLE				(0x1 << 0)
#define	CTRL_RELOAD				(0x1 << 1)
#define	CTRL_SRC_32K			(0x0 << 2)
#define	CTRL_SRC_24M			(0x1 << 2)

#define	CTRL_PRE_1				(0x0 << 4)
#define	CTRL_PRE_2				(0x1 << 4)
#define	CTRL_PRE_4				(0x2 << 4)
#define	CTRL_PRE_8				(0x3 << 4)
#define	CTRL_PRE_16				(0x4 << 4)
#define	CTRL_PRE_32				(0x5 << 4)
#define	CTRL_PRE_64				(0x6 << 4)
#define	CTRL_PRE_128			(0x7 << 4)

#define	CTRL_AUTO				(0x0 << 7)
#define	CTRL_SINGLE				(0x1 << 7)

#define CLOCK_24M				24000000

#define TIMER_HZ_VALUE(hz)		(CLOCK_24M/hz)
#define TIMER_USEC_VALUE(usec)	(24 * (usec))
#define TIMER_AUTO_MODE			(0)
#define TIMER_SINGLESHOT_MODE	(1)

#define SYSTIMER				TIMER_0
#define SYSTIMER_IRQ			TIMER0_IRQ

/* Private macros ----------------------------------------- */


/* Private variables -------------------------------------- */
static h3_timer_t* h3Timers = NULL;


/* Private function prototypes ---------------------------- */

extern vaddr_t BoardPrivateTimers();

int32_t TimerInit(uint32_t timerId, uint32_t loadValue, uint32_t config);

void TimerInterruptEnable(uint32_t timerId);

void TimerInterruptAck(uint32_t timerId);

/* Private functions -------------------------------------- */

void SystemTickStart(uint32_t usec, void* (*handler)(void*, uint32_t))
{
	SytemTimerInit(AUTO_RELOAD_TIMER, usec);
	InterruptAttach(SystemTimerIrq(), 10, handler, NULL);
}

void SytemTimerInit(uint32_t mode, uint32_t u_sec)
{
	uint32_t loadValue = TIMER_USEC_VALUE(u_sec);

	switch (mode)
	{
		case AUTO_RELOAD_TIMER :
			(void)TimerInit(SYSTIMER, loadValue, TIMER_AUTO_MODE);
			break;

		case ONE_SHOT_TIMER :
			(void)TimerInit(SYSTIMER, loadValue, TIMER_SINGLESHOT_MODE);
			break;

		case DISABLE_TIMER :
		default:
			break;
	}

	TimerInterruptEnable(SYSTIMER);
}

void *SytemTimerReset()
{
	TimerInterruptAck(SYSTIMER);

	return NULL;
}

void SystemTimerHandler()
{
	(void)SytemTimerReset();
}

int32_t SystemTimerIrq()
{
	return SYSTIMER_IRQ;
}

int32_t TimerInit(uint32_t timerId, uint32_t loadValue, uint32_t config)
{
	if(h3Timers == NULL)
	{
		h3Timers = (h3_timer_t *)BoardPrivateTimers();
	}

	h3Timers->timer[timerId].intv = loadValue;
	h3Timers->timer[timerId].ctrl = 0x0;

	// By default disable the interrupt
	h3Timers->irqen &= (~(1 << timerId));

	if(TIMER_SINGLESHOT_MODE == config)
	{
		h3Timers->timer[timerId].ctrl |= CTRL_SINGLE;
	}

	h3Timers->timer[timerId].ctrl |= CTRL_SRC_24M;
	h3Timers->timer[timerId].ctrl |= CTRL_RELOAD;

	while (h3Timers->timer[timerId].ctrl & CTRL_RELOAD){}

	h3Timers->timer[timerId].ctrl |= CTRL_ENABLE;

	return 0;
}

void TimerInterruptEnable(uint32_t timerId)
{
	h3Timers->irqen |= (1 << timerId);
}

void TimerInterruptAck(uint32_t timerId)
{
	h3Timers->irqsta |= (1 << timerId);
}
