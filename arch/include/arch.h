/**
 * @file        arch.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        18 February, 2020
 * @brief       Architecture dependent functions header file
*/

#ifndef _ARCH_H_
#define _ARCH_H_

#ifdef __cplusplus
    extern "C" {
#endif

/* Includes ----------------------------------------------- */
#include <types.h>
#include <mmu.h>


/* Exported types ----------------------------------------- */


/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */
#define RUNNING_CPU				_cpuId()



/* Exported functions ------------------------------------- */

/* @brief	Architecture specific function to allocate memory to store
 * 			the processor registers state
 *
 * @param	No Parameters
 *
 * @retval	Returns a a valid pointer or NULL in case of an error
 */
void *_TaskAllocTCB();

void _TaskDeallocTCB(void* tcb);

/* @brief	Architecture specific function to set the task mode as privileged
 *
 * @param	tcb		- Pointer to the memory containing the registers stored stated
 *
 * @retval	No return
 */
void _TaskSetPriligeMode(void *tcb);


/* @brief	Architecture specific function to set the task to user mode
 *
 * @param	tcb		- Pointer to the memory containing the registers stored stated
 *
 * @retval	No return
 */
void _TaskSetUserMode(void *tcb);


/* @brief	Architecture specific function to set up to 3 registers that
 * 			are used to pass parameters
 *
 * @param	tcb		- Pointer to the memory containing the registers stored stated
 * 			param0	- First parameter
 * 			param1	- Second parameter
 * 			param2	- Third parameter
 *
 * @retval	No return
 */
void _TaskSetParameters(void *tcb, void *param0, void *param1, void *param2);


/* @brief	Architecture specific function to set up to the program counter register
 * 			to the task entry point
 *
 * @param	tcb		- Pointer to the memory containing the registers stored stated
 * 			entry	- Task entry point
 *
 * @retval	No return
 */
void _TaskSetEntry(void *tcb, vaddr_t entry);


/* @brief	Architecture specific function to set up to the "return" register
 * 			to the task exit point
 *
 * @param	tcb		- Pointer to the memory containing the registers stored stated
 * 			exit	- Task exit point
 *
 * @retval	No return
 */
void _TaskSetExit(void *tcb, vaddr_t exit);


/* @brief	Architecture specific function to set the stack pointer register
 *
 * @param	tcb		- Pointer to the memory containing the registers stored stated
 * 			sp		- Stack top address
 *
 * @retval	No return
 */
void  _TaskSetSP(void *tcb, vaddr_t sp);


/* @brief	Architecture specific function to save the running task current state
 *
 * @param	tcb		- Pointer to the memory containing the registers stored stated
 *
 * @retval	No return
 */
void _TaskContextSave(void *tcb);


/* @brief	Architecture specific function to restore the task state
 *
 * @param	tcb		- Pointer to the memory containing the registers stored stated
 *
 * @retval	No return
 */
void _TaskContextRestore(void *tcb);

/* @brief	Architecture specific function to set the task local storage
 *
 * @param	tls		- Pointer to the task local storage
 *
 * @retval	No return
 */
void _TaskSetTls(void* tls);


void _TaskSave(void* tcb);


/* @brief	Architecture specific function initialize a process virtual memory space
 *
 * @param	pgt		- Process page table
 * 			asid	- Process Memory Id
 *
 * @retval	No return
 */
void _VirtualSpaceSet(void* tcb, pgt_t pgt, pid_t pid);


void _TerminateRunningTask();

void _TerminateRunningProcess();

uint32_t _cpuId();

void* _BoardGetBaseStack();

void _cpu_hold();

void _cpus_signal();

void cpu_boot_finish();

void cpus_set_stacks(void* stacks);

void _SchedulerStart(void* tcb, void* restore_ksp);

void _SchedResumeTask(void* tcb, pgt_t pgt, pid_t pid);

void *_IdleTask(void *arg);

#ifdef __cplusplus
    }
#endif

#endif /* _ARCH_H_ */
