/**
 * @file        vector.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        12 January, 2020
 * @brief       Kernel Vector implementation
*/

/* Includes ----------------------------------------------- */
#include <vector.h>
#include <string.h>
#include <kheap.h>


/* Private types ------------------------------------------ */



/* Private constants -------------------------------------- */
#define VECTOR_MAX_SIZE			(0xffff-1)
#define VECTOR_MIN_SIZE			(0x8)
#define INVALIDE_ENTRY          ((entry_t *)0x1)


/* Private macros ----------------------------------------- */

static inline entry_t *EntryGetNext(entry_t *entry)
{
	return (entry_t *)((uint32_t)entry->next & ~0x1);
}

static inline entry_t *EntryGet(entry_t *entry)
{
	return (entry_t *)((uint32_t)entry & ~0x1);
}

static inline entry_t *EntrySet(void *ptr)
{
	return (entry_t *)((uint32_t)ptr | 0x1);
}

static inline entry_t *EntryAdjust(entry_t *entry, int32_t offset)
{
	return (entry_t *)((int32_t)entry + offset);
}

static inline entry_t *Ptr2Entry(void *ptr)
{
	return (entry_t *)(ptr);
}

static inline void *GetPtr(entry_t *entry)
{
	return (void *)((uint32_t)entry & ~0x1);
}

/* Private variables -------------------------------------- */


/* Private function prototypes ---------------------------- */

static int32_t VectorExpand(vector_t *vector, uint32_t size);

/* Private functions -------------------------------------- */

/**
 * VectorInit Implementation (See header file for description)
*/
int32_t VectorInit(vector_t *vector, uint32_t size)
{
    memset(vector, 0x0, sizeof(vector_t));

    if(size > VECTOR_MAX_SIZE)
    {
        size = VECTOR_MAX_SIZE;
    }

    if(size < VECTOR_MIN_SIZE)
    {
        size = VECTOR_MIN_SIZE;
    }

    vector->data = (void**)kmalloc((size + 1) * sizeof(void *));

    vector->free = Ptr2Entry(vector->data);

    vector->size = vector->nfree = (size + 1);

    entry_t *entry = vector->free;
    int i;
    void **ptr = vector->data;

    ++ptr;

    for(i = 0 ; i < (vector->size - 1); ptr++, i++)
    {
    	entry->next = EntrySet(ptr);
        entry = EntryGetNext(entry);
    }

    entry->next = EntrySet(NULL);

    return E_OK;
}

/**
 * VectorExpand Implementation (See header file for description)
*/
int32_t VectorExpand(vector_t *vector, uint32_t size)
{
    if(vector->size == VECTOR_MAX_SIZE)
    {
        return -1;
    }

    uint32_t newSize;

    if(vector->size < size)
    {
        newSize = (size + 1);
    }
    else
    {
        newSize = vector->size + VECTOR_MIN_SIZE;
    }

    if(newSize > VECTOR_MAX_SIZE)
    {
        newSize = VECTOR_MAX_SIZE;
    }

    void *newData = kmalloc(newSize * sizeof(void *));

    void *oldData = (void*)vector->data;

    memcpy(newData, oldData, vector->size * sizeof(void *));

    entry_t *prev = NULL;

    // If we have free entries we will need to adjust the pointer
    if(vector->free != NULL)
    {
    	int32_t offset = (int)newData - (int)oldData;
        vector->free = EntryAdjust(vector->free, offset);

        entry_t *cur = EntryGet(vector->free);
        for( ; cur->next != EntrySet(NULL); cur = EntryGetNext(cur))
        {
            cur->next = EntryAdjust(cur->next, offset);
        }
        prev = EntryGet(cur);
    }
    else
    {
        vector->free = Ptr2Entry(&((void**)newData)[vector->size]);
        prev = EntryGet(vector->free);
    }

    vector->data = (void**)newData;

    kfree(oldData, vector->size);

    int32_t i = 0;

    entry_t *entry = Ptr2Entry(&vector->data[vector->size]);

    // If prev is equal to entry we already have free pointing to the
    // first slot in the expanded part of the vector
    if(entry == prev)
    {
        entry++;
        i = 1;
    }

    for(i = 0; i < (newSize - vector->size - 1); i++)
    {
        prev->next = EntrySet((void*)entry);
        prev = EntryGetNext(prev);
        entry++;
    }
    prev->next = EntrySet(NULL);

    vector->nfree += newSize - vector->size;
    vector->size = newSize;

    return E_OK;
}

/**
 * VectorInsert Implementation (See header file for description)
*/
int32_t VectorInsert(vector_t *vector, void *obj)
{
    if((unsigned)obj & 0x3)
    {
        return -1;
    }

    if(vector->free != NULL)
    {
        void **ptr = (void**)GetPtr(vector->free);
        vector->free = EntryGetNext(vector->free);
        *ptr = obj;
        vector->nfree--;
        return ptr - vector->data;
    }

    if(VectorExpand(vector, 0) == 0)
    {
        return VectorInsert(vector, obj);
    }

    return -1;
}

/**
 * VectorInsertAt Implementation (See header file for description)
*/
int32_t VectorInsertAt(vector_t *vector, void *obj, uint32_t index)
{
	if((unsigned)obj & 0x3)
	{
		return -1;
	}

	entry_t *cur = EntryGet(vector->free);
	entry_t *prev = NULL;
	for( ; EntryGet(cur) != NULL; cur = EntryGetNext(cur))
	{
		uint32_t at = cur - Ptr2Entry(&vector->data[0]);
		if(index <= at)
		{
			if(EntryGet(prev) == NULL)
			{
				vector->free = EntryGet(cur->next);
			}
			else
			{
				prev->next = EntrySet(EntryGetNext(cur));
			}

			*((void**)GetPtr(cur)) = obj;
			vector->nfree--;

			return at;
		}
		prev = EntryGet(cur);
	}

	if(VectorExpand(vector, 0) == 0)
	{
		return VectorInsertAt(vector, obj, index);
	}

	return -1;
}

/**
 * VectorRemove Implementation (See header file for description)
*/
void *VectorRemove(vector_t *vector, unsigned index)
{
    if(vector == NULL || vector->size < index)
    {
        return NULL;
    }

    void *ptr = vector->data[index];

    // Check if index is already free
    if(((unsigned)ptr) & 0x1)
    {
        return NULL;
    }

    entry_t *entry = Ptr2Entry(&vector->data[index]);

    entry->next = EntrySet((void*)vector->free);
    vector->free = EntryGet(entry);
    vector->nfree++;

    return ptr;
}

/**
 * VectorPeek Implementation (See header file for description)
*/
void *VectorPeek(vector_t *vector, uint32_t index)
{
	if(vector == NULL || vector->size < index)
	{
		return NULL;
	}

	return (((uint32_t)vector->data[index] & 0x1) ? (NULL): (vector->data[index]));
}

/**
 * VectorUsage Implementation (See header file for description)
*/
uint32_t VectorUsage(vector_t *vector)
{
	return (vector->size - vector->nfree);
}

/**
 * VectorSize Implementation (See header file for description)
*/
uint32_t VectorSize(vector_t *vector)
{
	return (uint32_t)vector->size;
}

/**
 * VectorFree Implementation (See header file for description)
*/
void VectorFree(vector_t *vector)
{
	if(vector->data)
	{
		kfree(vector->data, vector->size);
	}
	memset(vector, 0x0, sizeof(vector_t));
}
