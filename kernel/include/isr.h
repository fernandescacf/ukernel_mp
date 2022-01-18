/**
 * @file        isr.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        07 June, 2020
 * @brief		Kernel Interrupt Service Routine Handler header file
*/

#ifndef _ISR_H_
#define _ISR_H_

#ifdef __cplusplus
    extern "C" {
#endif


/* Includes ----------------------------------------------- */
#include <types.h>
#include <proctypes.h>

/* Exported constants ------------------------------------- */


/* Exported types ----------------------------------------- */

typedef struct
{
	struct
	{
		int32_t		irq;
		uint16_t	target;
		uint8_t		priority;
		uint8_t		enable;
	}interrupt;

	struct
	{
		int32_t		id;
		task_t		*task;
		const void	*arg;
		void		*(*handler)(void*, uint32_t);
		int16_t		set;
		int16_t		pending;
	}attach;

} isr_t;

/* Exported macros ---------------------------------------- */

#define RUNING_CPU					(0xFFFF)
#define INTERRUPT_INVALID			(-1)
#define SCHEDULER_IRQ				(0)

/* Exported functions ------------------------------------- */

int32_t InterruptHandlerInit();

int32_t InterruptAttach (int32_t intr, uint8_t priority, void *(*handler)(void *, uint32_t), const void *area);

int32_t InterrupDetach(int32_t id);

int32_t InterruptClean(task_t *task);

int32_t InterruptMask( int32_t intr, int32_t id );

int32_t InterruptUnmask( int32_t intr, int32_t id );

int32_t InterruptWait(int32_t id);

#ifdef __cplusplus
    }
#endif

#endif /* _ISR_H_ */
