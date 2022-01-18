/**
 * @file        string.S
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        December 20, 2019
 * @brief       String functions implementation
 */


/* Includes ---------------------------------------------------------- */


/* Defines ----------------------------------------------------------- */


/* Macros --------------------------------------------------- */


/* Aligment -------------------------------------------------- */
.text
.align 2


/* Imported Functions ---------------------------------------- */



/* Function -------------------------------------------------- */

.global memset
.func	memset
// void *memset(void *s, int c, size_t n)
memset:
	mov		r12, r0
	cmp		r2, #0x8
	blt		3f

	ands	r3, r0, #0x3
	beq		0f
	rsbs	r3, r3, #4
	sub		r2, r2, r3
	cmp		r3, #0x2
	strgtb	r1, [r12], #0x1
	strgeb	r1, [r12], #0x1
	strb	r1, [r12], #0x1

0:	orr		r1, r1, r1, lsl #8
	orr		r1, r1, r1, lsl #16
	cmp		r2, #0x10
	blt		2f

	push	{r4-r6}
	mov		r3, r1
	mov		r4, r1
	mov		r5, r1
	mov		r6, r1
0:	stmia	r12!,{r3-r6}
	sub		r2, r2, #0x10
	cmp		r2, #0x10
	bge		0b
	pop		{r4-r6}
	b		2f

1:	str		r1, [r12], #0x4
	sub		r2, r2, #0x4
2:	cmp		r2, #0x4
	bge		1b

3:	teq		r2, #0x0
	strneb	r1, [r12], #1
	subnes	r2, r2, #0x1
	bne		3b
	bx		lr
.endfunc

.global memcpy
.func	memcpy
// void *memcpy(void *dst, const void *src, size_t len)
memcpy:
	mov		r3, r0
	push	{r4-r7}
	cmp		r2, #0x10
	blt		3f
	// Align dst
	ands	r12, r0, #0x3
	beq		0f
	rsbs	r12, r12, #4
	sub		r2, r2, r12
	cmp		r12, #0x2
	ldrgtb	r4, [r1], #0x1
	ldrgeb	r5, [r1], #0x1
	ldrb	r6, [r1], #0x1
	strgtb	r4, [r3], #0x1
	strgeb	r5, [r3], #0x1
	strb	r6, [r3], #0x1
	// Check if src is aligned
0:	ands	r12, r1, #0x3
	bne		3f
	// Try copy 16 bytes chunks
0:	cmp		r2, #0x10
	blt		1f
	ldmia	r1!, {r4-r7}
	stmia	r3!, {r4-r7}
	sub		r2, r2, #0x10
	b		0b
	// Now we have < 16 bytes left
1:	tst		r2, #0x8
	beq		2f
	ldmia	r1!, {r4-r5}
	sub		r2, r2, #0x8
	stmia	r3!, {r4-r5}
2:	tst		r2, #0x4
	ldrne	r4, [r1], #0x4
	subne	r2, r2, #0x4
	strne	r4, [r3], #0x4
	// Copy bytes left
3:	teq		r2, #0x0
	ldrneb	r4, [r1], #1
	strneb	r4, [r3], #1
	subnes	r2, r2, #0x1
	bne		3b
	pop		{r4-r7}
	bx		lr
.endfunc

.global memcmp
.func	memcmp
// int32_t memcmp(const void *s1, const void *s2, size_t n)
memcmp:
0:	cmp		r2, #0x0
	moveq	r0, #0x0
	moveq	pc, lr
	ldrb	r3, [r0], #0x1
	ldrb	r12, [r1], #0x1
	sub		r2, r2, #0x1
	cmp		r3, r12
	movlt	r0, #-1
	movgt	r0, #1
	beq		0b
	bx		lr
.endfunc

.global strlen
.func	strlen
// uint32_t strlen(const void *str)
strlen:
	mov		r2, #0x0
0:	ldrb	r1, [r0], #0x1
	cmp		r1, #0x0
	addne	r2, r2, #0x1
	moveq	r0, r2
	bxeq	lr
	b		0b

.endfunc

.global strcmp
.func	strcmp
// int32_t strcmp(const void *s1, const void *s2)
/*
strcmp:
	push	{lr}
	push	{r0-r1}
	bl		strlen
	mov		r2, r0
	pop		{r0-r1}
	cmp		r2, #0x0
	moveq	r0, #-1
	popeq	{pc}
	bl		memcmp
	pop		{pc}
.endfunc
*/
strcmp:
1:	ldrb	r2, [r0], #0x1
	ldrb	r3, [r1], #0x1
	cmp		r2, r3
	movne	r0, #-1
	movne	pc, lr
	cmp		r2, #0x0
	moveq	r0, #0
	moveq	pc, lr
	b		1b
.endfunc

