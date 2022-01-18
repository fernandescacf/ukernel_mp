/**
 * @file        arch.S
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        March 25, 2020
 * @brief       Arm Architecture dependent functions implementation
 */


/* Includes ---------------------------------------------------------- */
#include <armv7.h>


/* Defines ----------------------------------------------------------- */
.set R0_offset,		(0)
.set R1_offset,		(4)
.set R2_offset,		(8)
.set R3_offset,		(12)
.set R4_offset,		(16)
.set R5_offset,		(20)
.set R6_offset,		(24)
.set R7_offset,		(28)
.set R8_offset,		(32)
.set R9_offset,		(36)
.set R10_offset,	(40)
.set R11_offset,	(44)
.set R12_offset,	(48)
.set SP_offset,		(52)
.set LR_offset,		(56)
.set PC_offset,		(60)
.set SPRS_offset,	(64)

.set SCU_CONFIG_OFFSET,     (0x04)
.set SCU_CPUNUM_MASK,       (0x03)

/* Macros --------------------------------------------------- */


/* Aligment -------------------------------------------------- */
.text
.align 2


/* Imported Functions ---------------------------------------- */



/* Functions ------------------------------------------------- */

// void _TaskContextSave(void* tcb, void* saved)
.global _TaskContextSave
.func	_TaskContextSave
_TaskContextSave:
    add   r2, r0, #R4_offset
    stm   r2, {r4-r11}^
    ldmia r1, {r3-r9}
    stm   r0, {r3-r6}
    add   r2, r0, #R12_offset
    stmia r2, {r7, sp, lr}^
    add   r2, r0, #PC_offset
    stm   r2, {r8, r9}
    bx    lr
.endfunc

// void _TaskContextRestore(void *tcb)
.global _TaskContextRestore
.func	_TaskContextRestore
_TaskContextRestore:
    cpsid   i, #SVC_MODE        // Ensure we are not in system mode (should be harmless)
    mov   lr, r0
    ldmia lr, {r0-lr}^
    add   lr, lr, #PC_offset
	clrex
    rfeia lr!
.endfunc


//void _TaskSetTls(void* tls)
.global _TaskSetTls
.func	_TaskSetTls
_TaskSetTls:
	mcr     p15, 0, r0, c13, c0, 2   // write TPIDRURW
	isb
	bx      lr
.endfunc

//void* _TaskGetTls(void)
.global _TaskGetTls
.func	_TaskGetTls
_TaskGetTls:
	mrc     p15, 0, r0, c13, c0, 2    // read TPIDRURW
	bx      lr
.endfunc

// void _TaskSave(void* tcb)
.global _TaskSave
.func	_TaskSave
_TaskSave:
	// Save registers: r4-r11 in the task TCB
	add		r12, r0, #R4_offset
	stmia	r12, {r4-r11}
	str		sp,[r0, #SP_offset]
	str		lr,[r0, #PC_offset]
	mrs     r12, cpsr
	str		r12,[r0, #SPRS_offset]
	bx		lr
.endfunc


// void _VirtualSpaceSet(void* tcb, pgt_t pgt, pid_t pid)
.global _VirtualSpaceSet
.func   _VirtualSpaceSet
_VirtualSpaceSet:
#ifdef ARM_FVP
	// Change User PGT (TTRB0)
    mcr    p15, 0, r1, c2, c0, 0
    dsb
    // In cortex A-9 asid doesn't seems to work :(
    mov		r3, #0x0
	mcr    p15, 0, r3, c8, c7, 0
#else
	// Enable Page table lookup in cache -> IRGN = 01, S = 1, NOS = 1
    orr		r1, r1, #0x62
    mov		r3, #0
    isb
    // Change ASID to 0
    mcr    p15, 0, r3, c13, c0, 1
    // Change User PGT (TTRB0)
    mcr    p15, 0, r1, c2, c0, 0
    // Change to actual ASID -> Avoids need of TLB invalidation
    mcr    p15, 0, r2, c13, c0, 1
    // Invalidaye Icache and Branch Predictor  <---UNDER INVESTIGATION
    mcr     p15, 0, r3, c7, c1, 0
    // Sync memory
    dsb
#endif

    isb

    cmp    r0, #0
    bne    _TaskContextRestore                  // Restore task context -> will not return
    bx     lr
.endfunc

// uint32_t _cpuId()
.global _cpuId
.func _cpuId
_cpuId:
    mrc    p15, 0, r0, c0, c0, 5
    and    r0, r0, #0x03
    bx      lr
.endfunc

// uint32_t _BoardGetCpus()
//.globl _BoardGetCpus
//.func _BoardGetCpus
//_BoardGetCpus:
//    mrc     p15, 4, r0, c15, c0, 0          // Read Configuration Base Address Register (Get Periph Base Address)
//    ldr     r0, [r0, #SCU_CONFIG_OFFSET]    // Read the SCU Configuration Register (Base Addr + 0x04 offset)
//    and     r0, r0, #SCU_CPUNUM_MASK        // Bits [1:0] gives the number of cores
//    bx      lr
//.endfunc

// void __TerminateRunningTask(process_t*, task_t*)
.globl __TerminateRunningTask
.func __TerminateRunningTask
__TerminateRunningTask:
    cpsid   i, #SVC_MODE                // Change to SVC mode user stack will be discarded (we have to disable interrupts because of the way we handle interrups)
    bl      ProcTaskClean
    mov		r0, #0
    bl      SchedTerminateRunningTask   // Will only select a new task to run and set the virtual space
    b       _TaskContextRestore         // Put next task to run
.endfunc

// void __TerminateRunningProcess(process_t*)
.globl __TerminateRunningProcess
.func __TerminateRunningProcess
__TerminateRunningProcess:
    cpsid   i, #SVC_MODE                // Change to SVC mode user stack will be discarded
    bl      ProcProcessSafeClean
    mov		r0, #1
    bl      SchedTerminateRunningTask   // Will only select a new task to run and set the virtual space
    b       _TaskContextRestore         // Put next task to run
.endfunc

// void _SchedulerStart(void* tcb, void* restore_ksp)
.globl _SchedulerStart
.func _SchedulerStart
_SchedulerStart:
    cps     #SVC_MODE               // Ensure we are in SVC mode
    mov     sp, r1                  // Restore kernel stack to its begining
    b       _TaskContextRestore
.endfunc

// void _SchedResumeTask(void* tcb, pgt_t pgt, pid_t pid)
.globl _SchedResumeTask
.func _SchedResumeTask
_SchedResumeTask:
     cpsid  i, #SVC_MODE                // Change to SVC mode
     push   {r0}
     mov    r0, #0
     cmp    r1, #0
     blne   _VirtualSpaceSet
     mov    r0, #0
     bl     SchedUnlock
     pop    {r0}
     b      _TaskContextRestore
.endfunc

.global _IdleTask
.func	_IdleTask
// void *_IdleTask(void *arg)
_IdleTask:
1:	wfi
	b		1b
.endfunc
