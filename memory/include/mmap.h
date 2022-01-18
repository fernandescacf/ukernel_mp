/**
 * @file        mmap.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        14 May, 2020
 * @brief       Memory Map Definition Header File
*/

#ifndef _MMAP_H_
#define _MMAP_H_

#ifdef __cplusplus
    extern "C" {
#endif


/* Includes ----------------------------------------------- */
#include <types.h>
#include <vmap.h>
#include <glist.h>

#include "devices.h"


/* Exported types ----------------------------------------- */

// Memory Buffer Vector
typedef struct
{
	ptr_t  data;
	size_t size;
}mbv_t;

// Private Memory Object
typedef struct
{
	glistNode_t node;
	vSpace_t*   vspace;
	vaddr_t     vaddr;
	size_t      size;
	// Used to track shared references to this object
	uint32_t    refs;
	// List of physical memory
	mbv_t*      memory;
	memCfg_t*   memcfg;
	int32_t     parts;
}pobj_t;

// Shared memory object
typedef struct
{
	uint32_t   refs;
	uint32_t   flags;
	pobj_t*    obj;
}sobj_t;

// Reference to a shared memory object
typedef struct
{
	glistNode_t node;       // Used by the process handler (e.g. handle mummap)
	sobj_t*     shared;     // Reference to the object we are mapping
	int32_t     coid;       // Link/Connection owner of thid reference
	struct
	{
		vaddr_t   vaddr;    // Base virtual address
		size_t    size;     // Size
		vSpace_t* vspace;   // Virtual space handler
		memCfg_t* memcfg;   // Memory map configuration (e.g. shared, caches, ...)
	}map;
}sref_t;

typedef struct
{
	glistNode_t   node;
	// Device mapped
	dev_t     	 *dev;
	// Device mapping information
	vaddr_t       vaddr;
	vSpace_t     *vspace;
	// Memory Map configuration
	memCfg_t     *memcfg;
}dev_obj_t;


/* Exported constants ------------------------------------- */



/* Exported macros ---------------------------------------- */



/* Exported functions ------------------------------------- */

vaddr_t Mmap(void *addr, size_t len, int32_t prot, uint32_t flags, int32_t fd, uint32_t off);

int32_t Munmap(void *addr, size_t len);

vaddr_t ShareObject(void *addr, int32_t uscoid, uint32_t flags);

int32_t UnshareObject(int32_t uscoid);

#ifdef __cplusplus
    }
#endif

#endif /* _IO_TYPES_H_ */
