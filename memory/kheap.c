/**
 * @file        kheap.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        16 November, 2020
 * @brief       Kernel Heap source file
*/

/* Includes ----------------------------------------------- */
#include <kheap.h>
#include <memmgr.h>
#include <klock.h>


/* Private types ------------------------------------------ */

typedef struct HeapBlock heapBlock_t;

struct HeapBlock
{
    size_t       size;
    heapBlock_t* next;
};

/* Private constants -------------------------------------- */


/* Private macros ----------------------------------------- */


/* Private variables -------------------------------------- */

static struct
{
    size_t       size;
    size_t       grow;
    klock_t      lock;
    heapBlock_t* blocks;
}kheap;


/* Private function prototypes ---------------------------- */

uint32_t msb32(register uint32_t x)
{
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    return(x & ~(x >> 1));
}

ptr_t HeapGrow(size_t size)
{
    ptr_t ptr = MemoryGet(size, ZONE_DIRECT);

    if(ptr != NULL) kheap.size += size;

    return ptr;
}

void BlockMerge(heapBlock_t* block, heapBlock_t* next)
{
    if(((uint32_t)block + block->size) == (uint32_t)next)
    {
        block->size += next->size;
        block->next = next->next;
    }
}

bool_t BlockExpand(heapBlock_t* block, ptr_t addr, size_t size)
{
    if(((uint32_t)block + block->size) == (uint32_t)addr)
    {
        block->size += size;
        if(block->next != NULL) BlockMerge(block, block->next);
        return TRUE;
    }

    return FALSE;
}

void HeapBlockInsert(ptr_t addr, size_t size, heapBlock_t* test)
{
    if((test != NULL) && ((ptr_t)test < addr))
    {
        // Try Merge
        if(BlockExpand(test, addr, size) == TRUE)
        {
            return ;
        }

        heapBlock_t* block = (heapBlock_t*)addr;
        block->size = size;
        block->next = NULL;
        test->next = block;
        return ;
    }

    if(kheap.blocks == NULL)
    {
        kheap.blocks = (heapBlock_t*)addr;
        kheap.blocks->next = NULL;
        kheap.blocks->size = size;
        return ;
    }

    heapBlock_t* it = kheap.blocks;
    heapBlock_t* prev = NULL;

    while((it != NULL) && ((uint32_t)it < (uint32_t)addr))
    {
        prev = it;
        it = it->next;
    }

    if(it == NULL)
    {
        // Try Merge
        if(BlockExpand(prev, addr, size) == TRUE)
        {
            return ;
        }

        prev->next = (heapBlock_t*)addr;
        prev->next->size = size;
        prev->next->next = NULL;
    }
    else
    {
        if((prev != NULL) && (BlockExpand(prev, addr, size) == TRUE))
        {
            return ;
        }

        heapBlock_t* temp = (heapBlock_t*)addr;
        temp->size = size;
        temp->next = it;

        if(prev == NULL) kheap.blocks = temp;
        else prev->next = temp;

        BlockMerge(temp, it);
    }
}


/* Private functions -------------------------------------- */

int32_t kheapInit(size_t size, size_t grow)
{
    kheap.size = 0;
    kheap.grow = grow;
    kheap.blocks = NULL;
    kheap.blocks = (heapBlock_t*)HeapGrow(size);
    kheap.blocks->size = kheap.size;
    kheap.blocks->next = NULL;

    KlockInit(&kheap.lock);

    return E_OK;
}

ptr_t kmalloc(size_t size)
{
	if(kheap.grow == 0) return NULL;

    if(size < sizeof(heapBlock_t))
    {
        size = sizeof(heapBlock_t);
    }
    else if(size & 0x03)
    {
    	// Ensure 4 bit alignment
    	size = ((size & 0xFFFFFFFC) + 0x4);
    }

    uint32_t status;
    Klock(&kheap.lock, &status);

    heapBlock_t* it = kheap.blocks;
    heapBlock_t* prev = NULL;

//    while((it != NULL) && (it->size < (size + sizeof(heapBlock_t))))
    while((it != NULL) && (it->size < size))
    {
        prev = it;
        it = it->next;
    }

    if(it == NULL)
    {
        // No big enough block available, try fetch more memory
        size_t grow = kheap.grow;

        if(size > (kheap.grow - sizeof(heapBlock_t)))
        {
            grow = msb32(size << 1);
        }

        ptr_t ptr = HeapGrow(grow);

        if(ptr == NULL)
        {
        	Kunlock(&kheap.lock, &status);

        	return NULL;
        }

        HeapBlockInsert(ptr, (grow - size), prev);

        Kunlock(&kheap.lock, &status);

        return (ptr_t)((uint32_t)ptr + (grow - size));
    }

    if(it->size < (size + sizeof(heapBlock_t)))
    {
        if(prev == NULL) kheap.blocks = it->next;
        else prev->next = it->next;

        Kunlock(&kheap.lock, &status);

        return (ptr_t)it;
    }
    else
    {
        it->size -= size;

        ptr_t addr = (ptr_t)((uint32_t)it + it->size);

        Kunlock(&kheap.lock, &status);

        return addr;
    }
}

void kfree(ptr_t ptr, size_t size)
{
    if(size < sizeof(heapBlock_t))
    {
        size = sizeof(heapBlock_t);
    }
    else if(size & 0x03)
	{
		// Ensure 4 bit alignment
		size = ((size & 0xFFFFFFFC) + 0x4);
	}

    uint32_t status;
    Klock(&kheap.lock, &status);

    HeapBlockInsert(ptr, size, NULL);

    Kunlock(&kheap.lock, &status);
}
