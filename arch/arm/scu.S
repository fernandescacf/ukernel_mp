/**
 * @file        scu.S
 * @author      Carlos Fernandes, Sandro Pinto
 * @version     1.0
 * @date        15 March, 2015
 * @brief       ARMv7-A SCU Functions
 */


/* Includes ---------------------------------------------------------- */


/* Defines ----------------------------------------------------------- */

.set SCU_CTRL_OFFSET,		(0x00)			// SCU Control Register
.set SCU_CONFIG_OFFSET,		(0x04)			// SCU Configuration Register
.set SCU_SAC_OFFSET,		(0x50)			// SCU Secure Access Control Register
.set SCU_SNSAC_OFFSET,		(0x54)			// SCU Non-secure Access Control Register
.set SCU_INV_AllR_SECST,	(0x0C)
.set SCU_CTRL_E,			(1<<0)			// SCU_CTRL_REG.Enable bit
.set SCU_CPUNUM_MASK,		(0x03)
.set SCU_CPUSMP_SHIFT,		(0x04)
.set SCU_CPUSMP_MASK,		(0x0F)
.set ACTLR_SMP,				(1<<6)
.set ACTLR_FW,				(1<<0)


/* Macros --------------------------------------------------- */


/* Aligment -------------------------------------------------- */
.align 2


/* Imported Functions ---------------------------------------- */



/* Function -------------------------------------------------- */

// ------------------------------------------------------------------
// 							Enable SCU
// ------------------------------------------------------------------
.globl enable_scu
.func enable_scu
	// void enable_scu(void);
enable_scu:
	mrc     p15, 4, r0, c15, c0, 0  		// Read Configuration Base Address Register (periph base address)
	ldr     r1, [r0, #SCU_CTRL_OFFSET]      // Read SCU Control Register (Base Addr + 0x00 offset)
	orr     r1, r1, #SCU_CTRL_E     		// Set Enable bit
	str     r1, [r0, #SCU_CTRL_OFFSET]      // Write SCU Control Register
	bx      lr
.endfunc



// ------------------------------------------------------------------
// 						  Invalidate SCU
//					Invalidate all SCU TAG RAMs
// ------------------------------------------------------------------
.globl invalidate_scu
.func invalidate_scu
	// void invalidate_scu(void);
invalidate_scu:
	mvn		r0, #0							// r0 = 0xFFFFFFFF
	mrc     p15, 4, r1, c15, c0, 0  		// Get Periph Base Address
  	str     r0, [r1, #SCU_INV_AllR_SECST]   // Write to SCU Invalidate All in Secure State
	bx      lr
.endfunc



// ------------------------------------------------------------------
// 							Invalidates SCU
//  Invalidates the SCU copy of the tag rams for the specified core
// ------------------------------------------------------------------
.globl secure_invalidate_scu
.func secure_invalidate_scu
	// void secure_invalidate_scu(unsigned int cpu, unsigned int ways)
secure_invalidate_scu:
	and     r0, r0, #0x03   				// Mask off unused bits of CPU ID
	mov     r0, r0, lsl #2          		// Convert into bit offset (four bits per core)
  	and     r1, r1, #0x0F           		// Mask off unused bits of ways
  	mov     r1, r1, lsl r0          		// Shift ways into the correct CPU field
  	mrc     p15, 4, r2, c15, c0, 0  		// Get Periph Base Address
  	str     r1, [r2, #SCU_INV_AllR_SECST]   // Write to SCU Invalidate All in Secure State
	bx      lr
.endfunc



// ------------------------------------------------------------------
// 							Disable SCU
// ------------------------------------------------------------------
.globl disable_scu
.func disable_scu
	// void disable_scu(void);
disable_scu:
	mrc     p15, 4, r0, c15, c0, 0  		// Read Configuration Base Address Register (periph base address)
	ldr     r1, [r0, #SCU_CTRL_OFFSET]      // Read SCU Control Register (Base Addr + 0x00 offset)
	bic     r1, r1, #SCU_CTRL_E     		// Clear Enable bit
	str     r1, [r0, #SCU_CTRL_OFFSET]      // Write SCU Control Register
	bx      lr
.endfunc






// ------------------------------------------------------------------
// 							Get Base Address
//  Returns the base address of the private peripheral memory space
// ------------------------------------------------------------------
.globl get_base_addr
.func get_base_addr
	// unsigned int get_base_addr(void);
get_base_addr:
	mrc     p15, 4, r0, c15, c0, 0  		// Read Configuration Base Address Register
	bx      lr
.endfunc



// ------------------------------------------------------------------
// 						Get the Number of Cores
//  		Returns the number of CPUs in the A9 Cluster
// ------------------------------------------------------------------
.globl get_num_cpus
.func get_num_cpus
	// unsigned int get_num_cpus(void);
get_num_cpus:
	mrc     p15, 4, r0, c15, c0, 0  		// Read Configuration Base Address Register (Get Periph Base Address)
	ldr     r0, [r0, #SCU_CONFIG_OFFSET]    // Read the SCU Configuration Register (Base Addr + 0x04 offset)
	and     r0, r0, #SCU_CPUNUM_MASK        // Bits [1:0] gives the number of cores
	bx      lr
.endfunc



// ------------------------------------------------------------------
// 							Join SMP
//  			Set this CPU as participating in SMP
// ------------------------------------------------------------------
.globl join_smp
.func join_smp
	// void join_smp(void);
join_smp:
  	mrc     p15, 0, r0, c1, c0, 1			// Read Auxiliary Control Register (ACTLR)
	// Only try to set SMP mode if not yet set
    tst     r0, #ACTLR_SMP 					// SMP bit
    orreq   r0, r0, #ACTLR_SMP				// Set SMP bit in ACTLR
//    orr     r0, r0, #ACTLR_FW               // Enable forward of cache and tlb operations
    mcr     p15, 0, r0, c1, c0, 1			// Write ACTLR
	bx      lr
.endfunc






// ------------------------------------------------------------------
// 							Leave SMP
//  			Set this CPU as NOT participating in SMP
// ------------------------------------------------------------------
.globl leave_smp
.func leave_smp
	// void leave_smp(void);
leave_smp:
	mrc     p15, 0, r0, c1, c0, 1   		// Read ACTLR
  	bic     r0, r0, #ACTLR_SMP          	// Clear SMP bit
  	mcr     p15, 0, r0, c1, c0, 1   		// Write ACTLR
	bx      lr
.endfunc



// ------------------------------------------------------------------
// 						Get CPUs in SMP
//  	Returns the number of CPUs in SMP mode (1bit per core)
// ------------------------------------------------------------------
.globl get_cpus_smp
.func get_cpus_smp
	// unsigned int get_cpus_smp(void);
get_cpus_smp:
	mrc     p15, 4, r0, c15, c0, 0  		// Get Periph Base Address
	ldr     r0, [r0, #SCU_CONFIG_OFFSET]    // Read the SCU Configuration Register (Base Addr + 0x04 offset)
	mov		r0, r0, lsr#SCU_CPUSMP_SHIFT	// Bits [7:4] gives the number of cores in SMP mode. So SHIFT
	and     r0, r0, #SCU_CPUSMP_MASK		// then MASK
	bx      lr
.endfunc





// ------------------------------------------------------------------
// 					Enable Maintenance Broadcast
//   Enable the broadcasting of cache & TLB maintenance operations
// ------------------------------------------------------------------
.globl enable_maintenance_broadcast
.func enable_maintenance_broadcast
	// void enable_maintenance_broadcast(void)
enable_maintenance_broadcast:
	mrc     p15, 0, r0, c1, c0, 1   		// Read Auxiliary Control Register (ACTLR)
  	orr     r0, r0, #ACTLR_FW          		// Set FW bit
  	mcr     p15, 0, r0, c1, c0, 1   		// Write ACTLR
	bx      lr
.endfunc



// ------------------------------------------------------------------
// 					Disable Maintenance Broadcast
//   Disable the broadcasting of cache & TLB maintenance operations
// ------------------------------------------------------------------
.globl disable_maintenance_broadcast
.func disable_maintenance_broadcast
	// void disable_maintenance_broadcast(void)
disable_maintenance_broadcast:
	mrc     p15, 0, r0, c1, c0, 1   		// Read Auxiliary Control Register (ACTLR)
  	bic     r0, r0, #ACTLR_FW          		// Clear FW bit
  	mcr     p15, 0, r0, c1, c0, 1   		// Write ACTLR
	bx      lr
.endfunc



/*********************************************************************
								END
						ARMv7-A SCU Functions
*********************************************************************/
