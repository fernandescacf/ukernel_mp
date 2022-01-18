/**
 * @file        gic.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        16 December, 2017
 * @brief       GIC driver implementation
*/


/* Includes ----------------------------------------------- */
#include <gic.h>
#include <misc.h>
#include <asm.h>


/* Private types ------------------------------------------ */



/* Private constants -------------------------------------- */



/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */

// Interrupt Distributor memory mapped registers
static gicDist_t *gicDistributer;

// CPU Interface memory mapped registers - aliased for each CPU
static gicCpu_t *gicCpuInterface;


/* Private function prototypes ---------------------------- */



/* Private functions -------------------------------------- */

/**
 * interrupt_distributor_init Implementation (See header file for description)
*/
void GicDistributorInit(vaddr_t vaddr)
{
	uint32_t i;

	gicDistributer = (gicDist_t*)(vaddr);

	// Disable the whole controller (Secure and Non-Secure)
	gicDistributer->ctrl = 0x00000000;

	// Disable all interrupts
	for (i = 0; i < GIC_NUM_REGISTERS; i++)
	{
		gicDistributer->eclear[i] = 0xFFFFFFFF;			// Clear-Enable
	}

	// Clear all SPI interrupts
	for (i = 1; i < GIC_NUM_REGISTERS; i++)
	{
		gicDistributer->pclear[i] = 0xFFFFFFFF;			// Clear-Pending
	}

	// Reset SPI interrupt priorities
	for (i = 8; i < GIC_PRIORITY_REGISTERS; i++)
	{
		gicDistributer->prio[i] = 0x00000000;
	}

	// Reset interrupt targets
	for (i = 0; i < GIC_TARGET_REGISTERS; i++)
	{
		gicDistributer->target[i] = 0x00000000;
	}

	// Set interrupt configuration (level high sensitive, 1-N)
	for (i = 2; i < GIC_CONFIG_REGISTERS; i++)
	{
		gicDistributer->config[i] = 0x55555555;
	}

	// Enable the interrupt controller
	 gicDistributer->ctrl = 0x00000001;					// Enable_secure
}

/**
 * interrupt_interface_init Implementation (See header file for description)
*/
void GicCpuInterfaceInit(vaddr_t vaddr)
{
	uint32_t i;

	gicCpuInterface = (gicCpu_t*)(vaddr);

	// Clear the bits of the distributor which are CPU-specific
	gicDistributer->pclear[0] = 0xFFFFFFFF;				// Clear-Pending
	for (i = 0; i < 8; i++)
	{
		gicDistributer->prio[i] = 0x00000000;			// SGI and PPI interrupt priorities
	}
	gicDistributer->config[0] = 0xAAAAAAAA;				// SGI and PPI set interrupt configuration
	gicDistributer->config[1] = 0xAAAAAAAA;				// SGI and PPI set interrupt configuration

	// Disable CPU Interface
	gicCpuInterface->ctrl = 0x00000000;

	// Allow interrupts with higher priority (i.e. lower number) than 0xF
	gicCpuInterface->primask = 0x000000F0;

	// All priority bits are compared for pre-emption
	gicCpuInterface->binpoint = 0x00000003;					// 3 or 7 ?????????? ICCBPR[0:2] Se � all 0 a 2 � o 7 ???????

	// Clear any pending interrupts
	for( ; ; )
	{
		uint32_t temp;

		temp = gicCpuInterface->iack;						// interrupt_ack

		if((temp & GIC_ACK_INTID_MASK) == FAKE_INTERRUPT)
		{
			break;
		}

		gicCpuInterface->eoi = temp;						// end_of_interrupt
	}


	// Enable the CPU Interface
	gicCpuInterface->ctrl = 0x00000001;						// Enable_secure

	gicCpuInterface->primask = 0xFF;
}


/**
 * interrupt_priority_set Implementation (See header file for description)
*/
int32_t InterruptSetPriority(uint32_t irq, uint32_t priority)
{
	uint32_t word, bit_shift, temp, old_priority;

	if(priority > 31) priority = 31;

    priority = (priority << 3) & 0xF8;

    // There are 4 interrupt priority registers per word
    word = irq / 4; 										// Register where this interrupt priority is
    bit_shift = (irq % 4) * 8;	 							// Offset of this interrupt's priority within the register

    temp = old_priority = gicDistributer->prio[word]; 		// Get priority register

    temp &= ~((uint32_t)0xF8 << bit_shift); 					// Reset the priority for this interrupt to 0

    temp |= (priority << bit_shift); 						// Set the new priority

    gicDistributer->prio[word] = temp; 						// Write new priority

    return ((old_priority >> bit_shift) & 0xF8); 			// Return original priority
}



/**
 * interrupt_target_set Implementation (See header file for description)
*/
void InterruptSetTarget(uint32_t irq, uint32_t target, uint32_t set)
{
	uint32_t word, bit_shift, temp;

    // There are 4 interrupt target registers per word
    word = irq / 4;
    bit_shift = (irq % 4) * 8;
    target = (1 << target) << bit_shift;

    temp = gicDistributer->target[word];
    if (set)
    {
        temp |= target;
    }
    else
    {
        temp &= ~target;
    }
    gicDistributer->target[word] = temp;			// Interrupt Processor Target Registers
}


/**
 * InterruptEnable Implementation (See header file for description)
*/
void InterruptEnable(uint32_t irq)
{
	uint32_t word;

	/* Calculate interrupt position in register */
	word = irq / 32;
	irq %= 32;
	irq = 1 << irq;

	gicDistributer->eset[word] |= irq;
}

/**
 * InterruptDisable Implementation (See header file for description)
*/
void InterruptDisable(uint32_t irq)
{
	uint32_t word;

	/* Calculate interrupt position in register */
	word = irq / 32;
	irq %= 32;
	irq = 1 << irq;

	gicDistributer->eclear[word] |= irq;
}

void InterruptGenerate(uint32_t irq, uint32_t cpu)
{
	irq &= IPI_MASK;
	uint32_t target_mask = ((1 << cpu) & 0xFF);

	gicDistributer->sgir = (target_mask << IPI_TARGET_SHIFT) | irq;

	dmb();
}

void InterruptGenerateSelf(uint32_t irq)
{
	irq &= IPI_MASK;

	gicDistributer->sgir = (SELF << IPI_TARGET_FILTER_SHIFT) | irq;

	dmb();
}

void InterruptDecode(uint32_t irqData, uint32_t *irq, uint32_t *source)
{
	*source = (irqData >> 10);
	*irq = (irqData & 0x3FF);
}

uint32_t IrqSourceClean()
{
	uint32_t irqData = gicCpuInterface->iack;

//	InterruptUnpend(irqData & 0x3FF);

	return irqData;
}

void InterruptUnpend(uint32_t irq)
{
	int x = irq / 32;
	uint32_t mask = (1 << (irq%32));

	gicDistributer->pclear[x] = mask;
}

void InterruptAcknowledge(uint32_t irq)
{
	gicCpuInterface->eoi = irq;
	InterruptUnpend(irq);
}

uint32_t InterruptPrioritySet()
{
	// Get priority mask
	uint32_t priority_mask = gicCpuInterface->primask;
	// Set priority mask to current interrupt priority
	gicCpuInterface->primask = (gicCpuInterface->run_pri & PRIORITY_MASK_MASK);
	// return previous priority
	return priority_mask;
}

void InterruptPriorityRestore(uint32_t prio)
{
	// Restore original priority mask
	gicCpuInterface->primask = prio;
}

uint32_t InterruptAck()
{
	return gicCpuInterface->iack;
}

void InterruptEnd(uint32_t irq)
{
	gicCpuInterface->eoi = irq;
	InterruptUnpend(irq);
}

void InterruptDispache(uint32_t irq, uint32_t source, void* (*handler)(void*, uint32_t), void *arg)
{
	(void)source;
	(void)irq;

	// Get priority mask
//	uint32_t priority_mask = gicCpuInterface->primask;
	// Set priority mask to current interrupt priority
//	gicCpuInterface->primask = (gicCpuInterface->run_pri & PRIORITY_MASK_MASK);

	// Call Interrupt Handler
	handler(arg, irq);

//	InterruptAcknowledge (irq);

	// Restore original priority mask
//	gicCpuInterface->primask = priority_mask;
}

void Test_InterruptHandler()
{
//	uint32_t irq;
//	uint32_t source;

//	InterruptDecode(&irq, &source);

//	InterruptAcknowledge(irq);
}
