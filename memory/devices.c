/**
 * @file        zonedevice.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        07 January, 2020
 * @brief       Device Zone Implementation file
*/

/* Includes ----------------------------------------------- */
#include <devices.h>
#include <kheap.h>
#include <string.h>


/* Private types ------------------------------------------ */



/* Private constants -------------------------------------- */

#define DEVICE_UNLOCKED		(0x0)
#define DEVICE_LOCKED		(0x1)

/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */

static struct
{
	dev_t		*devs;
	uint32_t	count;
}devices;

/* Private function prototypes ---------------------------- */



/* Private functions -------------------------------------- */

int32_t DeviceRegister(paddr_t addr, size_t size, char *name)
{
	uint32_t len = strlen(name);
	dev_t *dev = (dev_t *)kmalloc(sizeof(dev_t) + len);

	dev->addr = addr;
	dev->size = size;
	dev->lock = DEVICE_UNLOCKED;
	dev->refs = 0;
	dev->len = (uint16_t)len + 1;
	memcpy(dev->name, name, dev->len);

	dev->next = devices.devs;
	devices.devs = dev;

	devices.count += 1;

	return E_OK;
}

dev_t *DeviceGet(paddr_t addr, size_t size)
{
	dev_t *dev = devices.devs;
	while(dev && dev->addr != addr)
	{
		dev = dev->next;
	}

	if((dev != NULL) && (dev->lock == DEVICE_UNLOCKED) && (dev->size == size))
	{
		dev->refs += 1;
		return dev;
	}

	return NULL;
}

int32_t DeviceFree(dev_t *dev)
{
	if(dev->refs == 0)
	{
		return E_ERROR;
	}

	dev->refs -= 1;

	return E_OK;
}

int32_t DeviceLock(dev_t *dev)
{
	dev->lock = DEVICE_LOCKED;

	return E_OK;
}
