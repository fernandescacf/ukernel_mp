/**
 * @file        zone.h
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        22 December, 2019
 * @brief       Memory Zone header file
*/

#ifndef ZONE_H
#define ZONE_H


/* Includes ----------------------------------------------- */
#include <types.h>
#include <mmtypes.h>
#include <misc.h>
#include <klock.h>


/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */


/* Exported types ----------------------------------------- */

typedef enum
{
	ZONE_DIRECT,
	ZONE_INDIRECT
}ZoneType_t;

typedef struct Zone zone_t;

struct Zone
{
	ZoneType_t	zoneType;
	zone_t		*next;
	paddr_t		pAddr;
	vaddr_t		vAddr;
	uint32_t	size;
	uint32_t	availableMemory;
	klock_t     lock;
	struct
	{
		//int32_t	destroy(void *self);
		int32_t	(*destroy)(zone_t *);
		//ptr_t		memoryGet(zone_t *self, ptr_t addr, size_t size);
		ptr_t	(*memoryGet)(zone_t *, ptr_t, size_t);
		//void		memoryFree(zone_t *self, ptr_t memory, size_t size);
		void	(*memoryFree)(zone_t *, ptr_t, size_t);
		//ptr_t MemoryL2P(zone_t *self, ptr_t laddr);
		ptr_t	(*memoryL2P)(zone_t *, ptr_t);
		//ptr_t MemoryP2L(zone_t *self, ptr_t paddr);
		ptr_t	(*memoryP2L)(zone_t *, ptr_t);
		//bool_t FitAddressSpace(zone_t *self, bool_t isPaddr, ptr_t addr, size_t size)
		bool_t	(*fitAddressSpace)(zone_t *, bool_t, ptr_t, size_t);
	}zoneHandler;
};

/* Exported functions ------------------------------------- */

/* @brief	Routine to create the first memory zone (Direct Memory Zone).
 * 			This routine must be called before trying to create any
 * 			other memory zones. It uses static allocated objects to
 * 			manage the zone information and handlers to avoid
 * 			dynamic memory allocations that at this point are not available.
 *
 * @param	pAddr	- base physical address of the memory
 * 			vAddr	- base virtual address of the memory
 * 			size	- size of the memory
 * 			offset	- offset from the base address that the
 * 			memory zone has to discard from the respective
 * 			memory handler
 *
 * @retval	Pointer to the memory zone handler
 */
zone_t *ZoneCreateEarly(paddr_t pAddr, vaddr_t vAddr, uint32_t size, uint32_t offset);

/* @brief	Routine to create a new memory zone. It should only be called
 * 			after ZoneCreateEarly because it depends on dynamic memory
 * 			allocation
 * @param	type	- type of the memory zone
 * 			pAddr	- base physical address of the memory
 * 			vAddr	- base virtual address of the memory
 * 			size	- size of the memory
 * 			offset	- offset from the base address that the
 * 			memory zone has to discard from the respective
 * 			memory handler
 *
 * @retval	Pointer to the memory zone handler
 */

zone_t *ZoneCreate(ZoneType_t type, paddr_t pAddr, vaddr_t vAddr, uint32_t size, uint32_t offset);

/* @brief	Routine to destroy a new memory zone. It should not be necessary
 * 			but anyhow it is available.
 * @param	zone	- pointer to the zone handler
 *
 * @retval	Success
 */
int32_t ZoneDestroy(zone_t *zone);


#endif // ZONE_H
