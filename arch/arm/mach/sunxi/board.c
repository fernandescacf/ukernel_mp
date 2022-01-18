/**
 * @file        board.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        17 June, 2020
 * @brief       Sunxi H3 Board Setup Driver file
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

#define	SCU_OFFSET				0x0
#define SCU_SIZE				0x1000

#define GIC_DIST_OFFSET			0x1000
#define GIC_DIST_SIZE			0x1000
#define GIC_CPUIF_OFFSET		0x2000
#define GIC_CPUIF_SIZE			0x1000
#define GIC_AREA_SIZE			(GIC_DIST_SIZE + GIC_CPUIF_SIZE)

#define SCU_AREA_SIZE			(SCU_SIZE + GIC_AREA_SIZE)

#define SYS_TIMER_BASE_ALIGN	(0x01C20000)
#define SYS_TIMER_BASE			(0x01C20C00)
#define SYS_TIMER_OFFSET		(SYS_TIMER_BASE - SYS_TIMER_BASE_ALIGN)
#define SYS_TIMER_SIZE			(0x0400)

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
	}scu;

	struct
	{
		vaddr_t  base;
	}timer;
}board_setup;

/* Private function prototypes ---------------------------- */

static paddr_t gic_periphbase()
{
	uint32_t cbar = read_cbar();

	return (paddr_t)(cbar & (uint32_t)CBAR_PERIPHBASE_MASK);
}

static paddr_t scu_base()
{
	return gic_periphbase();
}

static paddr_t gic_base()
{
	return (paddr_t)(((uint32_t)scu_base()) + GIC_DIST_OFFSET);
}

static vaddr_t gic_distributer_base(vaddr_t scu_base)
{
	return (vaddr_t)((uint32_t)scu_base + GIC_DIST_OFFSET);
}

static vaddr_t gic_interface_base(vaddr_t scu_base)
{
	return (vaddr_t)((uint32_t)scu_base + GIC_CPUIF_OFFSET);
}

static vaddr_t system_timer_base()
{
	return (vaddr_t)((uint32_t)VirtualSpaceIomap((paddr_t)SYS_TIMER_BASE_ALIGN, SYS_TIMER_SIZE) + SYS_TIMER_OFFSET);
}

/* Private functions -------------------------------------- */

int32_t BoardEarlyInit()
{
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


	board_setup.scu.base = VirtualSpaceIomap(scu_base(), SCU_AREA_SIZE);
	board_setup.scu.distributer = gic_distributer_base(board_setup.scu.base);
	board_setup.scu.interface = gic_interface_base(board_setup.scu.base);

	dev_t *timer_dev = DeviceGet((paddr_t)SYS_TIMER_BASE, SYS_TIMER_SIZE);

	if(timer_dev == NULL)
	{
		// Even if the RFS doesn't specify the timer device we will still proceed
		// TODO: Is there another timer we can use???
	}
	else
	{
		// System Timer can only be used by the kernel
		// TODO: We will not lock the timer because it will lock other timers in the same area
		//       this is dangerous since user drivers can mess up the system timer :)
		//DeviceLock(timer_dev);
		(void)timer_dev;
	}

	board_setup.timer.base = system_timer_base();

	// Initialize the interrupt controller
	GicDistributorInit(board_setup.scu.distributer);
	GicCpuInterfaceInit(board_setup.scu.interface);

	return E_OK;
}

uint32_t BoardGetCpus()
{
	return 4; //(((uint32_t*)(board_setup.scu.base))[1] & 0x03) + 1;
}

int32_t BoardSecCpuInit()
{
	// Initialize the CPU GIC Interface
	GicCpuInterfaceInit(board_setup.scu.interface);

	return E_OK;
}

vaddr_t BoardPrivateTimers()
{
	return board_setup.timer.base;
}
