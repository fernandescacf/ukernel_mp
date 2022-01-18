/**
 * @file        zonedirect.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        22 December, 2019
 * @brief       Direct Memory Zone header file
*/

#ifndef ZONEDIRECT_H_
#define ZONEDIRECT_H_


/* Includes ----------------------------------------------- */
#include <types.h>
#include <misc.h>
#include <zone.h>
#include <buddy.h>


/* Exported constants ------------------------------------- */



/* Exported macros ---------------------------------------- */


/* Exported types ----------------------------------------- */

typedef struct ZoneDirect
{
	zone_t		zone;
	buddy_t *	buddy;
}zoneDirect_t;

/* Exported functions ------------------------------------- */

/* @brief	Routine to create a new Direct Memory Zone. It will use a buddy
 * 			system to manage the memory.
 * @param	zone	- pointer to the zone handler
 * 			offset	- offset from the base address (memory already in use)
 *
 * @retval	Success
 */
int32_t ZoneDirectCreate(zoneDirect_t *zone, uint32_t offset);

#endif /* ZONEDIRECT_H_ */
