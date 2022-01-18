/**
 * @file        entry.S
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        March 25, 2020
 * @brief       Arm exceptions entry handlers implementation
 */


/* Includes ---------------------------------------------------------- */
#include <armv7.h>


/* Defines ----------------------------------------------------------- */


/* Macros --------------------------------------------------- */


/* Aligment -------------------------------------------------- */
.text
.align 2


/* Imported Functions ---------------------------------------- */



/* Functions ------------------------------------------------- */

// SVC exception Handler: Used to attend system calls
.global svc_raw_handler
svc_raw_handler:
	/* Store svc_lr and svc_spsr in the user stack*/
	SRSDB	#SYS_MODE!
	/* Change to system mode */
	cpsid	i, #SYS_MODE
	/* Save user_lr */
	push	{r6, lr}
	/* Enable Interrupts */
	cpsie	i
	/* Put the 2 last arguments in the stack */
	stmdb	sp!, {r4,r5}
	/* Call system call */
	ldr		r6, =systable
	add		r6, r6, r7, lsl #2
	ldr		r7, [r6]
	cmp		r7, #0
	blxne	r7
	/* Disable interrupts */
	cpsid	i
	/* Adjust stack */
	add	sp, sp, #8
	/* Restore user_lr */
	pop	{r6, lr}
	/* Return to user space */
	rfefd   sp!

.global irq_raw_handler
irq_raw_handler:
    sub     lr, lr, #4               // Construct the return address
    srsdb   sp!, #SVC_MODE           // Save LR_irq and SPSR_irq in Kernel (SVC) stack
    cps     #SVC_MODE                // Switch to svc mode
    push    {r0-r3, r12}             // Save AAPCS registers
    push    {lr}                     // Save SVC lr in case we were running in SVC mode
    bl      SchedIrqAttend           // Signal Interrupt handling to kernel: r0 -> NULL or Task TCB
    pop     {lr}                     // Restore SVC lr
    cmp     r0, #0
    movne   r1, sp
    addne   sp, sp, #28              // set sp to begining of saved registers
    blne    _TaskContextSave         // _TaskContextSave(tcb, saved)
    and     r1, sp, #4               // Test alignment of the stack
    sub     sp, sp, r1               // Remove any misalignment (0 or 4)
    push    {r1, lr}                 // store the adjustment and lr (from interrupted mode)
    bl      InterruptAck             // Acknowledge interrupt and get irq info
    cpsie   i                        // Enable interrupts
    bl      InterruptHandler         // Call kernel handler
    cpsid   i
    bl      SchedIrqExit
    pop     {r1, lr}
    add     sp, sp, r1
    cmp     r0, #0                   // Do we want to restore a task context
    bne     _TaskContextRestore
    pop     {r0-r3, r12}
    rfeia   sp!



// FIQ exception Handler: Not supported
.global fiq_raw_handler
fiq_raw_handler:
	b		.


.global abort_raw_handler
abort_raw_handler:
	cpsid	i
	push	{r0 - r12}
	srsdb   sp!, #ABT_MODE
	cps		#SYS_MODE
	mov		r0, sp
	mov		r1, lr
	cps		#ABT_MODE
	push	{r0}
	push	{r1}
	mrc     p15, 0, r0, c5, c0, 0    // Get Cause of the abort: Read Data Fault Status Register
    mrc     p15, 0, r1, c6, c0, 0    // Read Data Fault Address Register
    mov		r2, sp
	bl		AbortDataHandler
1:  wfi
    b       1b

/*
    sub     lr, lr, #4               // Construct the return address
    srsdb   sp!, #SVC_MODE           // Save LR_abt and SPSR_abt in Kernel (SVC) stack
    cps     #SVC_MODE                // Switch to svc mode

    push    {r0-r3, r12}             // Save AAPCS registers
    bl		SchedGetRunningTaskTcb   // Get running task TCB

    mov     r1, sp
    add     sp, sp, #28              // set sp to begining of saved registers
    bl      _TaskContextSave         // _TaskContextSave(tcb, saved)

    mrc     p15, 0, r0, c5, c0, 0    // Get Cause of the abort: Read Data Fault Status Register
    mrc     p15, 0, r1, c6, c0, 0    // Read Data Fault Address Register

    cpsie	i						 // Enable interruptions
    bl		AbortDataHandler		 // Call high level Abort Handler
    cpsid	i						 // Disable interruptions for the context restore

    cmp		r0, #0
    bne		_TaskContextRestore		 // We were able to recover so resume task

    bl		SchedGetRunningProcess   // Get task parent process to terminate it
    cpsie	i						 // Enable interruptions to prevent blocking the running core
    bl		ProcProcessTerminate	 // We should not return since a new task will be resumed

1:  wfi								 // Just in case
    b       1b
*/

/*
    // Ensure interruption are disabled
    cpsid	i
    // Set abort stack
    ldr		sp, =__kernel_stack
    // Save r0
    push    {r0}
    // Get Cause of the abort
    mrc     p15, 0, r0, c5, c0, 0               // Read Data Fault Status Register
    // Check if cause was a data write/read
    tst     r0, #(0x1 << 11)
    moveq   r0, #0
    beq     1f
    // Cause was a data access we migth be able to recover
    sub     lr, lr, #4                          // abt_lr = pc - 8
    // Save r0-r3, r12, lr registers
    pop		{r0}
    push    {r0, r1, r2, r3, r12, lr}
    mrs    	r0, spsr
    push	{r0}
    // Get running task TCB
    bl		RunningTaskTcb
    // Save running task context
    bl		_TaskContextSave
    // Call kernel data abort handler
    mrc     p15, 0, r0, c6, c0, 0               // Read Data Fault Address Register
1:  bl      AbortDataHandler
    // If we return we were able to solve the data abort so we fo back to the task execution
    b       _TaskContextRestore
*/

.global undef_raw_handler
undef_raw_handler:
	cpsid	i
	mov		r0, #2
	mov		r1, lr
	cps		SYS_MODE
	mov		r2, sp
	mov		r3, lr
	cps		SVC_MODE
	bl		FaultDumper
1:  wfi
    b       1b


.global prefetch_raw_handler
prefetch_raw_handler:
	cpsid	i
	mov		r0, #1
	mov		r1, lr
	cps		SYS_MODE
	mov		r2, sp
	mov		r3, lr
	cps		SVC_MODE
	bl		FaultDumper
1:  wfi
    b       1b
