/**
 * @file        spinlock.S
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        11 September, 2016
 * @brief       Spin Lock functions
*/

.text
.align 2

/*
 * bit I set	-> Interrupts disable
 * bit I clear	-> Interrupts enable
 */

.global critical_lock
.func	critical_lock
// void critical_lock(uint32_t *state)
critical_lock:
	mrs		r1, CPSR		// read cpsr register
	and		r1, r1, #0x80	// get irq bit value
	str		r1, [r0]		// save irq bit value
	cpsid	i				// disable interrupts
	bx		lr
.endfunc

.global critical_unlock
.func	critical_unlock
// void critical_unlock(uint32_t *state)
critical_unlock:
	ldr		r0, [r0]		// get saved irq bit value
	and		r0, r0, #0x80	// ensure that we don't mess with other bits
	mrs		r1, CPSR		// read cpsr register
	bic		r1, r1, #0x80	// clear irq bit
	orr		r1, r1, r0		// restore irq bit
	msr		CPSR, r1		// update cpsr
	bx		lr
.endfunc

.global spinlock_init
.func	spinlock_init
// void spinlock_init(spinlock_t *lock)
spinlock_init:
	mov     r1, #0
	str		r1, [r0]
	bx		lr
.endfunc

.global spinlock
.func	spinlock
// void spinlock(spinlock_t *lock)
spinlock:
	mov     r2, #1
0:	ldrex   r1, [r0]
	cmp		r1, #0
	wfene
	strexeq	r1, r2, [r0]
	cmpeq	r1, #0
	bne		0b
	dmb
	bx		lr
.endfunc

.global spinunlock
.func	spinunlock
// void spinunlock(spinlock_t *lock)
spinunlock:
	mov     r1, #0
	str		r1, [r0]
	sev
	bx		lr
.endfunc

.global spinlock_irq
.func	spinlock_irq
// void spinlock_irq(spinlock_t *lock, uint32_t *state)
spinlock_irq:
	mov		r12, lr
	mov		r3, r1
	bl		spinlock
	mov		r0, r3
	bl		critical_lock
	bx		r12
.endfunc

.global spinunlock_irq
.func	spinunlock_irq
// void spinunlock_irq(spinlock_t *lock, uint32_t *state)
spinunlock_irq:
	mov		r12, lr
	mov		r3, r1
	bl		spinunlock
	mov		r0, r3
	bl		critical_unlock
	bx		r12
.endfunc
