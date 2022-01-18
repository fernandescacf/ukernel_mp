/**
 * @file        vmap.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        December 28, 2019
 * @brief       Virtual Memory Mapper Implementation
*/

/* Includes ----------------------------------------------- */
#include <vpage.h>
#include <kheap.h>
#include <string.h>
#include <misc.h>


/* Private types ------------------------------------------ */



/* Private constants -------------------------------------- */



/* Private macros ----------------------------------------- */


/* Private variables -------------------------------------- */

static memCfg_t mem_types [] =
{
		{CPOLICY_WRITEALLOC,		APOLICY_RONA, TRUE,  TRUE,  TRUE},  // Kernel text page
		{CPOLICY_WRITEALLOC, 		APOLICY_RWNA, TRUE,  TRUE,  TRUE},  // kernel data page
		{CPOLICY_DEVICE_SHARED, 	APOLICY_RWNA, TRUE,  FALSE, TRUE},  // kernel device page
		{CPOLICY_WRITEALLOC,		APOLICY_RWRO, TRUE,  TRUE,  FALSE}, // User text page
		{CPOLICY_WRITEALLOC,		APOLICY_RWRW, TRUE,  FALSE, FALSE}, // User data page
		{CPOLICY_DEVICE_SHARED,		APOLICY_RWRW, TRUE,  FALSE, FALSE}, // User device page
		{CPOLICY_STRONGLY_ORDERED,	APOLICY_NANA, FALSE, FALSE, FALSE}, // Fault
};


/* Private function prototypes ---------------------------- */

/**
 * vPageMapMemory Implementation (See header file for description)
*/
uint32_t vPageMapMemory(klock_t* lock, pgt_t pgt, paddr_t pAddr, vaddr_t vAddr, size_t size, ulong_t mapType, memCfg_t *memcfg)
{
	// Ensure that the physical address is aligned to a PAGE SIZE boundary
	paddr_t alignAddr = (ptr_t)ROUND_DOWN((uint32_t)pAddr,PAGE_SIZE);
	uint32_t MapSize = ROUND_UP(size + (uint32_t)((uint32_t)pAddr - (uint32_t)alignAddr), PAGE_SIZE);

	uint32_t status = 0;
	if(lock != NULL) Klock(lock, &status);

	// Chose the must appropriated function to map the memory
	if(mapType == PAGE_FAULT)
	{
		// Use the unmap function to be sure that the page table entries will be set to zero
		MemoryUnmap(pgt, vAddr, MapSize);
	}
	else if(mapType == PAGE_CUSTOM)
	{
		// Map the memory
		MemoryMap(pgt, alignAddr, vAddr, MapSize, memcfg);
	}
	else
	{
		// Map the memory
		MemoryMap(pgt, alignAddr, vAddr, MapSize, &mem_types[mapType]);
	}

//	MemorySynchronize();
	uint32_t pid = ((vAddr > (vaddr_t)0x80000000) ? (0) : (-1));
	MemoryVmaSynchronize(vAddr, size, pid);

	if(lock != NULL) Kunlock(lock, &status);

	return MapSize;
}

/**
 * vPageMap Implementation (See header file for description)
*/
vPage_t *vPageMap(klock_t* lock, pgt_t pgt, paddr_t pAddr, vaddr_t vAddr, size_t size, ulong_t mapType, memCfg_t *memcfg)
{
	if((mapType == PAGE_CUSTOM) && (memcfg == NULL))
	{
		return NULL;
	}

	// Create a virtual page handler
	vPage_t *vPage = (vPage_t*)kmalloc(sizeof(vPage_t));

	if(!vPage)
	{
		return NULL;
	}

	// Create a physical memory handler to store the info related to the physical memory
	pmm_t *pmm = (pmm_t*)kmalloc(sizeof(pmm_t));

	if(!pmm)
	{
		kfree(vPage, sizeof(*vPage));
		return NULL;
	}

	memset(vPage,0x0,sizeof(vPage_t));
	memset(pmm,0x0,sizeof(pmm_t));

	pmm->addr = pAddr;
	pmm->size = size;

	vPage->pPageList = pmm;
	vPage->mapType = mapType;
	vPage->memcfg = memcfg;
	vPage->vAddr = vAddr;

	vPage->size = vPageMapMemory(lock, pgt, pAddr, vAddr, size, mapType, memcfg);

	return vPage;
}

/**
 * vPageUnmap Implementation (See header file for description)
*/
int32_t vPageUnmap(klock_t* lock, pgt_t pgt, vPage_t *vPage, void *arg, int32_t (*unmapHandler)(void*, pmm_t*))
{
	if(!vPage)
	{
		return E_INVAL;
	}

	uint32_t status = 0;
	if(lock != NULL) Klock(lock, &status);

	MemoryUnmap(pgt, vPage->vAddr, vPage->size);

	uint32_t pid = ((vPage->vAddr > (vaddr_t)0x80000000) ? (0) : (-1));
	MemoryVmaSynchronize(vPage->vAddr, vPage->size, pid);

	if(lock != NULL) Kunlock(lock, &status);

	pmm_t *pmm = vPage->pPageList;
	while(pmm)
	{
		if(unmapHandler)
		{
			unmapHandler(arg, pmm);
		}
		pmm_t *__pmm = pmm->next;
		kfree(pmm, sizeof(*pmm));
		pmm = __pmm;
	}

//	MemorySynchronize();

	kfree(vPage, sizeof(*vPage));

	return E_OK;
}
