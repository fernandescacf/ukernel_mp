/**
 * @file        board.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        17 June, 2020
 * @brief       Arm VE Board Setup Driver file
*/


/* Includes ----------------------------------------------- */
#include <types.h>
#include <devices.h>
#include <kvspace.h>

#include <gic.h>
#include <arch.h>
#include <asm.h>


/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */

#define CBAR_PERIPHBASE_MASK	0xFFFF8000

#define GIC_DIST_OFFSET			0x1000
#define GIC_DIST_SIZE			0x1000
//#define GIC_CPUIF_OFFSET		0x2000
#define GIC_CPUIF_OFFSET		0x100
#define GIC_CPUIF_SIZE			0xF00

#define GIC_AREA_SIZE			(0x2000)//(GIC_DIST_SIZE + GIC_CPUIF_SIZE)

#define PRIV_TIMERS				(2)
#define PRIV_TIMER_OFFSET		(0x0600)

/* Private macros ----------------------------------------- */

#define read_cbar()				({													\
									uint32_t rval; 									\
									asm volatile(									\
											" mrc     p15, 4, %0, c15, c0, 0\n\t" 	\
											: "=r" (rval) : : "memory", "cc"); 		\
									rval;											\
								})

/* Private variables -------------------------------------- */

struct
{
	struct
	{
		vaddr_t  base;
		vaddr_t  distributer;
		vaddr_t  interface;
	}gic;

	struct
	{
		uint32_t number;
		vaddr_t  base;
	}timer;
}board_setup;

/* Private function prototypes ---------------------------- */

static paddr_t gic_periphbase()
{
	uint32_t cbar = read_cbar();

	return (paddr_t)(cbar & (uint32_t)CBAR_PERIPHBASE_MASK);
}

static paddr_t gic_base()
{
	return gic_periphbase();
}

//static paddr_t gic_distributer_base()
static vaddr_t gic_distributer_base(vaddr_t gic_base)
{
	return (vaddr_t)((uint32_t)gic_base + GIC_DIST_OFFSET);
	//return (paddr_t)((uint32_t)gic_periphbase() + GIC_DIST_OFFSET);
}

//static paddr_t gic_interface_base()
static vaddr_t gic_interface_base(vaddr_t gic_base)
{
	return (vaddr_t)((uint32_t)gic_base + GIC_CPUIF_OFFSET);
	//return (paddr_t)((uint32_t)gic_periphbase() + GIC_CPUIF_OFFSET);
}

static vaddr_t private_timers_base(vaddr_t gic_base)
{
	return (vaddr_t)((uint32_t)gic_base + PRIV_TIMER_OFFSET);
}


/* Private functions -------------------------------------- */

int32_t BoardEarlyInit()
{
	//dev_t *gic_dev = DeviceGet(gic_distributer_base(), GIC_AREA_SIZE);
	dev_t *gic_dev = DeviceGet(gic_base(), GIC_AREA_SIZE);

	if(gic_dev == NULL)
	{
		// Even if the RFS doesn't specify the GIC device we will still proceed
	}
	else
	{
		// GIC can only be used by the kernel
		DeviceLock(gic_dev);
	}


	board_setup.gic.base = VirtualSpaceIomap(gic_base(), GIC_AREA_SIZE);
	board_setup.gic.distributer = gic_distributer_base(board_setup.gic.base);
	board_setup.gic.interface = gic_interface_base(board_setup.gic.base);

	//board_setup.gic.distributer = VirtualSpaceIomap(gic_distributer_base(), GIC_DIST_SIZE);
	//board_setup.gic.interface = VirtualSpaceIomap(gic_interface_base(), GIC_CPUIF_SIZE);

	board_setup.timer.number = PRIV_TIMERS;
	board_setup.timer.base = private_timers_base(board_setup.gic.base);

	// Initialize the interrupt controller
	GicDistributorInit(board_setup.gic.distributer);
	GicCpuInterfaceInit(board_setup.gic.interface);

	return E_OK;
}

uint32_t BoardGetCpus()
{
	return (((uint32_t*)(board_setup.gic.base))[1] & 0x03) + 1;
}

int32_t BoardSecCpuInit()
{
	// Initialize the CPU GIC Interface
	GicCpuInterfaceInit(board_setup.gic.interface);

	return E_OK;
}

/*
int32_t BoardInterruptCtrlInit(vaddr_t isr_sp)
{
	if(_cpuId() == 0)
	{
		GicDistributorInit(board_setup.gic.distributer);
	}

	GicCpuInterfaceInit(board_setup.gic.interface);

	// It's not required to set this stack since we will only use the SVC stack
	// Set Interrupt stack
	asm volatile (
				"mrs    	r0, cpsr			\n\t"		// Save current mode
				"cps		0x12				\n\t"		// Change to IRQ mode
				"mov		sp, %[_sp]			\n\t"		// Change to IRQ stack
				"msr     	cpsr, r0			\n\t"		// Restore Mode
				: : [_sp] "r" (isr_sp) : "r0"
			);

	return E_OK;
}
*/
int32_t BoardTestInterrupts()
{
	cpsie();

	InterruptSetTarget(0, 0x0, TRUE);
	InterruptSetPriority(0, 10);
	InterruptEnable(0);

	InterruptGenerate(0, 0);

//	cpsid();

	return E_OK;
}

int32_t BoardSystemTimerInit()
{

	return E_OK;
}

vaddr_t BoardPrivateTimers()
{
	return board_setup.timer.base;
}
