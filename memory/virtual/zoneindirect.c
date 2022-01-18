/**
 * @file        zoneindirect.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        29 December, 2019
 * @brief       Indirect Memory Zone Implementation file
*/


/* Includes ----------------------------------------------- */
#include <zoneindirect.h>
#include <mmu.h>

/* Private types ------------------------------------------ */



/* Private constants -------------------------------------- */


/* Private macros ----------------------------------------- */

/*
 * @brief	Routine to convert from zone_t* to zoneDirect_t*
 */
inline static zoneIndirect_t *child_ptr(zone_t *parent)
{
	return (zoneIndirect_t *)(parent - offsetof(zoneIndirect_t, zone));
}


/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */

/* @brief	Routine to destroy a direct memory zone. It should not be necessary
 * 			but anyhow it is available.
 * @param	zone	- pointer to the zone handler
 *
 * @retval	Success
 */
static int32_t ZoneIndirectDestroy(zone_t *zone);

/* @brief	Routine to allocate a memory block with the specified size
 *
 * @param	zone	- pointer to the zone handler
 * 			addr	- Not used in direct memory zones
 * 			size	- size of the memory
 *
 * @retval	Pointer to the allocated memory or NULL if the request couldn't be fulfilled
 */
static ptr_t GetMemory(zone_t *zone, ptr_t addr, size_t size);

/* @brief	Routine to return allocated memory to the memory zone.
 *
 * @param	zone	- pointer to the zone handler
 * 			memory	- base address of the allocated memory
 * 			size	- size of the allocated memory
 *
 * @retval	No Return
 */
static void FreeMemory(zone_t *zone, ptr_t memory, size_t size);


/* Private functions -------------------------------------- */

//highMemory_t *highSystemCreate(ptr_t pAddr, uint32_t size, uint32_t offset)
int32_t ZoneIndirectCreate(zoneIndirect_t *zone)
{
	zone->zone.zoneHandler.destroy = ZoneIndirectDestroy;
	zone->zone.zoneHandler.memoryGet = GetMemory;
	zone->zone.zoneHandler.memoryFree = FreeMemory;
	zone->mPool = mPoolCreate(
			zone->zone.pAddr,
			(ptr_t)(zone->zone.pAddr + zone->zone.size),
			PAGE_SIZE,
			PAGE_SIZE,
			(MPOOL_FREEALLOCATION | MPOOL_ALIGNCHECK | MPOOL_UNMAPPED));

	return E_OK;
}

/**
 * ZoneIndirectDestroy Implementation (See header file for description)
*/
ptr_t GetMemory(zone_t *zone, ptr_t addr, size_t size)
{
	(void)addr;
	uint32_t status;
	Klock(&zone->lock, &status);
	ptr_t ptr = MemoryBlockAlloc(child_ptr(zone)->mPool, size);
	Kunlock(&zone->lock, &status);
	return ptr;
}

/**
 * ZoneIndirectDestroy Implementation (See header file for description)
*/
void FreeMemory(zone_t *zone, ptr_t memory, size_t size)
{
	uint32_t status;
	Klock(&zone->lock, &status);
    MemoryBlockFree(child_ptr(zone)->mPool, memory, size);
    Kunlock(&zone->lock, &status);
}

/**
 * ZoneIndirectDestroy Implementation (See header file for description)
*/
int32_t ZoneIndirectDestroy(zone_t *zone)
{
	(void)zone;
	return E_OK;
}

