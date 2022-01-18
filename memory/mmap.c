/**
 * @file        mmap.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        14 May, 2020
 * @brief       Memory Map implementation
*/

/* Includes ----------------------------------------------- */
#include <mmap.h>
#include <string.h>
#include <system.h>
#include <ipc_2.h>
#include <memmgr.h>
#include <procmgr.h>
#include <process.h>
#include <rfs.h>
#include <kheap.h>
#include <scheduler.h>


/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */
// MMap defines
#define PROT_MASK		(0x3)
#define PROT_READ		(1 << 0)	// Data can be read
#define PROT_WRITE		(1 << 1)	// Data can be written
#define PROT_EXEC		(1 << 2)	// Data can be execute.
#define PROT_NOCACHE	(1 << 3)	// Don't use caches
#define PROT_NONE		(0 << 0)	// Data cannot be accessed

#define MAP_SHARED		(1 << 0)	// Changes are shared
#define MAP_PRIVATE 	(1 << 1)	// Changes are private
#define MAP_FIXED		(1 << 2)	// Interpret address exactly
#define MAP_ANON		(1 << 5)	// Map anonymous memory not associated with any specific file
#define MAP_ANONYMOUS	MAP_ANON
#define MAP_PHYS		(1 << 6)	// Map physical memory

#define MAP_RFS			((uint32_t)(-1))

/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */

memCfg_t *MmapGetFlags(int32_t prot, uint32_t flags)
{
	memCfg_t *memcfg = (memCfg_t*)kmalloc(sizeof(memCfg_t));

	memset(memcfg, 0x0, sizeof(memCfg_t));

	// No access
	if(prot == PROT_NONE)
	{
		return memcfg;
	}

	// Access permissions
	if(prot & PROT_WRITE)
	{
		memcfg->apolicy = APOLICY_RWRW;
	}
	else if(prot & PROT_READ)
	{
		memcfg->apolicy = APOLICY_RWRO;
	}

	// Cache use
	if(!(prot & PROT_NOCACHE))
	{
		memcfg->cpolicy = CPOLICY_WRITEALLOC;
	}

	// Is memory executable
	if(prot & PROT_EXEC)
	{
		memcfg->executable = TRUE;
	}

	// Is memory shared
	if(flags & MAP_SHARED)
	{
		memcfg->shared = TRUE;
	}

	// Process memory is not global
	memcfg->global = FALSE;

	return memcfg;
}

extern uint32_t msb32(register uint32_t x);

mbv_t* GetPages(size_t size, uint32_t* pages)
{
	size_t helper[32];

	size = ROUND_UP(size, PAGE_SIZE);

	uint32_t i;
	for(i = 0; size > 0; ++i)
	{
		helper[i] = msb32(size);
		size ^= helper[i];
	}

	// Get memory vector with the needed size
	mbv_t* mbv = (mbv_t*)kmalloc(sizeof(mbv_t) * i);
	// Allocate memory
	uint32_t j;
	for(j = 0; j < i; j++)
	{
		mbv[j].size = helper[j];
		mbv[j].data = MemoryGet(mbv[j].size, ZONE_INDIRECT);

		if(mbv[j].data  == NULL)
		{
			for(j -= 1; j >= 0; j--)
			{
				MemoryFree(mbv[j].data, mbv[j].size);
			}

			return NULL;
		}
	}

	*pages = i;

	return mbv;
}

/* Private functions -------------------------------------- */

/**
 * Mmap Implementation (See header file for description)
*/
vaddr_t Mmap(void* addr, size_t len, int32_t prot, uint32_t flags, int32_t fd, uint32_t off)
{
	// Get running process
	process_t *process = SchedGetRunningProcess();

	// Check if we have a valid fd
	if(fd == NOFD)
	{
		// This is a memory request to the system but what type of request
		if(flags & MAP_PHYS)
		{
			if(off == MAP_RFS)
			{
				mbv_t *mbv = (mbv_t*)kmalloc(sizeof(mbv_t));

				if(RfsGet(&mbv->data, &mbv->size) != E_OK)
				{
					kfree(mbv, sizeof(*mbv));
					return NULL;
				}

				// Convert from virtual to physical
				mbv->data = MemoryL2P(mbv->data);

				// Handle mapping flags
				memCfg_t *memcfg = MmapGetFlags(prot, flags);

				return ProcessRegisterPrivMemory(process, mbv, 1, mbv->size, memcfg);
			}

			// Device Mapping request
			dev_t *device = DeviceGet((paddr_t)off, len);

			if(device == NULL)
			{
				return NULL;
			}

			// Handle mapping flags
			memCfg_t *memcfg = MmapGetFlags(prot, flags);

			dev_obj_t *devobj = ProcessRegisterDevice(process, device, memcfg);

			if(devobj == NULL)
			{
				DeviceFree(device);
				return NULL;
			}

			return devobj->vaddr;

		}
		else if(flags & MAP_ANON)
		{
/*			// Memory allocation request
			// We can only allocate PAGE_SIZE multiple sizes
			len = ROUND_UP(len, PAGE_SIZE);
			// Check how much pages we need (we allocate page by page)
			uint32_t pages = (len / PAGE_SIZE);
			// Get memory vector with the needed size
			mbv_t *mbv = (mbv_t*)kmalloc(sizeof(mbv_t) * pages);
			// Allocate memory
			uint32_t i;
			for(i = 0; (i < pages) && (mbv[i - 1].data); i++)
			{
				mbv[i].size = PAGE_SIZE;
				mbv[i].data = MemoryGet(PAGE_SIZE, ZONE_INDIRECT);
			}

			// Check if we where able to allocate all memory
			if((i < pages) && (mbv[i - 1].data))
			{
				// Allocation failed: Flush memory
				for(i -= 1; i >= 0; i--)
				{
					MemoryFree(mbv[i].data, PAGE_SIZE);
				}

				kfree(mbv, sizeof(mbv_t) * pages);

				return NULL;
			}
*/
			uint32_t pages = 0;
			mbv_t* mbv = GetPages(len, &pages);

			if(mbv == NULL)
			{
				return NULL;
			}

			// Handle mapping flags
			memCfg_t *memcfg = MmapGetFlags(prot, flags);

			return ProcessRegisterPrivMemory(process, mbv, pages, len, memcfg);

		}
		else
		{
			// Not supported
			return NULL;
		}
	}
	else
	{
		// We are mapping a shared object
		// Check for not supported flags
		if((flags & MAP_PHYS) || (flags & MAP_ANON) || (flags & MAP_PRIVATE))
		{
			return NULL;
		}

		// fd == coid
		clink_t* link = (clink_t*)VectorPeek(&process->connections, (uint32_t)fd);

		// TODO: Add support to mmap the same fd more than once


		if((link == NULL) || (link->privMap != NULL) || (link->connection->shared == NULL))
		{
			return NULL;
		}

		// Get shared object from the connection
		sobj_t* shared = link->connection->shared;

		// Cross mapping flags with what is allowed by the server

		memCfg_t* memcfg = MmapGetFlags(prot, flags);

		// TODO: Do we want to check the size or just use the total object size???

		link->privMap = ProcessRegisterShareMemory(process, fd, shared, memcfg);

		if(link->privMap == NULL)
		{
			kfree(memcfg, sizeof(*memcfg));
			return NULL;
		}

		return link->privMap->map.vaddr;
	}

	return NULL;
}

int32_t Munmap(void* addr, size_t len)
{
	// Get running process
	process_t* process = SchedGetRunningProcess();

	// Is it a private memory object
	pobj_t* obj = GLISTNODE2TYPE(GlistGetObject(&process->Memory.privList, addr), pobj_t, node);

	if(obj != NULL)
	{
		// Check if len is valid
		if(obj->size != len)
		{
			return E_INVAL;
		}

		// Process API will handle all deallocations
		return ProcessCleanPrivateObject(obj);
	}

	// Is it a shared reference object
	sref_t* sref = GLISTNODE2TYPE(GlistGetObject(&process->Memory.sharedList, addr), sref_t, node);

	if(sref != NULL)
	{
		// Check if len is valid
		if(sref->map.size != len)
		{
			return E_INVAL;
		}

		clink_t* link = (clink_t*)VectorPeek(&process->connections, (uint32_t)sref->coid);
		link->privMap = NULL;

		// Process API will handle all deallocations
		return ProcessCleanSharedRef(process, sref);

		// TODO: delete also the shared object?
	}

	// Is it a device object
	dev_obj_t * devobj = GLISTNODE2TYPE(GlistGetObject(&process->Memory.devicesList, addr), dev_obj_t, node);

	if(devobj != NULL)
	{
		// It is a device we are unmapping
		return ProcessCleanDevice(process, devobj);
	}

	return E_INVAL;
}

vaddr_t ShareObject(void* addr, int32_t uscoid, uint32_t flags)
{
	// Get running process
	process_t* process = SchedGetRunningProcess();

	int32_t chid = CONNECTION_CHID(uscoid);
	int32_t scoid = CONNECTION_SCOID(uscoid);

	// Get Channel
	channel_t* channel = (channel_t*)VectorPeek(&process->channels, chid);

	if(channel == NULL)
	{
		return NULL;
	}

	// Get Connection with who we want to share the object
	connection_t* connection = (connection_t *)VectorPeek(&channel->connections, scoid);

	if(connection == NULL)
	{
		return NULL;
	}

	// Get private object that we want to share
	pobj_t* obj = GLISTNODE2TYPE(GlistGetObject(&process->Memory.privList, addr), pobj_t, node);

	// Did we get an object?
	if(obj == NULL)
	{
		return NULL;
	}

	sobj_t* shared = (sobj_t*)kmalloc(sizeof(sobj_t));
	shared->refs = 1;
	// TODO: check flags...
	shared->flags = flags;
	shared->obj = obj;

	connection->shared = shared;

	return addr;
}

int32_t UnshareObject(int32_t uscoid)
{
	// Get running process
	process_t* process = SchedGetRunningProcess();

	int32_t chid = CONNECTION_CHID(uscoid);
	int32_t scoid = CONNECTION_SCOID(uscoid);

	// Get Channel
	channel_t* channel = (channel_t*)VectorPeek(&process->channels, chid);

	if(channel == NULL)
	{
		return E_INVAL;
	}

	// Get Connection owner of the shared object
	connection_t* connection = (connection_t *)VectorPeek(&channel->connections, scoid);

	if(connection == NULL || connection->shared == NULL)
	{
		return E_OK;
	}

	// If any link to this connection was mapping this shared object we need to unmap it
	clink_t* link = GLIST_FIRST(&connection->clinks, clink_t, node);
	for(; link != NULL; link = GLIST_NEXT(&link->node, clink_t, node))
	{
		// TODO: check if link is valid
		if(link->privMap != NULL)
		{
			ProcessCleanSharedRef(ProcGetProcess(link->pid), link->privMap);
		}
	}

	connection->shared->obj->refs -= 1;
	kfree(connection->shared, sizeof(*connection->shared));
	connection->shared = NULL;

	return E_OK;
}
