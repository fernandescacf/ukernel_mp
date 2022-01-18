/**
 * @file        memmgr.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        26 December, 2019
 * @brief       Kernel Memory Manager Implementation file
*/

/* Includes ----------------------------------------------- */
#include <memmgr.h>
#include <mmu.h>
#include <misc.h>
#include <zone.h>
#include <vpage.h>
#include <kheap.h>
#include <string.h>
#include <rfs.h>


/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */

#define DIRECT_MEMORY_SIZE 			(1920 * 0x100000)
#define KERNEL_VIRTUAL_ADDRESS		((uint32_t)&KernelVirtualBase)

/* Private macros ----------------------------------------- */


/* Private variables -------------------------------------- */
static struct
{
	struct
	{
		uint32_t total;
		uint32_t free;
		uint32_t used;
	}ram;
	struct
	{
		zone_t *first;
		zone_t *devices;
	}zones;
}memmgr;


extern vaddr_t KernelVirtualBase;

/* Private function prototypes ---------------------------- */

inline static void RamAlloc(size_t size)
{
	memmgr.ram.free -= size;
	memmgr.ram.used += size;
}

inline static void RamDealloc(size_t size)
{
	memmgr.ram.free += size;
	memmgr.ram.used -= size;
}


/* Private functions -------------------------------------- */

/**
 * MemoryManagerInit Implementation (See header file for description)
*/
int32_t MemoryManagerInit(bootLayout_t *bootLayout)
{
	memset(&memmgr, 0x0, sizeof(memmgr));

	// To create the first memory zone we need to ensure that we have
	// the memory mapped into the kernel virtual space since the
	// direct zones need to have access to the memory

	// Initialize the MMU
	mmuInitialization();

	// Get Kernel Page Table
	pgt_t pgt = MemoryKernelPageTableGet();

	// Map all available RAM that fits in the direct memory section
	uint32_t mappedSize = 0;
	uint32_t size = 0;
	paddr_t paddr = NULL;
	uint32_t mapSize = 0;
	int32_t cont = 0;
	zone_t *zoneList = NULL;

	do{
		cont = RfsGetRamInfo(&paddr, &size);

		memmgr.ram.total += size;
		memmgr.ram.free += size;

		if(size > (DIRECT_MEMORY_SIZE - mappedSize)){
			mapSize = (DIRECT_MEMORY_SIZE - mappedSize);
		}
		else{
			mapSize = size;
		}

		// Map Memory that fits in the direct memory zone. We use vPageMapMemory function
		// to avoid virtual page handlers allocation, that we don't need
		(void)vPageMapMemory(NULL, pgt, paddr, (vaddr_t)((uint32_t)KERNEL_VIRTUAL_ADDRESS + mappedSize), mapSize, PAGE_KERNEL_DATA, NULL);

		if(NULL == memmgr.zones.first)
		{
			uint32_t offset = ALIGN_UP(((uint32_t)bootLayout->rfs.base + (bootLayout->rfs.size)), PAGE_SIZE) - KERNEL_VIRTUAL_ADDRESS;

			memmgr.zones.first = ZoneCreateEarly(paddr, (vaddr_t)KERNEL_VIRTUAL_ADDRESS, mapSize, offset);
			zoneList = memmgr.zones.first;
			// Update RAM statistics
			memmgr.ram.used += offset;
			memmgr.ram.free -= offset;
		}
		else
		{
			zoneList->next = ZoneCreate(ZONE_DIRECT, paddr, (ptr_t)((uint32_t)KERNEL_VIRTUAL_ADDRESS + mappedSize), mapSize, 0);
			zoneList = zoneList->next;
		}

		mappedSize += mapSize;

	}while(cont && mappedSize < DIRECT_MEMORY_SIZE);

	// Clean any mapping used for the physical to virtual memory transition
	(void)vPageMapMemory(
			NULL,
			(pgt_t)MemoryP2L((ptr_t)pgt),
			(paddr_t)0x0,
			(vaddr_t)0x0,
			(uint32_t)KERNEL_VIRTUAL_ADDRESS,
			PAGE_FAULT, NULL);

	// Initialize the kernel heap
	kheapInit(KHEAP_DEFAULT_SIZE * 4, KHEAP_DEFAULT_SIZE);

	// All the RAM that doesn't fit the direct memory section will be attributed to indirect memory zones
	if(size > mapSize){
		// If there is memory left create an indirect memory zone for it
		zoneList->next = ZoneCreate(ZONE_INDIRECT, (ptr_t)((uint32_t)paddr + mapSize), UNMAPPED, (size - mapSize), 0x0);
		zoneList = zoneList->next;
	}

	return E_OK;
}

/**
 * RamGetTotal Implementation (See header file for description)
*/
uint32_t RamGetTotal()
{
	return memmgr.ram.total;
}

/**
 * RamGetAvailable Implementation (See header file for description)
*/
uint32_t RamGetAvailable()
{
	return memmgr.ram.free;
}

/**
 * RamGetUsage Implementation (See header file for description)
*/
uint32_t RamGetUsage()
{
	return memmgr.ram.used;
}

/**
 * MemoryGetAligned Implementation (See header file for description)
*/
ptr_t MemoryGetAligned(size_t size, ulong_t align, ZoneType_t type)
{
	// Align to minimal size -> Page Size
	size = ROUND_UP(size,PAGE_SIZE);
	align = ROUND_UP(align,PAGE_SIZE);

	// If alignment is bigger than size we get a block of memory with size == align to ensure the alignment
	// than we cut the memory block in 2: block address + size and the rest is returned to the memory system

	size_t memBlockSize = ((align > size) ? align: size);

	ptr_t addr = MemoryGet(memBlockSize, type);

	if(NULL == addr)
	{
		return NULL;
	}

	if(align > size)
	{
		ptr_t tempAddr = (ptr_t)((uint32_t)addr + (memBlockSize - size));
		MemoryFree(tempAddr, (memBlockSize - size));
	}
	return addr;
}

/**
 * MemoryGet Implementation (See header file for description)
*/
ptr_t MemoryGet(uint32_t size, ZoneType_t type)
{
	ptr_t addr = NULL;
	// Align to a page size
	size = ROUND_UP(size, PAGE_SIZE);

	zone_t *zone = memmgr.zones.first;

	while(zone != NULL)
	{
		if(zone->zoneType == type && zone->availableMemory >= size)
		{
			addr = zone->zoneHandler.memoryGet(zone, NULL, size);
			if(addr)
			{
				zone->availableMemory -= size;
				RamAlloc(size);
				return addr;
			}
		}
		zone = zone->next;
	}

	if(ZONE_INDIRECT == type)
	{
		// If there isn't enough high memory try to get memory from the lower memory section
		ptr_t addr = MemoryGet(size, ZONE_DIRECT);
		// The returned address is a logical address so we need to convert it to a physical
		// address since that is what the caller is expecting
		return MemoryL2P(addr);
	}

	return NULL;
}


/**
 * MemoryFree Implementation (See header file for description)
*/
void MemoryFree(ptr_t addr, uint32_t size)
{
	size = ROUND_UP(size,PAGE_SIZE);
	zone_t *zone = memmgr.zones.first;

	while(zone != NULL)
	{
		if(ZONE_DIRECT == zone->zoneType)
		{
			// In principle we should receive a virtual address
			// but it is possible that we receive a physical address
			if(zone->zoneHandler.fitAddressSpace(zone, FALSE, addr, size))
			{
				break;
			}
			else if(zone->zoneHandler.fitAddressSpace(zone, TRUE, addr, size))
			{
				// Direct zones expect a virtual address so we need to convert it
				addr = zone->zoneHandler.memoryP2L(zone, addr);
				break;
			}
		}
		else
		{
			// In the other zones we only use the physical address
			if(zone->zoneHandler.fitAddressSpace(zone, TRUE, addr, size))
			{
				break;
			}
		}

		zone = zone->next;
	}
	if(zone)
	{
		zone->zoneHandler.memoryFree(zone, addr, size);
        zone->availableMemory += size;
        RamDealloc(size);
	}
	else
	{
		// It should never enter the else! The memory will be lost :-(
	}
}

/**
 * MemoryL2P Implementation (See header file for description)
*/
ptr_t MemoryL2P(ptr_t laddr)
{
	zone_t *zone = memmgr.zones.first;
	while(zone && !zone->zoneHandler.fitAddressSpace(zone, FALSE, laddr, 0))
	{
		zone = zone->next;
	}
	if(zone)
	{
		return zone->zoneHandler.memoryL2P(zone, laddr);
	}
	return NULL;
}

/**
 * MemoryP2L Implementation (See header file for description)
*/
ptr_t MemoryP2L(ptr_t paddr)
{
	zone_t *zone = memmgr.zones.first;
	while(zone && !zone->zoneHandler.fitAddressSpace(zone, TRUE, paddr, 0))
	{
		zone = zone->next;
	}
	if(zone)
	{
		return zone->zoneHandler.memoryP2L(zone, paddr);
	}
	return NULL;
}

bool_t MemoryIsLogicalAddr(vaddr_t vaddr)
{
	return ((vaddr >= (vaddr_t)KERNEL_VIRTUAL_ADDRESS) && (vaddr <= (vaddr_t)(KERNEL_VIRTUAL_ADDRESS + DIRECT_MEMORY_SIZE)));
}
