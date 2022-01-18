/**
 * @file        zoneindirect.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        29 December, 2019
 * @brief       Indirect Memory Zone header file
*/

#ifndef ZONEINDIRECT_H_
#define ZONEINDIRECT_H_


/* Includes ----------------------------------------------- */
#include <types.h>
#include <zone.h>
#include <mpool.h>


/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */


/* Exported types ----------------------------------------- */

typedef struct ZoneIndirect
{
	zone_t		zone;
	mPool_t *	mPool;
}zoneIndirect_t;


/* Exported functions ------------------------------------- */

/* @brief	Routine to create a new Indirect Memory Zone. It will use a memory
 * 			pool to manage the memory (that is unmapped).
 * @param	zone	- pointer to the zone handler
 *
 * @retval	Success
 */
int32_t ZoneIndirectCreate(zoneIndirect_t *zone);

#endif // ZONEINDIRECT_H_
