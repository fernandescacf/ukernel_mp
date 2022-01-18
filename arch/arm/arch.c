/**
 * @file        arch.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        18 February, 2020
 * @brief       Arm Architecture dependent functions implementation
*/

/* Includes ----------------------------------------------- */
#include <arch.h>
#include <kheap.h>
#include <string.h>
#include <scheduler.h>


/* Private constants -------------------------------------- */

#define ARMV7_REGISTERS_SIZE	(sizeof(unsigned long) * 17)

/* Private types ------------------------------------------ */


/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */



/* Private functions -------------------------------------- */

/**
 * _TaskAllocTCB Implementation (See header file for description)
*/
void *_TaskAllocTCB()
{
	void *tcb = kmalloc(ARMV7_REGISTERS_SIZE);

	if(tcb)
	{
		memset(tcb, 0x0, ARMV7_REGISTERS_SIZE);

		// Initialize CPSR
		asm volatile (
				"ldr	r0, =0x00000050			\n\t"		// user mode
				"str	r0, [%[_tcb], #64]		\n\t"		// set SPRS register
				: : [_tcb] "r" (tcb) : "r0"
			);
	}

	return tcb;
}

void _TaskDeallocTCB(void* tcb)
{
	kfree(tcb, ARMV7_REGISTERS_SIZE);
}

/**
 * _TaskSetPriligeMode Implementation (See header file for description)
*/
void _TaskSetPriligeMode(void *tcb)
{
	asm volatile (
			"ldr	r0, =0x0000005F			\n\t"		// system mode -> Privileged mode
			"str	r0, [%[_tcb], #64]		\n\t"		// set SPRS register
			: : [_tcb] "r" (tcb) : "r0"
		);
}

/**
 * _TaskSetUserMode Implementation (See header file for description)
*/
void _TaskSetUserMode(void *tcb)
{
	asm volatile (
			"ldr    r0, [%[_tcb], #64]      \n\t"
			"bic	r0, r0, #0x0F			\n\t"		// system mode -> Privileged mode
			"str	r0, [%[_tcb], #64]		\n\t"		// set SPRS register
			: : [_tcb] "r" (tcb) : "r0"
		);
}

/**
 * _TaskSetParameters Implementation (See header file for description)
*/
void _TaskSetParameters(void *tcb, void *param0, void *param1, void *param2)
{
	asm volatile (
			"str	%[param0], [%[_tcb], #0]	\n\t"
			"str	%[param1], [%[_tcb], #4]	\n\t"
			"str	%[param2], [%[_tcb], #8] \n\t"
			: : [_tcb] "r" (tcb), [param0] "r" (param0), [param1] "r" (param1), [param2] "r" (param2)
		);
}

/**
 * _TaskSetEntry Implementation (See header file for description)
*/
void _TaskSetEntry(void *tcb, vaddr_t entry)
{
	asm volatile (
			"str	%[_entry], [%[_tcb], #60]	\n\t"
			: : [_tcb] "r" (tcb), [_entry] "r" (entry)
		);
}

/**
 * _TaskSetExit Implementation (See header file for description)
*/
void _TaskSetExit(void *tcb, vaddr_t exit)
{
	asm volatile ("str	%[_exit], [%[_tcb], #56]" : : [_tcb] "r" (tcb), [_exit] "r" (exit));
}

/**
 * _TaskSetExit Implementation (See header file for description)
*/
void  _TaskSetSP(void *tcb, vaddr_t sp)
{
	asm volatile ("str	%[_sp], [%[_tcb], #52]" : : [_tcb] "r" (tcb), [_sp] "r" (sp));
}

/**
 * _VirtualSpaceSet Implementation (See header file for description)
*/
/*
void _VirtualSpaceSet(void* tcb, pgt_t pgt, pid_t pid)
{
	asm volatile (
			"mcr	p15, 0, %[_pgt], c2, c0, 0		\n\t"		// Change TTRB0 -> MMU Page table 0
			"dsb									\n\t"
			"mcr	p15, 0, %[_pid], c13, c0, 1		\n\t"		// Change ID -> Avoids TLB invalidation
			"isb									\n\t"
			"cmp	%[_tcb], #0						\n\t"
			"bne	_TaskContextRestore				\n\t"		// Restore task context -> will not return
			: : [_tcb] "r" (tcb), [_pgt] "r" (pgt), [_pid] "r" (pid)
		);
}
*/

void _TerminateRunningTask()
{
	extern void __TerminateRunningTask(process_t*, task_t*);

	__TerminateRunningTask(SchedGetRunningProcess(), SchedGetRunningTask());
}

void _TerminateRunningProcess()
{
	extern void __TerminateRunningProcess(process_t*);

	__TerminateRunningProcess(SchedGetRunningProcess());
}

void* _BoardGetBaseStack()
{
	extern void* __kernel_stack;
	return (void*) &__kernel_stack;
}

void _cpu_hold()
{
	asm volatile ("wfe");
}

void _cpus_signal()
{
	asm volatile ("dsb");
	asm volatile ("sev");
}
