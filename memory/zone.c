/**
 * @file        zone.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        22 December, 2019
 * @brief       Memory Zone Implementation file
*/

/* Includes ----------------------------------------------- */
#include <zone.h>
#include <kheap.h>
#include <zonedirect.h>
#include <zoneindirect.h>


/* Private types ------------------------------------------ */
typedef union ZoneObj
{
	zone_t			zone;
	zoneDirect_t	zoneDirect;
	zoneIndirect_t	zoneIndirect;
}zoneObj_t;


/* Private constants -------------------------------------- */


/* Private macros ----------------------------------------- */


/* Private variables -------------------------------------- */

// The first zone has to be allocated statically since until
// we have the first direct access zone we don't have
// dynamic memory available
static zoneDirect_t	zoneFirst;


/* Private function prototypes ---------------------------- */

/* @brief	Routine to initialize the zone common data fields
 *
 * @param	zone	- pointer to the zone handler
 * 			pAddr	- base physical address of the memory
 * 			vAddr	- base virtual address of the memory
 * 			size	- size of the memory
 * 			offset	- offset from the base address (memory already in use)
 *
 * @retval	Pointer to the memory zone handler
 */
void ZoneInit(zone_t *zone, ptr_t pAddr, ptr_t vAddr, size_t size, uint32_t offset);

/* @brief	Generic routine to check if the given address belongs to the specified zone.
 * 			This routine will be used by default if the concrete zones do not overwrite it.
 *
 * @param	zone	- pointer to the zone handler
 * 			isPaddr	- TRUE if is a physical address FALSE if it is a virtual address
 * 			addr	- address to be checked
 * 			size	- size
 *
 * @retval	TRUE if the address belongs to the zone
 * 			FALSE if the address does not belongs to the zone
 */
bool_t fitAddressSpace(zone_t *self, bool_t isPaddr, ptr_t addr, size_t size);

/* @brief	Generic routine to convert a virtual/logical address to a physical address.
 * 			This routine will be used by default if the concrete zones do not overwrite it.
 *
 * @param	zone	- pointer to the zone handler
 * 			laddr	- address to be converted
 *
 * @retval	Physical address or NULL if it couldn't convert
 */
ptr_t memoryL2P(zone_t *self, ptr_t laddr);

/* @brief	Generic routine to convert a physical address to a virtual/logical address.
 * 			This routine will be used by default if the concrete zones do not overwrite it.
 *
 * @param	zone	- pointer to the zone handler
 * 			laddr	- address to be converted
 *
 * @retval	Virtual/logical address or NULL if it couldn't convert
 */
ptr_t memoryP2L(zone_t *self, ptr_t paddr);

/* Private functions -------------------------------------- */

/**
 * ZoneInit Implementation (See header file for description)
*/
void ZoneInit(zone_t *zone, ptr_t pAddr, ptr_t vAddr, size_t size, uint32_t offset)
{
	zone->pAddr = pAddr;
	zone->vAddr = vAddr;
	zone->size = size;
	zone->availableMemory = size - offset;
	zone->next = NULL;
	zone->zoneHandler.fitAddressSpace = fitAddressSpace;
	zone->zoneHandler.memoryL2P = memoryL2P;
	zone->zoneHandler.memoryP2L = memoryP2L;
	KlockInit(&zone->lock);
}

/**
 * fitAddressSpace Implementation (See header file for description)
*/
bool_t fitAddressSpace(zone_t *self, bool_t isPaddr, ptr_t addr, size_t size)
{
	if(isPaddr)
	{
		return ((self->pAddr <= addr) && (addr <= (ptr_t)(self->pAddr + self->size - size)));
	}
	else if(self->zoneType == ZONE_DIRECT)
	{
		return ((self->vAddr <= addr) && (addr <= (ptr_t)(self->vAddr + self->size - size)));
	}
	else
	{
		return FALSE;
	}
}

/**
 * memoryL2P Implementation (See header file for description)
*/
ptr_t memoryL2P(zone_t *self, ptr_t laddr)
{
	if((self->zoneType == ZONE_DIRECT) && (self->zoneHandler.fitAddressSpace(self, FALSE, laddr, 0)))
	{
		return (self->pAddr + (laddr - self->vAddr));
	}

	return NULL;
}

/**
 * memoryP2L Implementation (See header file for description)
*/
ptr_t memoryP2L(zone_t *self, ptr_t paddr)
{
	if((self->zoneType == ZONE_DIRECT) && (self->zoneHandler.fitAddressSpace(self, TRUE, paddr, 0)))
	{
		return (self->vAddr + (paddr - self->pAddr));
	}
	return NULL;
}

/**
 * ZoneCreateEarly Implementation (See header file for description)
*/
zone_t *ZoneCreateEarly(ptr_t pAddr, ptr_t vAddr, uint32_t size, uint32_t offset)
{
	// Since there is no memory allocator available we get a static allocated zone
	// to use as the first available zone that handles diretly mapped memory
	zoneObj_t *zone = (zoneObj_t*)&zoneFirst;

	zone->zoneDirect.zone.zoneType = ZONE_DIRECT;
	ZoneInit(&zone->zone, pAddr, vAddr, size, offset);

	ZoneDirectCreate(&zone->zoneDirect, offset);

	return &zone->zone;
}

/**
 * ZoneCreate Implementation (See header file for description)
*/
zone_t *ZoneCreate(ZoneType_t type, ptr_t pAddr, ptr_t vAddr, uint32_t size, uint32_t offset){

	zoneObj_t *zone = NULL;

	switch(type)
	{
	case ZONE_DIRECT:
		zone = (zoneObj_t *)kmalloc(sizeof(zoneDirect_t));
		zone->zoneDirect.zone.zoneType = ZONE_DIRECT;
		ZoneInit(&zone->zone, pAddr, vAddr, size, offset);
		ZoneDirectCreate(&zone->zoneDirect, offset);
		break;
	case ZONE_INDIRECT:
		zone = (zoneObj_t *)kmalloc(sizeof(zoneIndirect_t));
		zone->zoneIndirect.zone.zoneType = ZONE_INDIRECT;
		ZoneInit(&zone->zone, pAddr, vAddr, size, offset);
		ZoneIndirectCreate(&zone->zoneIndirect);
		break;
	}

	return &zone->zone;
}
