/**
 * @file        cache.S
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        15 March, 2015
 * @brief       ARMv7-A Cache Driver
 */


/* Includes ---------------------------------------------------------- */


/* Defines ----------------------------------------------------------- */
.set SCTLR_C,				(1<<2) 				// SCTLR.C bit
.set SCTLR_I,				(1<<12) 			// SCTLR.I bit

.set clear_C,				(0x00)
.set clean_dC,				(0x01)
.set cleanInvalidate_dC,	(0x02)


/* Macros --------------------------------------------------- */


/* Aligment -------------------------------------------------- */
.align 2


/* Imported Functions ---------------------------------------- */



/* Function -------------------------------------------------- */


// ------------------------------------------------------------------
// 					Invalidate Instruction Cache
// ------------------------------------------------------------------
.globl v7_invalidate_Icache
.func v7_invalidate_Icache
	// void v7_invalidate_Icache(void);
v7_invalidate_Icache:
	mov     r1, #0
	mcr     p15, 0, r1, c7, c5, 0 		// ICIALLU - Invalidate all Instruction Cache
	bx 		lr
.endfunc

.globl v7_invalidate_Dcache
.func v7_invalidate_Dcache
v7_invalidate_Dcache:
	dmb
	mov     r0, #0
	mcr     p15, 2, r0, c0, c0, 0
	mrc     p15, 1, r0, c0, c0, 0

	movw    r1, #0x7fff
	and     r2, r1, r0, lsr #13

	movw    r1, #0x3ff

	and     r3, r1, r0, lsr #3      	// NumWays - 1
	add     r2, r2, #1              	// NumSets

	and     r0, r0, #0x7
	add     r0, r0, #4      			// SetShift

	clz     r1, r3          			// WayShift
	add     r4, r3, #1      			// NumWays
1:  sub     r2, r2, #1     				// NumSets--
	mov     r3, r4          			// Temp = NumWays
2:  subs    r3, r3, #1      			// Temp--
	mov     r5, r3, lsl r1
	mov     r6, r2, lsl r0
	orr     r5, r5, r6      			// Reg = (Temp<<WayShift)|(NumSets<<SetShift)
	mcr     p15, 0, r5, c7, c6, 2
	bgt     2b
	cmp     r2, #0
	bgt     1b
	dsb     st
	isb
	bx     lr
.endfunc

// ------------------------------------------------------------------
// 						Cache Operation
// ------------------------------------------------------------------
//	r6 = 0x0 invalidate by set/way
//	r6 = 0x1 clean by set/way
//	r6 = 0x2 clean and invalidate by set/way
do_cache_operation:
	dmb									// ensure ordering with previous memory accesses
	// Accessing Cache Level ID Register
	mrc     p15, 1, r0, c0, c0, 1     	// Read Cache Level Identification Register (CLIDR)
  	ands    r3, r0, #0x7000000			// Geting only Level of Coherency (LoC[26:24])
  	mov     r3, r3, lsr #23           	// Aligning LoC
  	beq     cache_operation_end			// If the LoC field value is 0x0, this means that no levels of cache need to be cleaned or invalidated
  	mov     r10, #0						//

  	//
  	// 		r10 -> Cache number
  	//		r2 -> LineSize
  	//		...
  	//

cache_operation_l1:
	add     r2, r10, r10, lsr #1      	// Work out 3xcachelevel
  	mov     r1, r0, lsr r2            	// bottom 3 bits are the Cache type for this level
  	and     r1, r1, #7                	// get those 3 bits alone
  	cmp     r1, #2						// 0x00 - No cache; 0x01 ICache only; 0x02 DChache only; 0x03 Separate I and DCache; 0x04 Unified Cache; RESERVED
  	blt     cache_operation_skip    	// No cache or ICache only at this level
  	mcr     p15, 2, r10, c0, c0, 0    	// Write Cache Size Selection Register (CSSELR)
  	isb                               	// Sync the change to the Cache Size Identification Register (CCSIDR)
  	mrc     p15, 1, r1, c0, c0, 0     	// Reads current CCSIDR
  	and     r2, r1, #0x07               // Get LineSize (CCSIDR[2:0])
  	add     r2, r2, #0x04               // add 4 for the LineSize offset (log2(16 bytes)=4)
  	ldr     r4, =0x3FF					//
  	ands    r4, r4, r1, lsr #3        	// R4 is the max number on the way size (right aligned)
  	clz     r5, r4                    	// R5 is the bit position of the way size increment
  	ldr     r7, =0x00007FFF
  	ands    r7, r7, r1, lsr #13       	// R7 is the max number of the index size (right aligned)

cache_operation_l2:
	mov     r9, r4                    	// R9 working copy of the max way size (right aligned)

cache_operation_l3:
	orr     r11, r10, r9, LSL r5      	// factor in the way number and cache number into R11
  	orr     r11, r11, r7, LSL r2      	// factor in the index number

  	//*************CHECK
  	cmp     r6, #0x0					// Invalidate ?
  	mcreq   p15, 0, r11, c7, c6, 2    	// DCISW - invalidate by set/way
  	cmpne	r6, #0x1					// Clean ?
  	mcreq   p15, 0, r11, c7, c10, 2    	// DCCSW - clean by set/way
  	// Clean and Invalidate
  	mcrne   p15, 0, r11, c7, c14, 2    	// DCCISW - clean and invalidate by set/way
  	//*************CHECK

  	subs    r9, r9, #1                	// decrement the way number
  	bge     cache_operation_l3
  	subs    r7, r7, #1                	// decrement the index
  	bge     cache_operation_l2

cache_operation_skip:
	add     r10, r10, #2              	// increment the cache number
  	cmp     r3, r10
  	bgt     cache_operation_l1

cache_operation_end:
	mov     r10, #0

	/* Code from ARM example */

   mcr     p15, 0, r10, c7, c10, 4    // drain write buffer

//    mcr     p15, 0, r10, c8, c7, 0     // invalidate I + D TLBs
//    mcr     p15, 0, r10, c2, c0, 2     // TTB control register -> this line was messing up the page tables!!!

	/* Acording to Linux Code: */

	mcr		p15, 2, r10, c0, c0, 0		// select current cache level in cssr
	dsb     st
	isb

  	bx      lr



// ------------------------------------------------------------------
// 						Invalidate chaches
// ------------------------------------------------------------------
.globl v7_invalidate_caches
.func v7_invalidate_caches
	// void v7_invalidate_caches(void);
v7_invalidate_caches:
	push    {r4-r12,lr}					// Backup registers

	// Invalidate Instruction Cache
	mov     r0, #0
	mcr     p15, 0, r0, c7, c5, 0 		// ICIALLU - Invalidate all Instruction Cache

	mov		r6, #clear_C				// Identifying operation
	bl		do_cache_operation			// Perform D-Cache Invalidation by set/way

	pop     {r4-r12,lr}					// Restore registers
  	bx      lr
.endfunc



// ------------------------------------------------------------------
// 						Clean D-Cache
// ------------------------------------------------------------------
.globl v7_clean_dcache
.func v7_clean_dcache
	// void v7_clean_dcache(void);
v7_clean_dcache:
	push    {r4-r12,lr}					// Backup registers

	mov		r6, #clean_dC				// Identifying operation
	bl		do_cache_operation			// Perform D-Cache Clean by set/way

	pop     {r4-r12,lr}					// Restore registers
  	bx      lr
.endfunc



// ------------------------------------------------------------------
// 						Invalidate D-Cache
// ------------------------------------------------------------------
.globl v7_invalidate_clean_dcache
.func v7_invalidate_clean_dcache
	// void v7_invalidate_clean_dcache(void);
v7_invalidate_clean_dcache:
	push    {r4-r12,lr}					// Backup registers

	mov		r6, #cleanInvalidate_dC		// Identifying operation
	bl		do_cache_operation			// Perform D-Cache Clean and Invalidation by set/way

	pop     {r4-r12,lr}					// Restore registers
  	bx      lr
.endfunc


// ------------------------------------------------------------------
// 						Invalidate D-Cache_ALL
// ------------------------------------------------------------------
.globl v7_invalidate_dcache_all
.func v7_invalidate_dcache_all
	// void v7_invalidate_dcache_all(void);
v7_invalidate_dcache_all:
	push    {r4-r12,lr}					// Backup registers

	bl	v7_invalidate_Dcache

	pop     {r4-r12,lr}					// Restore registers
	bx	lr
.endfunc


// ------------------------------------------------------------------
// 						Clean D-Cache Line MVA
// ------------------------------------------------------------------
.globl clean_dcache_line_mva
.func clean_dcache_line_mva
	//  void clean_dcache_line_mva(uint32_t mva);
clean_dcache_line_mva:
	mcr		p15, 0, r0, c7, c10, 1		// DCCMVAC - Clean data cache line by MVA (Modified Virtual Address) to PoC (Point of Coherency)
    bx      lr
.endfunc

// ==================================================================
// ====================== BSP Cache Interface =======================
// ==================================================================

.globl InvalidateIcache
.func InvalidateIcache
	//  static inline void InvalidateIcache(void)
	//	Description: Invalidate all instruction caches to PoU and flushes branch target cache.
InvalidateIcache:
	mov     r0, #0
	// Invalidate all Instruction Cache
	//mcr     p15, 0, r0, c7, c5, 0
	// Invalidate entire branch predictor array
	//mcr 	p15, 0, r0, c7, c5, 6

	mcr     p15, 0, r0, c7, c1, 0

	// Full system DSB - make sure that the invalidation is complete
	dsb
	// ISB - make sure the instruction stream sees it
	isb
	// Return
    bx      lr
.endfunc

.globl InvalidateDcache
.func InvalidateDcache
	// void InvalidateDcache(void)
InvalidateDcache:
	push	{lr}
	// Disable interrupts
	mrs		r0, CPSR		// read cpsr register
	and		r0, r0, #0x80	// get irq bit value
	cpsid	i				// disable interrupts
	push	{r0}
	// Do cache operation
	bl		v7_invalidate_dcache_all
	dmb
	// Restore interrupts
	pop		{r0}
	and		r0, r0, #0x80	// ensure that we don't mess with other bits
	mrs		r1, CPSR		// read cpsr register
	bic		r1, r1, #0x80	// clear irq bit
	orr		r1, r1, r0		// restore irq bit
	msr		CPSR, r1		// update cpsr
	// Return
	pop		{pc}
.endfunc

.globl FlushDcache
.func FlushDcache
	// FlushDcache(void)
FlushDcache:
	// According to the Linux code the flush invalidates and cleans the cache
	push	{lr}
	// Disable interrupts
	mrs		r0, CPSR		// read cpsr register
	and		r0, r0, #0x80	// get irq bit value
	cpsid	i				// disable interrupts
	push	{r0}
	// Do cache operation
	bl		v7_invalidate_clean_dcache
	dmb
	// Restore interrupts
	pop		{r0}
	and		r0, r0, #0x80	// ensure that we don't mess with other bits
	mrs		r1, CPSR		// read cpsr register
	bic		r1, r1, #0x80	// clear irq bit
	orr		r1, r1, r0		// restore irq bit
	msr		CPSR, r1		// update cpsr
	// Return
	pop		{pc}
.endfunc

.globl InvalidateCaches
.func InvalidateCaches
	// void InvalidateCaches(void)
InvalidateCaches:
	push	{lr}
	bl		InvalidateIcache
	bl		InvalidateDcache
	pop		{pc}
.endfunc

.globl FlushCaches
.func FlushCaches
	// void FlushCaches(void)
FlushCaches:
	push	{lr}
	bl		FlushDcache
	pop		{pc}
.endfunc
