#include <armv7.h>

// Kernel Memory Size being mapped at early boot
#define KERNEL_SECTION	0x00200000
// MMU related defines
#define MMU_PGTBL_SIZE	0x00004000
#define MMU_FLAGS		0x0001140E
// MMU Barrier pass for secundary cpus
#define MMU_PASS        0xDEADFACE


.macro kernelFirstSection reg
    ldr			\reg, =KernelVirtualBase
    add			\reg, \reg, #KERNEL_SECTION
.endm

.macro pgtbl  reg
     adr			\reg, _start
     sub			\reg, \reg, #MMU_PGTBL_SIZE
.endm

.macro setpte  d, s
    ldr		\d, =MMU_FLAGS
     orr		\d, \d, \s, lsl #20
.endm

.macro get_cpuid  reg
     mrc    p15, 0, \reg, c0, c0, 5
     and    \reg, \reg, #0x03
.endm

.text

.global _start
_start:
    b		boot_code		// Reset			-> 0x00
    b		undef_raw_handler				// Undefined		-> 0x04
    b		svc_raw_handler	// Supervisor		-> 0x08
    b		prefetch_raw_handler				// Pre-fetch Abort	-> 0x0c
    b		abort_raw_handler				// Data Abort		-> 0x10
    NOP						// Hyper-visor		-> 0x14  (not support in this processor)
    b		irq_raw_handler				// IRQ				-> 0x18
    b		.				// FIQ				-> 0x1c

boot_code:
    // Get running cpu
    get_cpuid   r4

    // Check if cpu is the boot cpu (CPU 0)
    cmp     r4, #0
    bne     boot_sec_cpu
    // We may have to unlock the other cpus

    // Save boot parameters (rfs location and size)
    adr     r2, _rfs
    str     r0, [r2], #4
    str     r1, [r2]

    // Initialize cpu
    bl      _cpu_init

    // Initiaçize scu
    bl      disable_scu
    bl      invalidate_scu
    bl      enable_scu

    // Enter SMP domain
    bl      join_smp

    // Unlock next cpu
    get_cpuid   r0
    add		r0, r0, #0x1
    adr		r1, _cpu_init_lock
    bl		lock_signal

    /* Create page tables */
    bl		_create_page_table

    // Signal MMU Page Table is ready
    ldr     r0, =MMU_PASS
    adr     r1, _mmu_barrier
    bl		lock_signal

    /* Enable MMU (pass to virtual memory) */
    ldr		lr, _switch_data
    b		_enable_mmu

.align  2
.type   __switch_data, %object
_switch_data:
    .long   _mmap_switched
    .long   _bss_start
    .long   _bss_end
    .long	_start
    .long	__kernel_stack

.align	2
.type	__rfs, %object
_rfs:
    .long	0x0
    .long	0x0

.align  2
.type   __smp_barriers, %object
_cpu_init_lock:
    .long	0x0
_boot_barrier:
    .long   0x0
_mmu_barrier:
    .long   0x0

_mmap_switched:
    /*
     * When we enter this routine we are already
     * executing in the virtual memory space
     */

    /* Get .bss limits and stack value */
    adr     r3, _switch_data + 4
    ldmia	r3!, {r5, r6, r7, r13}

    /* Initialize .bss section */
    mov		r0, #0x0
1:	cmp		r5, r6
    strne	r0, [r5], #4
    bne		1b

    /* Set Exception Vector Base */
    mrc 	p15, 0, r0, c1, c0, 0
    bic		r0, r0, #SCTLR_V		// Low exception vectors
    mcr		p15, 0, r0, c1, c0, 0
    /* Set Exception Vector Base to start*/
    mcr		p15, 0, r7, c12, c0, 0

    // Wait for secundary cpus
    ldr     r1, =_cpu_ready
2:  ldr     r0, [r1]
    cmp     r0, #3
    wfene
    bne     2b

    ldr		r0, =KernelImage

    ldr		r1, =RawFS
    ldr		r2, =_rfs
    ldr		r3, [r2], #4
    str		r3, [r1], #4
    ldr		r3, [r2]
    str		r3, [r1]

    b		kernelMain

    .long 	0xffffffff
    .long 	0xffffffff
    .long 	0xffffffff

2:	wfi
    b		2b

block_cpu:
    b		.

ret:
    bx		lr

_cpu_init:
    /* Disable Memory System (I-Cache, D-Cache, MMU) */
    mrc     p15, 0, r0, c1, c0, 0
    bic     r0, r0, #SCTLR_M       		// Clear M bit (MMU disable)
    bic     r0, r0, #SCTLR_D       		// Clear D bit (D-cache disable)
    bic     r0, r0, #SCTLR_I       		// Clear I bit (I-cache disable)
    mcr     p15, 0, r0, c1, c0, 0

    /* Enable coherent requests to the processor */
    MRC 	p15, 0, r0, c1, c0, 1			// Read Auxiliary Control Register
    orr		r0, r0, #(1 << 6)
    MCR 	p15, 0, r0, c1, c0, 1			// Write Auxiliary Control Register

    /* Invalidate Data Cache */
    adr     r12, _setup_stack			// local stack
    stmia   r12, {r0-r5, lr}            // invalidate_l1 curropts r0-r6
    bl      v7_invalidate_Dcache
    ldmia   r12, {r0-r5, lr}

    /* Invalidate Instruction Cache */
    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 0

    /* Invalidate Branch Predictor */
    mcr     p15, 0, r0, c7, c5, 6

    bx		lr

.align  2
_setup_stack:
    .space  4 * 7


_create_page_table:
    pgtbl	r4
    /* Clear 16k level 1 page table */
    mov    r0, r4               	//r0 = l1 page table base
    mov    r3, #0
    add    r6, r0, #MMU_PGTBL_SIZE  //r6 = l1 page table top
1:	str    r3, [r0], #4
    str    r3, [r0], #4
    str    r3, [r0], #4
    str    r3, [r0], #4
    teq    r0, r6
    bne    1b

    /*
     * Create identity mapping for first MB of
     * kernel to cater for the MMU enable.
     * This identity mapping will be removed later.
     */
    lsr		r6, pc, #20				// start of the kernel section
    setpte	r3, r6					// generate page table entry
    str     r3, [r4, r6, lsl #2]	// identity mapping

    /*
     * Now setup the pagetables for our kernel direct
     * mapped region.
     */
    ldr		r6, =KernelVirtualBase
    add		r0, r4, r6, LSR #18
    str		r3, [r0], #4
    kernelFirstSection r6
    lsr		r6, r6, #18
    add		r6, r6, r4
1:	cmp		r0, r6
    add		r3, r3, #1 << 20
    strne	r3, [r0], #4
    bne		1b

    // Convert rfs address from physical to virtual
    ldr		r0, =KernelVirtualBase
    // Kernel page table has to be located at the kernel base
    pgtbl   r1
    // Get offset from virtual to physical
    sub		r0, r0, r1
    // Rewrite RFS address
    adr		r1, _rfs
    ldr		r2, [r1]
    add		r0, r2, r0
    str		r0, [r1]

    bx		lr


_enable_mmu:
    /* write to TTBCR						*/
    mov		r5, #1
    mcr		p15, 0, r5, c2, c0, 2
    /* write domains (client)				*/
    ldr		r5, =0x55555555
    mcr		p15, 0, r5, c3, c0, 0
    /* write to TTBR1 MMU Page Table		*/
//    orr		r4, r4, #0x62
    mcr		p15, 0, r4, c2, c0, 1
    /* write to TTBR0 MMU Page Table		*/
    mcr		p15, 0, r4, c2, c0, 0
    /* Invalidate TLBs						*/
    mov		r5, #0
    mcr		p15, 0, r5, c8, c7, 0
    mcr 	p15, 0, r5, c8, c6, 0
    mcr 	p15, 0, r5, c8, c5, 0

    /* Enable MMU, Caches, Branch Prediction, Write Buffer, ... */
    mrc		p15, 0, r5, c1, c0, 0
    orr		r5, r5, #(1<<0)
    bic		r5, r5, #(1<<1)
    orr		r5, r5, #(1<<2)
	orr		r5, r5, #(1<<3)
    orr		r5, r5, #(1<<11)
    orr		r5, r5, #(1<<12)
    orr		r5, r5, #(1<<22)
    orr		r5, r5, #(1<<23)
    mcr		p15, 0, r5, c1, c0, 0
    isb
    dsb

    bx		lr

// --------------------------- Secundary CPUS Boot --------------------------- //
// (r0 -> signal, r1 -> lock)
lock_signal:
    str     r0, [r1]
    isb
    dsb
    dmb
    sev
    bx      lr

// (r0 -> signal, r1 -> lock)
lock_wait:
1:  ldr     r2, [r1]
    cmp		r0, r2
    wfene
    bne		1b
    bx      lr

cpu_set_ready:
    ldr     r0, =_cpu_ready
0:	ldrex   r1, [r0]
    add     r1, r1, #0x1
    strex	r2, r1, [r0]
    cmp     r2, #0
    bne		0b
    dmb
    sev
    bx		lr

.global cpus_set_stacks
.func   cpus_set_stacks
cpus_set_stacks:
    ldr     r1, =_cpu_stacks
    str     r0, [r1]
    dsb
    dmb
    bx      lr
.endfunc

.global cpu_boot_finish
.func   cpu_boot_finish
cpu_boot_finish:
    mov     r0, #0x1
    ldr     r1, =_smp_lock
    str     r0, [r1]
    dsb
    dmb
    sev
    bx      lr
.endfunc

cpu_get_stack:
    get_cpuid   r0
    ldr     r1, =_cpu_stacks
    ldr     r1, [r1]
    add     r1, r1, r0, LSL #2
    ldr     sp, [r1]
    bx      lr

boot_sec_cpu:
    get_cpuid   r0
    adr		r1, _cpu_init_lock
    bl		lock_wait

    // Initialize cpu
    bl      _cpu_init

    // Enter SMP domain
    bl      join_smp

    // Unlock next cpu
    get_cpuid   r0
    add		r0, r0, #0x1
    adr		r1, _cpu_init_lock
    bl		lock_signal

    // Wait for MMU initialization
    ldr     r0, =MMU_PASS
    adr     r1, _mmu_barrier
    bl      lock_wait

    // Enable MMU (pass to virtual memory)
    pgtbl   r4
    ldr     lr, =cpu_holdpen
    b       _enable_mmu


cpu_holdpen:
    bl      cpu_set_ready

    // Set Exception Vector Base
    mrc     p15, 0, r0, c1, c0, 0
    bic     r0, r0, #SCTLR_V        // Low exception vectors
    mcr     p15, 0, r0, c1, c0, 0
    // Set Exception Vector Base to start
    ldr     r0, =_start
    mcr     p15, 0, r0, c12, c0, 0

    // Hold until cpu 0 signals to continue
    mov     r0, #0
    ldr     r1,= _smp_lock
1:  ldr     r0, [r1]
    cmp     r0, #1
    wfene
    bne     1b

    // Set cpu stack (allocated by cpu 0)
    bl      cpu_get_stack

    // We are ready to start executing the kernel code
    bl      kernelMainSec

_block:
1:  wfi
    b       1b



.data
.align 2
.global KernelImage
.type   KernelImage, %object
KernelImage:
    .long	_text_start
    .long	(_rodata_end)
    .long	_data_start
    .long	(_bss_end)
    .long	__kernel_stack_base
    .long	(__kernel_stack)
//	.long	__Kimage_end
RawFS:
    .long	0x0
    .long	0x0

.align  2
.type   __cpu_stacks, %object
_cpu_stacks:
    .long   0x0

.align  2
.type   __cpu_ready, %object
_cpu_ready:
    .long   0x0

.align  2
.type   __smp_lock, %object
_smp_lock:
    .long   0x0
