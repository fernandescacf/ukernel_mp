/**
 * @file        zonedevice.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        07 January, 2020
 * @brief       Device Zone header file
*/

#ifndef ZONEDEVICE_H_
#define ZONEDEVICE_H_


/* Includes ----------------------------------------------- */
#include <types.h>
#include <misc.h>


/* Exported constants ------------------------------------- */



/* Exported macros ---------------------------------------- */



/* Exported types ----------------------------------------- */

typedef struct DEV
{
	struct DEV	   *next;
	paddr_t			addr;
	size_t			size;
	uint32_t		lock;
	uint16_t		refs;
	uint16_t		len;
	char 			name[1];
}dev_t;

/* Exported functions ------------------------------------- */

int32_t DeviceRegister(paddr_t addr, size_t size, char *name);

dev_t *DeviceGet(paddr_t addr, size_t size);

dev_t *DeviceGetByName(const char *name);

int32_t DeviceLock(dev_t *dev);

int32_t DeviceFree(dev_t *dev);

#endif /* ZONEDEVICE_H_ */
