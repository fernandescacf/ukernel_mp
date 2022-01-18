/**
 * @file        zonedirect.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        22 December, 2019
 * @brief       Direct Memory Zone Implementation file
*/

/* Includes ----------------------------------------------- */
#include <zonedirect.h>


/* Private types ------------------------------------------ */



/* Private constants -------------------------------------- */


/* Private macros ----------------------------------------- */

/*
 * @brief	Routine to convert from zone_t* to zoneDirect_t*
 */
inline static zoneDirect_t *child_ptr(zone_t *parent)
{
	return (zoneDirect_t *)(parent - offsetof(zoneDirect_t, zone));
}


/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */

/* @brief	Routine to destroy a direct memory zone. It should not be necessary
 * 			but anyhow it is available.
 * @param	zone	- pointer to the zone handler
 *
 * @retval	Success
 */
static int32_t ZoneDirectDestroy(zone_t *zone);

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

/**
 * ZoneDirectCreate Implementation (See header file for description)
*/
int32_t ZoneDirectCreate(zoneDirect_t *zone, uint32_t offset)
{
	zone->zone.zoneHandler.destroy = ZoneDirectDestroy;
	zone->zone.zoneHandler.memoryGet = GetMemory;
	zone->zone.zoneHandler.memoryFree = FreeMemory;
	zone->buddy = buddySystemCreate(zone->zone.pAddr,zone->zone.vAddr,zone->zone.size, offset);
	buddyInit(zone->buddy);

	return E_OK;
}

/**
 * ZoneDirectDestroy Implementation (See header file for description)
*/
int32_t ZoneDirectDestroy(zone_t *zone)
{
	(void)zone;
	return E_OK;
}

/**
 * GetMemory Implementation (See header file for description)
*/
ptr_t GetMemory(zone_t *zone, ptr_t addr, size_t size)
{
	(void)addr;
	uint32_t status;
	Klock(&zone->lock, &status);
	ptr_t ptr = buddyGetMemory(child_ptr(zone)->buddy, size);
	Kunlock(&zone->lock, &status);
	return ptr;
}

/**
 * FreeMemory Implementation (See header file for description)
*/
void FreeMemory(zone_t *zone, ptr_t memory, size_t size)
{
	uint32_t status;
	Klock(&zone->lock, &status);
	buddyFreeMemory(child_ptr(zone)->buddy, memory, size);
	Kunlock(&zone->lock, &status);
}
