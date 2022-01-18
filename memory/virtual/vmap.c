/**
 * @file        vmap.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        03 January, 2020
 * @brief       Virtual Memory Mapper Implementation
*/

/* Includes ----------------------------------------------- */
#include <vmap.h>
#include <kheap.h>
#include <string.h>

/* Private types ------------------------------------------ */



/* Private constants -------------------------------------- */

#define VMANAGER_MPOOL_FLAGS	(MPOOL_FREEALLOCATION | MPOOL_ALIGNCHECK | MPOOL_UNMAPPED)
// Virtual Manger configuration flags
#define VM_ALLOCATED			(0x1 << 1)


/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */

void vSpaceListRemove(vSpace_t *vSpace);


/* Private functions -------------------------------------- */

/**
 * vManagerCreate Implementation (See header file for description)
*/
vManager_t *vManagerCreate(pgt_t pgt, vaddr_t vBase, vaddr_t vTop, vMgrType_t type)
{
    vManager_t *vm = (vManager_t*)kmalloc(sizeof(vManager_t));
    if(vm)
    {
        if(vManagerInitialize(vm, pgt, vBase, vTop, type))
        {
            kfree(vm, sizeof(*vm));
            return NULL;
        }
        else
        {
        	vm->flags = VM_ALLOCATED;
            return vm;
        }
    }
    return NULL;
}

/**
 * vManagerInitialize Implementation (See header file for description)
*/
int32_t vManagerInitialize(vManager_t *vm, pgt_t pgt, vaddr_t vBase, vaddr_t vTop, vMgrType_t type)
{
    if(!vm || !pgt)
    {
        return E_INVAL;
    }

    memset(vm, 0x0, sizeof(vManager_t));

    vm->pgt = pgt;
    vm->type = type;
    vm->vSpacePool = mPoolCreate(vBase, vTop, PAGE_SIZE, PAGE_SIZE, VMANAGER_MPOOL_FLAGS);

    if(!vm->vSpacePool)
    {
        memset(vm, 0x0, sizeof(vManager_t));
        return E_NO_RES;
    }

    KlockInit(&vm->lock);

    return E_OK;
}

/**
 * vManagerDestroy Implementation (See header file for description)
*/
int32_t vManagerDestroy(vManager_t *vm)
{
	while(vm->vSpaceList)
	{
		(void)vSpaceRelease(vm->vSpaceList);
	}

	while(vm->fixedPages)
	{
		vPage_t * vPage = vm->fixedPages;
		vm->fixedPages = vPage->next;
		// Unmap memory and release the virtual page handler
		vPageUnmap(&vm->lock, vm->pgt, vPage, vm->arg, vm->unmapHandler);
	}

	(void)mPoolDestroy(vm->vSpacePool);

	if(vm->flags & VM_ALLOCATED)
	{
		kfree(vm, sizeof(*vm));
	}

	return E_OK;
}

/**
 * vManagerSetUnmapHandler Implementation (See header file for description)
*/
int32_t vManagerSetUnmapHandler(vManager_t *vm, void *arg, int32_t (*unmapHandler)(void*, pmm_t*))
{
    if(!vm)
    {
        return E_INVAL;
    }

    vm->unmapHandler = unmapHandler;
    vm->arg = arg;

    return E_OK;
}

/**
 * vSpaceReserve Implementation (See header file for description)
*/
vSpace_t *vSpaceReserve(vManager_t *vm, size_t size)
{
    if(!vm)
    {
        return NULL;
    }

    vSpace_t *vSpace = (vSpace_t*)kmalloc(sizeof(vSpace_t));
    if(!vSpace)
    {
        return NULL;
    }

    memset(vSpace, 0x0, sizeof(vSpace_t));

    size = ROUND_UP(size, PAGE_SIZE);

    vSpace->owner = vm;

    uint32_t status;
    Klock(&vm->lock, &status);

    vSpace->base = MemoryBlockAlloc(vm->vSpacePool, size);

    if(vSpace->base == NULL)
    {
    	Kunlock(&vm->lock, &status);

        kfree(vSpace, sizeof(*vSpace));
        return NULL;
    }
    vSpace->top = (vaddr_t)((uint32_t)(vSpace->base) + size);
    vSpace->freePtr = ((vm->type & VMANAGER_STACK) ? (vSpace->top) : (vSpace->base));

    vSpace->next = vm->vSpaceList;
    if(vm->vSpaceList)
    {
        vm->vSpaceList->prev = vSpace;
    }

    vm->vSpaceList = vSpace;

    Kunlock(&vm->lock, &status);

    return vSpace;
}

/**
 * vSpaceListRemove Implementation (See header file for description)
*/
void vSpaceListRemove(vSpace_t *vSpace)
{
    if(vSpace->prev)
    {
        vSpace->prev->next = vSpace->next;
    }
    else
    {
        vSpace->owner->vSpaceList = vSpace->next;
    }
    if(vSpace->next)
    {
        vSpace->next->prev = vSpace->prev;
    }

    vSpace->next = NULL;
    vSpace->prev = NULL;
}

/**
 * vSpaceRelease Implementation (See header file for description)
*/
int32_t vSpaceRelease(vSpace_t *vSpace)
{
    if(!vSpace)
    {
        return E_INVAL;
    }
    vManager_t *vm = vSpace->owner;

    uint32_t status;
    Klock(&vm->lock, &status);

    vSpaceListRemove(vSpace);

    (void)vSpaceUnmap(vSpace);

    MemoryBlockFree(vm->vSpacePool, vSpace->base, (uint32_t)((uint32_t)vSpace->top - (uint32_t)vSpace->base));

    Kunlock(&vm->lock, &status);

    kfree(vSpace, sizeof(*vSpace));

    return E_OK;
}

/**
 * vSpaceMap Implementation (See header file for description)
*/
vaddr_t vSpaceMap(vSpace_t *vSpace, paddr_t pAddr, ulong_t mapType, memCfg_t *memcfg)
{
    if(!vSpace && vSpace->vPageList)
    {
        return NULL;
    }

    vPage_t *vPage =vPageMap(
    			&vSpace->owner->lock,
                vSpace->owner->pgt,
                pAddr,
                vSpace->base,
                (uint32_t)((uint32_t)vSpace->top - (uint32_t)vSpace->base),
				mapType,
				memcfg);

    if(!vPage)
    {
        return NULL;
    }

    vaddr_t vAddr = vSpace->freePtr;
    vSpace->freePtr = ((vSpace->owner->type & VMANAGER_STACK) ? (vSpace->base) : (vSpace->top));
    vSpace->vPageList = vPage;

    return vAddr;
}

/**
 * vSpaceUnmap Implementation (See header file for description)
*/
vaddr_t vSpaceUnmap(vSpace_t *vSpace)
{
    if(!vSpace && vSpace->vPageList)
    {
        return NULL;
    }

    vManager_t *vm = vSpace->owner;

    while(vSpace->vPageList)
    {
        vPage_t *vPage = vSpace->vPageList;
        vSpace->vPageList = vPage->next;
        vPageUnmap(&vm->lock, vm->pgt, vPage, vm->arg, vm->unmapHandler);
    }

    // Make sure we clean all entries in the L1 page table
    MemoryUnmap(vm->pgt,vSpace->base,(vSpace->top - vSpace->base));

    vSpace->freePtr = ((vm->type & VMANAGER_STACK) ? (vSpace->top) : (vSpace->base));

    return vSpace->freePtr;
}

/**
 * vSpaceMapSection Implementation (See header file for description)
*/
vaddr_t vSpaceMapSection(vSpace_t *vSpace, paddr_t pAddr, size_t size, ulong_t mapType, memCfg_t *memcfg)
{
    if(!vSpace)
    {
        return NULL;
    }

    vaddr_t vAddr = NULL;
    vaddr_t retAddr = vSpace->freePtr;
    vaddr_t newFreePtr = NULL;
    size_t mapSize = ROUND_UP(size + ((uint32_t)pAddr & (PAGE_SIZE - 1)), PAGE_SIZE);

    if(vSpace->owner->type == VMANAGER_STACK)
    {
        // Check if we have enough free space
        if(((uint32_t)vSpace->freePtr - (uint32_t)vSpace->base) < mapSize)
        {
            return NULL;
        }
        // Update the amount of free reserved memory
        newFreePtr = vSpace->freePtr - mapSize;
        vAddr = newFreePtr;
    }
    else
    {
        // Check if we have enough free space
        if(((uint32_t)vSpace->top - (uint32_t)vSpace->freePtr) < mapSize)
        {
            return NULL;
        }
        // Update the amount of free reserved memory
        newFreePtr = vSpace->freePtr + mapSize;
        vAddr = vSpace->freePtr;
    }
    vPage_t *vPage = vPageMap(
    			&vSpace->owner->lock,
                vSpace->owner->pgt,
                pAddr,
                vAddr,
				mapSize,
				mapType,
				memcfg);
    if(!vPage)
    {
        return NULL;
    }

    vSpace->freePtr = newFreePtr;
    vPage->next = vSpace->vPageList;
    vSpace->vPageList = vPage;

    return retAddr;
}

/**
 * vSpaceUnmapSection Implementation (See header file for description)
*/
vaddr_t vSpaceUnmapSection(vSpace_t *vSpace, size_t size)
{
    if(!vSpace)
    {
        return NULL;
    }

    vManager_t *vm = vSpace->owner;
    size_t unmapSize = ROUND_UP(size, PAGE_SIZE);

    while(unmapSize && vSpace->vPageList)
    {
        vPage_t *vPage = vSpace->vPageList;

        if(vPage->size > unmapSize){
            // There is no way to cut a physical memory block
            // at the moment!!!
            // Possible solution:
            // - if there is an available unmap handler we can use it
            //   to handle the physical memory
            //   Add a unmap with range!
            return NULL;
        }
        else
        {
            unmapSize -= vPage->size;
            vSpace->freePtr =   ((vSpace->owner->type == VMANAGER_STACK) ?
                                (vSpace->freePtr + vPage->size) :
                                (vSpace->freePtr - vPage->size));
            vSpace->vPageList = vPage->next;
            vPageUnmap(&vm->lock, vm->pgt, vPage, vm->arg, vm->unmapHandler);
        }
    }
    return vSpace->freePtr;
}

/**
 * vMap Implementation (See header file for description)
*/
vaddr_t vMap(vManager_t *vm, paddr_t pAddr, size_t size, ulong_t mapType, memCfg_t *memcfg)
{
	if(!vm)
	{
		return NULL;
	}

	vaddr_t vAddr = (vaddr_t)MemoryBlockAlloc(vm->vSpacePool, size);

	if(vAddr != NULL)
	{
		vPage_t *page = vPageMap(&vm->lock, vm->pgt, pAddr, vAddr, size, mapType, memcfg);

		if(page != NULL)
		{
			page->next = vm->fixedPages;
			vm->fixedPages = page;
		}
		return page->vAddr;
	}
	else
	{
		return NULL;
	}
}

/**
 * vUnmap Implementation (See header file for description)
*/
int32_t vUnmap(vManager_t *vm, vaddr_t vaddr)
{
	if(!vm)
	{
		return NULL;
	}

	vPage_t *page = vm->fixedPages;
	vPage_t *prev = NULL;

	while((page != NULL) && page->vAddr != vaddr)
	{
		prev = page;
		page = page->next;
	}

	if(page != NULL)
	{
		if(prev != NULL)
		{
			prev->next = page->next;
		}
		else
		{
			vm->fixedPages = page->next;
		}

		MemoryBlockFree(vm->vSpacePool, page->vAddr, page->size);

		return vPageUnmap(&vm->lock, vm->pgt, page, vm->arg, vm->unmapHandler);
	}
	else
	{
		return E_ERROR;
	}
}
