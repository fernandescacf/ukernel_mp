/**
 * @file        kvspace.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        14 May, 2016
 * @brief       Kernel Virtual Address Space Manager implementation
*/

/* Includes ----------------------------------------------- */
#include <kvspace.h>
#include <memmgr.h>


/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */

#define KERNEL_VIRTUAL_SPACE_ADDRESS	(0xF8000000)
#define KERNEL_VIRTUAL_SPACE_SIZE		(0x08000000)


/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */

static vManager_t VirtualSpaceHandler;

/* Private function prototypes ---------------------------- */


/* Private functions -------------------------------------- */

/**
 * VirtualSpaceInit Implementation (See header file for description)
*/
int32_t VirtualSpaceInit()
{
	pgt_t pgt = (pgt_t)MemoryP2L((ptr_t)MemoryKernelPageTableGet());
	vaddr_t base = (vaddr_t)KERNEL_VIRTUAL_SPACE_ADDRESS;
	size_t size = KERNEL_VIRTUAL_SPACE_SIZE;

	return vManagerInitialize(&VirtualSpaceHandler, pgt,  base, (vaddr_t)((uint32_t)base + size), VMANAGER_FIXED);
}

/**
 * VirtualSpaceMmap Implementation (See header file for description)
*/
vaddr_t VirtualSpaceMmap(paddr_t paddr, size_t size)
{
	// Check if the physical address is already mapped in the Kernel logical space
	vaddr_t vaddr = MemoryP2L(paddr);

	// If not, map it in the kernel on demand mapping area
	if(vaddr == NULL)
	{
		return vMap(&VirtualSpaceHandler, paddr, size, PAGE_KERNEL_DATA, NULL);
	}

	return vaddr;
}

/**
 * VirtualSpaceIomap Implementation (See header file for description)
*/
vaddr_t VirtualSpaceIomap(paddr_t paddr, size_t size)
{
	return vMap(&VirtualSpaceHandler, paddr, size, PAGE_KERNEL_DEVICE, NULL);
}

/**
 * VirtualSpaceUnmmap Implementation (See header file for description)
*/
void VirtualSpaceUnmmap(vaddr_t vaddr)
{
	// Only unmap if it is not a logical address
	if(MemoryIsLogicalAddr(vaddr) != TRUE)
	{
		(void)vUnmap(&VirtualSpaceHandler, vaddr);
	}
}


