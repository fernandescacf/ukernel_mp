/**
 * @file        gic.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        16 December, 2017
 * @brief       GIC driver header file
*/

#ifndef _GIC_H_
#define _GIC_H_

#ifdef __cplusplus
    extern "C" {
#endif

/* Includes ----------------------------------------------- */
#include <types.h>
#include <interrupt.h>


/* Exported constants ------------------------------------- */

// Number of 32 bit registers necessary in order to allocate enough space for 1020 irqs (1 bit each)
#define GIC_NUM_REGISTERS			(480 / 32)
#define GIC_PRIORITY_REGISTERS		(0x1FC / 4)
#define GIC_TARGET_REGISTERS		(0x1FC / 4)
#define GIC_CONFIG_REGISTERS		(0x7C / 4)

// Mask for interrupt ID (used with acknowledge, highest pending, software and end of interrupt registers
#define INTERRUPT_MASK				(0x000003FF)

// Mask for Inter Processor Interrupt ID
#define IPI_MASK					(0x0000000F)

// Mask for CPU source ID (used with highest pending and end of interrupt registers
#define CPUID_MASK					(0x00000C00)

// Shift required to locate the CPU_ID field in the interface registers
#define CPUID_SHIFT					(10)

// Shift required to locate the priority field in the interface registers
#define PRIORITY_SHIFT				(4)

// Mask for Priority Mask and Running Interrupt registers
#define PRIORITY_MASK_MASK 			(0x000000FF)

// Shift required to locate the CPU target list field in the Software Interrupt register
#define IPI_TARGET_SHIFT 			(16)

// Shift required to locate the target list filter field in the Software Interrupt register
#define IPI_TARGET_FILTER_SHIFT		(24)

// Target list filter for Software Interrupt register
#define USE_TARGET_LIST				(0x0) // Interrupt sent to the CPUs listed in CPU Target List
#define ALL_BUT_SELF				(0x1) // CPU Target list ignored, interrupt is sent to all but requesting CPU
#define SELF 						(0x2) // CPU Target list ignored, interrupt is sent to requesting CPU only

//The interrupt ID Mask in Interrupt Acknowledge Register
#define GIC_ACK_INTID_MASK			(0x3FF)

#define FAKE_INTERRUPT 				(1023)

/* Exported types ----------------------------------------- */

//Interrupt Distributor access structure
typedef struct
{
/* 0x000 */	volatile uint32_t ctrl; 									// Distributor Control Register (R/W)
/* 0x004 */	volatile uint32_t const typer; 								// Interrupt Controller Type Register (RO)
/* 0x008 */	volatile uint32_t const iidr; 								// Distributor Implementer Identification Register (RO)
/* 0x00c */	const uint8_t reserved1[0x74]; 								// Reserved : (0x74) bytes
/* 0x080 */	volatile uint32_t isr[GIC_NUM_REGISTERS]; 					// Interrupt Interrupt Group Registers (Security Registers) (R/W)
			const uint32_t reserved2[0x20 - GIC_NUM_REGISTERS]; 		// Reserved
/* 0x100 */	volatile uint32_t eset[GIC_NUM_REGISTERS]; 					// Interrupt Set-Enable Registers (R/W)
			const uint32_t reserved3[0x20 - GIC_NUM_REGISTERS]; 		// Reserved
/* 0x180 */	volatile uint32_t eclear[GIC_NUM_REGISTERS];				// Interrupt Clear-Enable Registers (R/W)
			const uint32_t reserved4[0x20 - GIC_NUM_REGISTERS]; 		// Reserved
/* 0x200 */	volatile uint32_t pset[GIC_NUM_REGISTERS]; 					// Interrupt Set-Pending Registers (R/W)
			const uint32_t reserved5[0x20 - GIC_NUM_REGISTERS]; 		// Reserved
/* 0x280 */	volatile uint32_t pclear[GIC_NUM_REGISTERS]; 				// Interrupt Clear-Pending Registers (R/W)
			const uint32_t reserved6[0x20 - GIC_NUM_REGISTERS]; 		// Reserved
/* 0x300 */	volatile uint32_t aset[GIC_NUM_REGISTERS]; 					// Active Bit Registers (R/W)
			const uint32_t reserved7[0x20 - GIC_NUM_REGISTERS];  		// Reserved
/* 0x380 */	volatile uint32_t aclear[GIC_NUM_REGISTERS]; 				// Interrupt Clear-Active Registers
			const uint32_t reserved8[0x20 - GIC_NUM_REGISTERS];			// Reserved
/* 0x400 */	volatile uint32_t prio[GIC_PRIORITY_REGISTERS]; 			// Interrupt Priority Registers (R/W)
			const uint32_t reserved9[0x100 - GIC_PRIORITY_REGISTERS]; 	// Reserved
/* 0x800 */	volatile uint32_t target[GIC_TARGET_REGISTERS]; 			// Interrupt Processor Target Registers (R/W)
			const uint32_t reserved10[0x100 - GIC_TARGET_REGISTERS]; 	// Reserved
/* 0xC00 */	volatile uint32_t config[GIC_CONFIG_REGISTERS]; 			// Interrupt Configuration Registers (R/W)
			const uint32_t reserved11[0x40 - GIC_CONFIG_REGISTERS]; 	// Reserved
/* 0xD00 */	const uint32_t reserved12[0x80]; 							// Reserved
/* 0xF00 */	volatile uint32_t sgir; 									// Software Generated Interrupt Register (R/W)
} gicDist_t;;


// CPU Interface access structure
// Note: These registers are aliased for each CPU
typedef struct
{
/* 0x00 */ volatile uint32_t ctrl; 										// CPU Interface Control Register (R/W)
/* 0x04 */ volatile uint32_t primask; 									// Priority Mask Register (R/W)
/* 0x08 */ volatile uint32_t binpoint; 									// Binary Point Register (R/W)
/* 0x0C */ volatile uint32_t iack; 										// Interrupt Acknowledge Register (R/W)
/* 0x10 */ volatile uint32_t eoi; 										// End of Interrupt Register (R/W)
/* 0x14 */ volatile uint32_t run_pri; 									// Running Priority Register (R/W)
/* 0x18 */ volatile uint32_t high_pri; 									// Highest Pending Interrupt Register (R/W)
/* 0x1C */ const uint32_t reserved1[0x38];								// Reserved	
/* 0xFC */ volatile uint32_t const idreg; 								// CPU Interface Implementer Identification Register (RO)
} gicCpu_t;


/* Exported macros ---------------------------------------- */



/* Exported functions ------------------------------------- */

void GicDistributorInit(vaddr_t vaddr);

void GicCpuInterfaceInit(vaddr_t vaddr);


#ifdef __cplusplus
    }
#endif

#endif /* _GIC_H_ */
