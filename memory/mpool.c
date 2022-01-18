/**
 * @file        mpool.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        30 March, 2016
 * @brief       Memory Pool Implementation
*/


/* Includes ----------------------------------------------- */
#include <mpool.h>
#include <misc.h>
#include <kheap.h>


/* Private types ------------------------------------------ */



/* Private constants -------------------------------------- */

#define MPOOL_NINSIZE			(sizeof(pmm_t))
#define MPOOL_DEFUALALIGN		(0x4)
#define MPOOL_DEFAULFLAGS		(MPOOL_FREEALLOCATION | MPOOL_ALIGNCHECK | MPOOL_INTEGRATEHDR)

#define MPOOL_PRIVATE_FLAGS(x)	(x << 16)
#define	MPOOL_PFREE				MPOOL_PRIVATE_FLAGS((0x1 << 0))
#define MPOOL_PHALF				MPOOL_PRIVATE_FLAGS((0x1 << 1))
#define MPOOL_PFULL				MPOOL_PRIVATE_FLAGS((0x1 << 2))
#define MPOOL_REMOTEHDR			MPOOL_PRIVATE_FLAGS((0x1 << 8))

#define MPOOL_POINTERS_OFFSET   (sizeof(void*)*2)

/* Private macros ----------------------------------------- */

#define MPOOL_THRESHOLD(obj)	((obj << 5) + sizeof(mPool_t))
#define MPOOL_ALIGNMENT(mpool)	((mpool)->size & (-((mpool)->size)))

#define ALLOC_MPOOL(offset)		((mPool_t*)kmalloc(sizeof(mPool_t) + offset))
#define DEALLOC_MPOOL(pool)		(kfree(pool, sizeof(*pool)))
#define ALLOC_PMM()				((pmm_t*)kmalloc(sizeof(pmm_t)))
#define DEALLOC_PMM(pmm)		(kfree(pmm, sizeof(*pmm)))

#define LIST_GET_HEAD(head,pmm)                 \
    do{                                         \
        (pmm) = (head);                         \
        (head) = (((head) != (head)->next) ?	\
				 (head)->next :					\
				 NULL);                  		\
        if((head)){                             \
            (head)->prev = NULL;                \
        }                                       \
    }while(0)

#define LIST_INSERT(head,pmm)                           \
    do{                                                 \
        if(!head){                                      \
            head = pmm;                                 \
            pmm->next = pmm->prev = NULL;               \
        }                                               \
        else{                                           \
            pmm_t *__pmm = head;                        \
            while(__pmm && __pmm->addr > pmm->addr){    \
                if(!__pmm->next){                       \
                    __pmm->next = pmm;                  \
                    pmm->prev = __pmm;                  \
                    pmm->next = NULL;                   \
                    __pmm = NULL;                       \
                }                                       \
                else{                                   \
                    __pmm = __pmm->next;                \
                }                                       \
            }                                           \
            if(__pmm){                                  \
                pmm->next = __pmm;                      \
                pmm->prev = __pmm->prev;                \
                __pmm->prev = pmm;                      \
                if(pmm->prev){pmm->prev->next = pmm;}   \
                else {head = pmm;}                      \
            }                                           \
        }                                               \
    }while(0)

/* Private variables -------------------------------------- */


/* Private function prototypes ---------------------------- */

void pmmInsert(pmm_t **head, ptr_t addr, uint32_t size, bool_t mapped);

pmm_t *pmmGet(pmm_t **head, uint32_t size, bool_t mapped);

/* Private functions -------------------------------------- */

/**
 * mPoolCreate Implementation (See header file for description)
*/
mPool_t *mPoolCreate(ptr_t start, ptr_t end, size_t size, uint32_t align, ulong_t flags)
{
    // Ensure that the size is at least the minimum size allowed
    uint32_t allocMinSize = ((size < MPOOL_NINSIZE) ? (MPOOL_NINSIZE) : (size));
    // Align all the start and end addresses
    ptr_t alignStart = (ptr_t)ROUND_DOWN((uint32_t)start,align);
    ptr_t alignEnd = (ptr_t)ROUND_UP((uint32_t)end,align);

    // If required ensure that all allocated memory respects the specified alignment
    if(flags & MPOOL_ALIGNCHECK){
        allocMinSize = ROUND_UP(allocMinSize,align);
    }

    // If allocation size is bigger than the available memory return an error
    if(((uint32_t)alignEnd - (uint32_t)alignStart) < allocMinSize){
        return NULL;
    }

    // If the memory pool is configured to allow different allocation sizes
    // the size represents the memory alignment
    allocMinSize = ((flags & MPOOL_FREEALLOCATION) ? (align) : (allocMinSize));

    // Allocate the memory pool structure
    mPool_t *mpool;
    uint32_t offset = ((flags & MPOOL_USEPOINTERS) ? MPOOL_POINTERS_OFFSET : 0);
    if(!(flags & MPOOL_UNMAPPED)){
        // If the memory used by this pool is mapped we can see if the mpool structure
        // fits in one of the cut blocks
        if((start - alignStart) >= (sizeof(mPool_t) + offset)){
            mpool = (mPool_t*)start;
        }
        else if((flags & MPOOL_INTEGRATEHDR) || ((end - alignStart) >= MPOOL_THRESHOLD(allocMinSize))){
            // If the memory available passes the threshold value or it is specified in
            // the control flags allocate the mpool structure at the end of the memory pool
            alignEnd = (alignEnd - (sizeof(mPool_t) + offset));
            mpool = (mPool_t*)alignEnd;
            // NOTE: the header integration (mPool_t structure) is only checked to be performed
            // at the end address so that it doesn't messes up the alignments
        }
        else{
            // If we come here there is no other solution than to allocate the mpool structure
            // from the kernel memory allocators
            mpool = ALLOC_MPOOL(offset);
            // Reminder that the memory pool structure isn't contained in the memory block
            flags |= MPOOL_REMOTEHDR;
        }
    }
    else{
        // If the memory is not mapped we have no choice but to allocated memory from the
        // kernel memory allocators
        mpool = ALLOC_MPOOL(offset);
        // Reminder that the memory pool structure isn't contained in the memory block
        flags |= MPOOL_REMOTEHDR;
    }

    // Check if we didn't run out of memory
    if(mpool == NULL){
        return NULL;
    }

    // Memory pool initialization
    mpool->start = alignStart;
    mpool->end = alignEnd;
    // Pointer to the beginning of the available memory
    mpool->free = alignStart;
    // Memory size already aligned
    mpool->size = allocMinSize;
    // Save control flags
    mpool->flags = (flags | MPOOL_PFREE);
    // Initialize the physical memory blocks list used to store the deallocated memory blocks
    // that can not be added to the memory pool right away
    mpool->mHead = NULL;
    // If free allocation is enable insert the memory pool in the memory objects list
    if(flags & MPOOL_FREEALLOCATION){
        pmmInsert(&mpool->mHead, mpool->free, (uint32_t)(mpool->end - mpool->free), !(flags & MPOOL_UNMAPPED));
    }
    // Return memory pool
    return mpool;
}

/**
 * mPoolInit Implementation (See header file for description)
*/
int32_t mPoolInit(mPool_t *mPool, ptr_t start, ptr_t end, size_t size, uint32_t align, ulong_t flags)
{
    if(!mPool){
        return 1;
    }

    // Ensure that the size is at least the minimum size allowed
    uint32_t allocMinSize = ((size < MPOOL_NINSIZE) ? (MPOOL_NINSIZE) : (size));
    // Align all the start and end addresses
    ptr_t alignStart = (ptr_t)ROUND_UP((uint32_t)start,align);
    ptr_t alignEnd = (ptr_t)ROUND_DOWN((uint32_t)end,align);

    // If required ensure that all allocated memory respects the specified alignment
    if(flags & MPOOL_ALIGNCHECK){
        allocMinSize = ROUND_UP(allocMinSize,align);
    }

    // If allocation size is bigger than the available memory return an error
    if(((uint32_t)alignEnd - (uint32_t)alignStart) < allocMinSize){
        return NULL;
    }

    // If the memory pool is configured to allow different allocation sizes
    // the size represents the memory alignment
    allocMinSize = ((flags & MPOOL_FREEALLOCATION) ? (align) : (allocMinSize));

    // Memory pool initialization
    mPool->start = alignStart;
    mPool->end = alignEnd;
    // Pointer to the beginning of the available memory
    mPool->free = alignStart;
    // Memory size already aligned
    mPool->size = allocMinSize;
    // Save control flags
    mPool->flags = (flags | MPOOL_PFREE);
    // Initialize the physical memory blocks list used to store the deallocated memory blocks
    // that can not be added to the memory pool right away
    mPool->mHead = NULL;
    // If free allocation is enable insert the memory pool in the memory objects list
    if(flags & MPOOL_FREEALLOCATION){
        pmmInsert(&mPool->mHead, mPool->free, (uint32_t)(mPool->end - mPool->free), !(flags & MPOOL_UNMAPPED));
    }

    return 0;
}

/**
 * mPoolDestroy Implementation (See header file for description)
*/
int32_t mPoolDestroy(mPool_t *mPool)
{

    if(mPool->flags & MPOOL_UNMAPPED){
        while(mPool->mHead){
            pmm_t *pmm;
            LIST_GET_HEAD(mPool->mHead,pmm);
            DEALLOC_PMM(pmm);
        }
    }

    if(mPool->flags & MPOOL_REMOTEHDR){
        DEALLOC_MPOOL(mPool);
    }
    return 0;
}

/**
 * pmmInsert Implementation (See header file for description)
*/
void pmmInsert(pmm_t **head, ptr_t addr, uint32_t size, bool_t mapped)
{
    if(!(*head)){
        pmm_t *pmm = ((mapped) ? ((pmm_t*)addr) : (ALLOC_PMM()));
        pmm->addr = addr;
        pmm->size = size;
        pmm->next = pmm;
        pmm->prev = pmm;
        *head = pmm;
    }
    else{
        pmm_t *iterator = (*head);
        do{
            if(iterator->addr > addr){
                break;
            }
            else{
                iterator = iterator->next;
            }
        }while(iterator != *head);

        bool_t skip = FALSE;

        if(((ptr_t)((uint32_t)iterator->prev->addr + iterator->prev->size) == addr)){
            iterator->prev->size += size;
            skip = TRUE;
        }
        if(iterator->addr == (ptr_t)((uint32_t)addr + size)){
            pmm_t *pmm;
            if(mapped){
                if(skip){
                    pmm = iterator->prev;
                    pmm->size += iterator->size;
                    pmm->next = iterator->next;
                    iterator->next->prev = pmm;
                }
                else{
                    pmm = ((pmm_t*)addr);
                    pmm->size = iterator->size + size;
                    pmm->addr = addr;
                    if(iterator->next == iterator){
                        pmm->next = pmm;
                        pmm->prev = pmm;
                    }
                    else{
                        pmm->next = iterator->next;
                        pmm->prev = iterator->prev;
                        iterator->prev->next = pmm;
                        iterator->next->prev = pmm;
                    }
                }
            }
            else{
				if(skip){
					pmm = iterator->prev;
					pmm->size += iterator->size;
					pmm->next = iterator->next;
					iterator->next->prev = pmm;
					DEALLOC_PMM(iterator);
				}
				else{
					pmm = iterator;
					pmm->addr = addr;
					pmm->size += size;
				}
			}
            if(*head == iterator){
                *head = pmm;
            }
            skip = TRUE;
        }
        if(!skip){
            pmm_t *pmm = ((mapped) ? ((pmm_t*)addr) : (ALLOC_PMM()));
            pmm->addr = addr;
            pmm->size = size;
            if(*head == iterator){
                *head = pmm;
            }
            pmm->prev = iterator->prev;
            iterator->prev->next = pmm;
            iterator->prev = pmm;
            pmm->next = iterator;
        }
    }
}

/**
 * pmmSearch Implementation (See header file for description)
*/
pmm_t *pmmSearch(pmm_t **head, uint32_t size)
{

    if(!(*head)){
        return NULL;
    }

    pmm_t *iterator = *head;

    while(iterator->size < size){
        iterator = iterator->next;
        if(iterator == *head){
            return NULL;
        }
    }

    return iterator;
}

/**
 * pmmGet Implementation (See header file for description)
*/
pmm_t *pmmGet(pmm_t **head, uint32_t size, bool_t mapped)
{
    pmm_t *pmm = pmmSearch(head,size);
    if(pmm){
        if(pmm->size >= (size + MPOOL_NINSIZE)){
            //Trim the memory block
            pmm_t *newPmm = ((mapped) ? ((pmm_t*)((uint32_t)pmm->addr + size)) : (ALLOC_PMM()));
            newPmm->addr = (ptr_t)((uint32_t)pmm->addr + size);
            newPmm->size = pmm->size - size;
            pmm->size -= newPmm->size;
            if(pmm->next == pmm){
                newPmm->next = newPmm;
                newPmm->prev = newPmm;
            }
            else{
                newPmm->next = pmm->next;
                newPmm->prev = pmm->prev;
                pmm->prev->next = newPmm;
                pmm->next->prev = newPmm;
            }
            if(*head == pmm){
                *head = newPmm;
            }
            return pmm;
        }
        if(pmm->next != pmm){
            if(pmm == *head){
                *head = pmm->next;
            }
            pmm->prev->next = pmm->next;
            pmm->next->prev = pmm->prev;
        }
        else{
            *head = NULL;
            pmm->prev = NULL;
            pmm->next = NULL;
        }
    }
    return pmm;
}

/**
 * MemoryBlockAlloc Implementation (See header file for description)
*/
ptr_t MemoryBlockAlloc(mPool_t *mPool, uint32_t size)
{
    // Check if the memory pool is available for allocation
    if(!mPool){
        return NULL;
    }
    // If the memory pool doesn't allow different size allocations try
    // the fixed allocation functions
    if(!(mPool->flags & MPOOL_FREEALLOCATION)){
        return mPoolMemoryAlloc(mPool);
    }
    if(mPool->flags & MPOOL_PFULL){
        return NULL;
    }
    // First thing make sure the size is at least MPOOL_NINSIZE
    if(size < MPOOL_NINSIZE){
        size = MPOOL_NINSIZE;
    }
    // Second thing ensure alignment
    size = ROUND_UP(size,mPool->size);
    // Now find a free memory block big enough
    pmm_t *pmm = pmmGet(&mPool->mHead, size, !(mPool->flags & MPOOL_UNMAPPED));
    // Check if we didn't run out of memory
    if(!pmm){
        return NULL;
    }
    // Get the memory object address
    ptr_t addr = (ptr_t)pmm->addr;
    // If the memory is unmapped deallocate the memory object structure
    if(mPool->flags & MPOOL_UNMAPPED){
        DEALLOC_PMM(pmm);
    }
    // Update the memory pool flags
    if(!mPool->mHead){
        mPool->flags &= ~MPOOL_PFREE;
        mPool->flags |= MPOOL_PFULL;
    }
    return addr;
}

/**
 * MemoryBlockFree Implementation (See header file for description)
*/
void MemoryBlockFree(mPool_t *mPool, ptr_t ptr, uint32_t size)
{
    // Check if the memory pool is available for allocation
    if(!mPool){
        return;
    }
    // If the memory pool doesn't allow different size allocations try
    // the fixed allocation functions
    if(!(mPool->flags & MPOOL_FREEALLOCATION)){
        mPoolMemoryFree(mPool, ptr);
        return;
    }
    // First thing make sure the size is at least MPOOL_NINSIZE
    if(size < MPOOL_NINSIZE){
        size = MPOOL_NINSIZE;
    }
    // Second thing ensure alignment
    size = ROUND_UP(size,mPool->size);
    ptr = (ptr_t)ROUND_DOWN((uint32_t)ptr,mPool->size);
    // Insert the memory block in the memory pool
    pmmInsert(&mPool->mHead, ptr, size, !(mPool->flags & MPOOL_UNMAPPED));
    // Update the memory pool flags
    mPool->flags &= ~MPOOL_PFULL;
    mPool->flags |= MPOOL_PFREE;

}

/**
 * mPoolMemoryAlloc Implementation (See header file for description)
*/
ptr_t mPoolMemoryAlloc(mPool_t *mPool)
{
    // Check if the memory pool is available for allocation
    if(!mPool || !(mPool->flags & MPOOL_FIXEDALLOCATION)){
        return NULL;
    }
    ptr_t ptr = NULL;
    // Check if the pool has pending objects
    if(mPool->mHead){
        // if so get one from the list
        pmm_t *pmm = NULL;
        LIST_GET_HEAD(mPool->mHead,pmm);
        ptr = pmm->addr;
        if(mPool->flags & MPOOL_UNMAPPED){
            // Deallocate the object structure if needed
            DEALLOC_PMM(pmm);
        }
        // return the object memory address
        return ptr;
    }
    // Check if we have enough memory in the memory pool
    if((mPool->end - mPool->free) >= mPool->size){
        // if so remove the required amount of memory and shrink the memory pool
        ptr = mPool->free;
        mPool->free += mPool->size;
    }
    if(mPool->end == mPool->free){
        mPool->flags &= ~MPOOL_PFREE;
        mPool->flags |= MPOOL_PFULL;
    }
    // return the object memory address or NULL if we run out of memory
    return ptr;
}

/**
 * mPoolMemoryFree Implementation (See header file for description)
*/
void mPoolMemoryFree(mPool_t *mPool, ptr_t ptr)
{
    // Check if the memory pool is available for allocation
    if(!mPool || !(mPool->flags & MPOOL_FIXEDALLOCATION)){
        return;
    }
    // Check if the object address belongs to the memory pool
    if(ptr < mPool->start || ptr > mPool->end){
        return;
    }
    // Check if we can add the object to the memory pool
    if((ptr + mPool->size) == mPool->free){
        mPool->free = ptr;
        // If list is empty return
        if(!mPool->mHead)return;
        // Check if there is objects pending that can now be added to the memory pool
        pmm_t *pmm =  mPool->mHead;
        ptr = pmm->addr;
        while((ptr + mPool->size) == mPool->free){
            // If the object can be added to the pool remove it from the objects list
            mPool->free = ptr;
            LIST_GET_HEAD(mPool->mHead,pmm);
            // Check if we had allocated an object memory structure
            if(mPool->flags & MPOOL_UNMAPPED){
                // If so deallocate it
                DEALLOC_PMM(pmm);
            }
            // If list is empty stop an return (there is nothing else to do)
            if(!mPool->mHead){
                return;
            }
            // Get the last object in the list (the list is sorted by the address)
            pmm = mPool->mHead;
            ptr = pmm->addr;
        }
    }
    else{
        pmm_t *pmm;
        if(mPool->flags & MPOOL_UNMAPPED){
            // If we are dealing with unmapped memory allocate an object structure
            pmm = ALLOC_PMM();
        }
        else{
            // Other wise just use the object memory (we have enough memory because MPOOL_NINSIZE == sizeof(pmm_t)
            pmm = (pmm_t*)ptr;
        }
        pmm->addr = ptr;
        pmm->size = mPool->size;
        // Add the memory object to the list
        LIST_INSERT(mPool->mHead,pmm);
    }
    mPool->flags &= ~MPOOL_PFULL;
    mPool->flags |= MPOOL_PFREE;
}

/**
 * mPoolState Implementation (See header file for description)
*/
uint32_t mPoolState(mPool_t *pool)
{
    if(pool->flags & MPOOL_PFREE)
    {
        return MPOOL_FREE;
    }
    else
    {
        return MPOOL_FULL;
    }
}
