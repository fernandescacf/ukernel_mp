/**
 * @file        atomic.S
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        9 August, 2016
 * @brief       Atomic operations functions
*/

.text
.align 2

.global atomic_inc
.func	atomic_inc
// int32_t atomic_inc(int32_t *data)
atomic_inc:
0:	ldrex   r1, [r0]
	add		r1, r1, #1
	strex	r2, r1, [r0]
	cmp		r2, #1
	beq		0b
	mov		r0, r1
	bx		lr
.endfunc

.global atomic_dec
.func	atomic_dec
// int32_t atomic_dec(int32_t *data)
atomic_dec:
0:	ldrex   r1, [r0]
	sub		r1, r1, #1
	strex	r2, r1, [r0]
	cmp		r2, #1
	beq		0b
	mov		r0, r1
	bx		lr
.endfunc

.global atomic_cmp_set
.func	atomic_cmp_set
// int32_t atomic_cmp_set(int32_t *dst, int32_t cmp, int32_t value)
atomic_cmp_set:
	push	{r4}
0:	ldrex   r3, [r0]
	cmp		r1, r3
	strexeq	r12, r2, [r0]
	moveq	r4, #0
	strexne	r12, r3, [r0]
	movne	r4, #1
	cmp		r12, #1
	beq		0b
	mov		r0, r4
	pop		{r4}
	bx			lr
.endfunc

.global atomic_exchange
.func	atomic_exchange
// void atomic_exchange(int32_t *p1, int32_t *p2)
atomic_exchange:
	ldr		r3, [r1]
0:	ldrex	r2, [r0]
	strex	r12, r3, [r0]
	cmp		r12, #1
	beq		0b
	str		r2, [r1]
	bx		lr
.endfunc
