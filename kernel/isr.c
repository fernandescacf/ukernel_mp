/**
 * @file        irq.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        06 October, 2018
 * @brief       Kernel Interrupt handler implementation
*/

/* Includes ----------------------------------------------- */
#include <isr.h>
#include <process.h>
#include <kheap.h>
#include <zone.h>
#include <vmap.h>
#include <glist.h>
#include <string.h>
#include <memmgr.h>

#include <interrupt.h>
#include <board.h>
#include <rfs.h>
#include <arch.h>

#include <scheduler.h>

#include <systimer.h>


/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */



/* Private macros ----------------------------------------- */

#define INTERRUP_ID(task, irq)		((task->parent->pid << 16) | intr)
#define INTERRUP_IRQ(id)			(id & 0xFFFF)

#define INTRERRUP_AVAILABLE			(0X0)
#define INTRERRUP_RESERVED			(0xFFFFFFFF)

/* Private variables -------------------------------------- */

struct
{
	uint32_t	private;
	isr_t		**privQueue;
	uint32_t	shared;
	isr_t		**sharedQueue;
}interruptHandler;;

/* Private function prototypes ---------------------------- */

int32_t InterruptReserve(int32_t intr, int32_t cpu)
{
//	(void)cpu;

	if(intr < interruptHandler.private)
	{
		int32_t privIntr = (intr + (interruptHandler.private * cpu));

		if(interruptHandler.privQueue[privIntr] == (isr_t*)(INTRERRUP_AVAILABLE))
		{
			interruptHandler.privQueue[privIntr] = (isr_t*)(INTRERRUP_RESERVED);
			return E_OK;
		}

		return E_BUSY;
	}
	else if(intr < interruptHandler.shared)
	{
		if(interruptHandler.sharedQueue[intr - interruptHandler.private] == (isr_t*)(INTRERRUP_AVAILABLE))
		{
			interruptHandler.sharedQueue[intr - interruptHandler.private] = (isr_t*)(INTRERRUP_RESERVED);
			return E_OK;
		}

		return E_BUSY;
	}
	else
	{
		return E_INVAL;
	}
}

int32_t InterruptRelease(int32_t intr, int32_t cpu)
{
//	(void)cpu;
	if(intr < interruptHandler.private)
	{
		int32_t privIntr = (intr + (interruptHandler.private * cpu));

		interruptHandler.privQueue[privIntr] = (isr_t*)(INTRERRUP_AVAILABLE);

	}
	else
	{
		interruptHandler.sharedQueue[intr - interruptHandler.private] = (isr_t*)(INTRERRUP_AVAILABLE);
	}

	return E_OK;
}

isr_t * InterruptGet(int32_t intr, int32_t cpu)
{
//	(void)cpu;
	if(intr < interruptHandler.private)
	{
		int32_t privIntr = (intr + (interruptHandler.private * cpu));

		return interruptHandler.privQueue[privIntr];

	}
	else
	{
		return interruptHandler.sharedQueue[intr - interruptHandler.private];
	}
}

int32_t InterruptRegister(isr_t *isr)
{
	if(isr->interrupt.irq < interruptHandler.private)
	{
		int32_t privIntr = (isr->interrupt.irq + (interruptHandler.private * isr->interrupt.target));

		interruptHandler.privQueue[privIntr] = isr;
	}
	else
	{
		interruptHandler.sharedQueue[isr->interrupt.irq - interruptHandler.private] = isr;
	}
	return E_OK;
}

/* Private functions -------------------------------------- */

int32_t InterruptHandlerInit()
{
	// Get Interrupts information from RFS
	RfsGetInterruptInfo(&interruptHandler.private, &interruptHandler.shared);

	interruptHandler.privQueue = (isr_t**)kmalloc(sizeof(isr_t*) * interruptHandler.private * BoardGetCpus());
	// TODO: For now we put all as supported (0x0) in future only the supported are set to 0x0 not supported are set to 0x1
	memset(interruptHandler.privQueue, 0x0, interruptHandler.private * sizeof(isr_t*) * BoardGetCpus());

	interruptHandler.sharedQueue = (isr_t**)kmalloc(sizeof(isr_t*) * interruptHandler.shared);
	// TODO: For now we put all as supported (0x0) in future only the supported are set to 0x0 not supported are set to 0x1
	memset(interruptHandler.sharedQueue, 0x0, interruptHandler.shared  * sizeof(isr_t*));

	return E_OK;
}

void UserInterruptHandle(uint32_t irq, uint32_t source)
{
	// Get ISR for the interrupt
	isr_t* isr = InterruptGet(irq, (uint16_t)source);

	if(isr == NULL)
	{
		return;
	}

	if(isr->attach.handler != NULL)
	{
		bool_t restore = FALSE; // Change running process and test and them restore it???

		// Switch context if necessary
		if(isr->attach.task != NULL && isr->attach.task->parent != SchedGetRunningProcess())
		{
			restore = TRUE;

			_VirtualSpaceSet(
				NULL,
				(pgt_t)MemoryL2P((ptr_t)isr->attach.task->parent->Memory.pgt),
				isr->attach.task->parent->pid
			);
		}

		// Call Interrupt Dispatcher
		InterruptDispache(irq, source, isr->attach.handler, (void*)isr->attach.arg);

		if(/*restore && */SchedGetRunningProcess() != NULL)
		{
			_VirtualSpaceSet(
				NULL,
				(pgt_t)MemoryL2P((ptr_t)SchedGetRunningProcess()->Memory.pgt),
				SchedGetRunningProcess()->pid
			);
		}

		// Unlock tasks waiting on this interrupt
		if((NULL != isr->attach.task) && (isr->attach.pending == TRUE))
		{
			isr->attach.pending = FALSE;
			SchedAddTask(isr->attach.task);
		}
		else
		{
			// Set interrupt as received
			isr->attach.set = TRUE;
		}
	}
}

void* InterruptHandler(uint32_t irqinfo)
{
	uint32_t irq;
	uint32_t source;
	InterruptDecode(irqinfo, &irq, &source);

//    switch(irq)
//    {
//    case SCHEDULER_IRQ:
//        Schedule();
//        break;
//    case 1:
//    	break;
//    default:
        UserInterruptHandle(irq, source);   // Will also handle System tick interrupt
//        break;
//    }

    // Signal end of interruption service
    InterruptEnd(irq);

    //return SchedIrqExit();
    return NULL;
}


int32_t InterruptAttach (int32_t intr, uint8_t priority, void *(*handler)(void *, uint32_t), const void *area)
{
	task_t *task = SchedGetRunningTask();

	// A task can only have one interrupt attached
	if((task != NULL) && (task->interrupt.id != INTERRUPT_INVALID))
	{
		return E_FAULT;
	}

	int32_t ret = InterruptReserve(intr, RUNNING_CPU);

	if(ret != E_OK)
	{
		return ret;
	}

	isr_t *isr = (isr_t*)kmalloc(sizeof(isr_t));

	if(NULL == isr)
	{
		InterruptRelease(intr, RUNNING_CPU);

		return E_ERROR;
	}

	if(task != NULL)
	{
		isr->attach.id = INTERRUP_ID(task, intr);

	}
	else
	{
		isr->attach.id = intr;
	}

	isr->interrupt.irq = intr;
	isr->interrupt.target = RUNNING_CPU;
	isr->interrupt.priority = priority;
	isr->interrupt.enable = TRUE;

	isr->attach.task = task;
	isr->attach.arg = area;
	isr->attach.handler = handler;
	isr->attach.set = FALSE;
	isr->attach.pending = FALSE;

	InterruptRegister(isr);

	// Set task interrupt data
	if(task != NULL)
	{
		task->interrupt.id  = isr->attach.id;
		task->interrupt.irq = isr->interrupt.irq;
	}

	InterruptSetTarget(isr->interrupt.irq, isr->interrupt.target, TRUE);
	InterruptSetPriority(isr->interrupt.irq, (uint32_t)isr->interrupt.priority);
	InterruptEnable(isr->interrupt.irq);

	return isr->attach.id;
}

int32_t InterruptClean(task_t *task)
{
	if(task->interrupt.id == INTERRUPT_INVALID)
	{
		return E_OK;
	}

	//TODO: we need to know the cpu where the interrupt is assigned
	isr_t* isr = InterruptGet(task->interrupt.irq, RUNNING_CPU);

	// Disable the interrupt
	InterruptDisable(isr->interrupt.irq);
	InterruptSetTarget(isr->interrupt.irq, isr->interrupt.target, FALSE);

	// Remove handler for the interrupt
	InterruptRelease(isr->interrupt.irq, isr->interrupt.target);

	// Clean task interrupt data
	task->interrupt.id  = INTERRUPT_INVALID;
	task->interrupt.irq = INTERRUPT_INVALID;

	// Clean interrupt structure
	kfree(isr, sizeof(*isr));

	return E_OK;

}

int32_t InterrupDetach(int32_t id)
{
	task_t *task = SchedGetRunningTask();

	if(task->interrupt.id != id)
	{
		return E_INVAL;
	}

	return InterruptClean(task);
}

int32_t InterruptMask( int32_t intr, int32_t id )
{
	task_t *task = SchedGetRunningTask();

	if((task->interrupt.id != id) || (task->interrupt.irq != intr))
	{
		return E_INVAL;
	}

	InterruptDisable(task->interrupt.irq);

	//TODO: we need to know the cpu where the interrupt is assigned
	isr_t *isr = InterruptGet(task->interrupt.irq, RUNNING_CPU);

	isr->interrupt.enable = FALSE;
	isr->attach.set = FALSE;

	return E_OK;

}

int32_t InterruptUnmask( int32_t intr, int32_t id )

{
	task_t *task = SchedGetRunningTask();

	if((task->interrupt.id != id) || (task->interrupt.irq != intr))
	{
		return E_INVAL;
	}

	//TODO: we need to know the cpu where the interrupt is assigned
	isr_t *isr = InterruptGet(task->interrupt.irq, RUNNING_CPU);

	isr->interrupt.enable = TRUE;

	InterruptEnable(task->interrupt.irq);

	return E_OK;

}

int32_t InterruptWait(int32_t id)
{
	task_t *task = SchedGetRunningTask();

	if(task->interrupt.id != id)
	{
		return E_INVAL;
	}

	//TODO: we need to know the cpu where the interrupt is assigned
	isr_t *isr = InterruptGet(task->interrupt.irq, RUNNING_CPU);

	if(isr->interrupt.enable != TRUE)
	{
		return E_FAULT;
	}

	if(isr->attach.set == TRUE)
	{
		isr->attach.set = FALSE;
		return E_OK;
	}

	isr->attach.pending = TRUE;
	SchedStopRunningTask(BLOCKED, INTERRUPT_PENDING);

	return E_OK;
}

