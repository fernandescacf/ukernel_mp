/**
 * @file        allocator.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        02 April, 2020
 * @brief       Fixed size allocator implementation
*/

/* Includes ----------------------------------------------- */
#include <allocator.h>
#include <kheap.h>
#include <string.h>
#include <misc.h>


/* Private types ------------------------------------------ */
typedef struct objEntry
{
    struct objEntry *next;
}objEntry_t;


/* Private constants -------------------------------------- */
#define ALLOCATOR_MIN_SIZE      (sizeof(uint32_t))


/* Private macros ----------------------------------------- */
#define ID_TO_ADDR(alloc, id)   ((void*)(id * (alloc)->allocSize + (uint32_t)(alloc)->data))
#define ADDR_TO_ID(alloc, addr) (((uint32_t)addr - (uint32_t)(alloc)->data) / (alloc)->allocSize)


/* Private variables -------------------------------------- */


/* Private function prototypes ---------------------------- */

allocator_t *AllocatorExpand(allocator_t *parent);

void *AllocatorGetObj(allocator_t *allocator, int32_t baseId, int32_t *id);

allocator_t *AllocatorPurge(allocator_t *allocator);


/* Private functions -------------------------------------- */

/**
 * AllocatorInit Implementation (See header file for description)
*/
int32_t AllocatorInit(allocator_t *allocator, uint16_t size, uint16_t allocSize, uint32_t flags)
{
    // Ensure that we only allocate block aligned to the minimum size allowed
    allocator->allocSize = ROUND_UP(allocSize, ALLOCATOR_MIN_SIZE);
    // Compute number of available memory objects
    allocator->maxId = allocator->nfree = size / allocator->allocSize;
    // Ensure that we have a total size multiple of the allocation size
    allocator->size = allocator->nfree * allocator->allocSize;

    allocator->next = NULL;
    allocator->flags = flags;
    allocator->data = (void*)kmalloc(allocator->size);

    if(allocator->data == NULL)
    {
        return E_NO_MEMORY;
    }

    allocator->free = allocator->data;

    if(allocator->flags | ALLOCATOR_CLEAN_MEMORY)
    {
        memset(allocator->data, 0x0, allocator->size);
    }
    else
    {
        ((objEntry_t*)allocator->free)->next = NULL;
    }

    RWlockInit(&allocator->lock);

    return E_OK;
}

/**
 * AllocatorExpand Implementation (See Private function prototypes file for description)
*/
allocator_t *AllocatorExpand(allocator_t *parent)
{
    allocator_t *allocator = (allocator_t *)kmalloc(sizeof(allocator_t));

    if(AllocatorInit(allocator, parent->size, parent->allocSize, parent->flags) != 0)
    {
        kfree(allocator, sizeof(*allocator));
        return NULL;
    }
    return allocator;
}

/**
 * AllocatorGetObj Implementation (See Private function prototypes file for description)
*/
void *AllocatorGetObj(allocator_t *allocator, int32_t baseId, int32_t *id)
{
	uint32_t status;
	WriteLock(&allocator->lock, &status);

    if(allocator->nfree == 0)
    {
        if(!(allocator->flags & ALLOCATOR_ALLOW_EXPAND))
        {
        	WriteUnlock(&allocator->lock, &status);
            return NULL;
        }

        if(allocator->next == NULL)
        {
            allocator->next = AllocatorExpand(allocator);
        }

        WriteUnlock(&allocator->lock, &status);

        return AllocatorGetObj(allocator->next, baseId + allocator->maxId, id);
    }

    void *obj = allocator->free;
    objEntry_t *entry = (objEntry_t*)allocator->free;

    allocator->nfree--;

    if(entry->next != NULL)
    {
        allocator->free = (void*)entry->next;
        *((uint32_t*)obj) = 0x0;
    }
    else
    {
        if(allocator->nfree > 0)
        {
            allocator->free = (void*)((uint8_t*)allocator->free + allocator->allocSize);
        }
        else
        {
            allocator->free = NULL;
        }
    }

    WriteUnlock(&allocator->lock, &status);

    if(id)
    {
        *id = baseId + ADDR_TO_ID(allocator, obj);
    }

    return obj;
}

/**
 * AllocatorGet Implementation (See header file for description)
*/
void *AllocatorGet(allocator_t *allocator, int32_t *id)
{
    return AllocatorGetObj(allocator, 0, id);
}

/**
 * AllocatorPurge Implementation (See Private function prototypes file for description)
*/
allocator_t *AllocatorPurge(allocator_t *allocator)
{
	uint32_t status;
	WriteLock(&allocator->lock, &status);

    if(allocator->nfree == allocator->maxId)
    {
        allocator_t *temp = allocator->next;
        kfree(allocator->data, allocator->size);
        kfree(allocator, sizeof(*allocator));
        allocator = temp;
    }

    WriteUnlock(&allocator->lock, &status);

    return allocator;
}

/**
 * AllocatorFree Implementation (See header file for description)
*/
int32_t AllocatorFree(allocator_t *allocator, void *obj)
{
	uint32_t status;
	WriteLock(&allocator->lock, &status);

    if(obj > (void*)((uint8_t*)allocator->data + allocator->size))
    {
        if(allocator->next != NULL)
        {
            int32_t ret = AllocatorFree(allocator->next, obj);

            if(allocator->flags & ALLOCATOR_PURGE)
            {
                allocator->next = AllocatorPurge(allocator->next);
            }

            WriteUnlock(&allocator->lock, &status);

            return ret;
        }

        WriteUnlock(&allocator->lock, &status);

        return -1;
    }

    if(allocator->flags & ALLOCATOR_CLEAN_MEMORY)
    {
        memset(obj, 0x0, allocator->allocSize);
    }

    objEntry_t *entry = (objEntry_t*)obj;
    entry->next = (objEntry_t*)allocator->free;
    allocator->free = obj;
    allocator->nfree++;

    WriteUnlock(&allocator->lock, &status);

    return 0;
}

/**
 * AllocatorFreeId Implementation (See header file for description)
*/
int32_t AllocatorFreeId(allocator_t *allocator, int32_t id)
{
    void *obj = AllocatorToAddr(allocator, id);

    return AllocatorFree(allocator, obj);
}

/**
 * AllocatorToAddr Implementation (See header file for description)
*/
void *AllocatorToAddr(allocator_t *allocator, int32_t id)
{
    if(id >= allocator->maxId)
    {
        if(allocator->next != NULL)
        {
            return AllocatorToAddr(allocator->next, id - allocator->maxId);
        }

        return NULL;
    }

    return ID_TO_ADDR(allocator, id);
}

/**
 * AllocatorToId Implementation (See header file for description)
*/
int32_t AllocatorToId(allocator_t *allocator,  void *obj)
{
    if(obj == NULL)
    {
        return -1;
    }

    if(obj > (void*)((uint8_t*)allocator->data + allocator->size))
    {
        if(allocator->next != NULL)
        {
            return AllocatorToId(allocator->next, obj);
        }

        return -1;
    }

    return ADDR_TO_ID(allocator, obj);
}

/**
 * AllocatorDestroy Implementation (See header file for description)
*/
void AllocatorDestroy(allocator_t *allocator)
{
	uint32_t status;
	WriteLock(&allocator->lock, &status);

    allocator_t *next = allocator->next;

    while(next != NULL)
    {
        allocator_t *temp = next->next;
        kfree(next->data, next->size);
        kfree(next, sizeof(*next));
        next = temp;
    }

    WriteUnlock(&allocator->lock, &status);

    kfree(allocator->data, allocator->size);
    memset(allocator, 0x0, sizeof(allocator_t));
    // We do not own allocator so we only free what we had allocated
}
