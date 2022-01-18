/**
 * @file        mpool.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        30 March, 2016
 * @brief       Kernel Memory Pools Definition Header File
*/

#ifndef MPOOL_H
#define MPOOL_H

#ifdef __cplusplus
    extern "C" {
#endif


/* Includes ----------------------------------------------- */
#include <types.h>
#include <mmtypes.h>


/* Exported types ----------------------------------------- */

typedef struct
{
    pmm_t		*mHead;		// list of memory blocks
    ptr_t		start;		// memory pool start address
    ptr_t		end;		// memory pool end address
    ptr_t		free;		// pointer to the beginning of the free memory in the pool
	uint32_t	size;		// size of the memory blocks to be allocated
	uint32_t	flags;		// control flags
}mPool_t;


/* Exported constants ------------------------------------- */

#define MPOOL_NOALLOCATION		(0x0)
#define	MPOOL_FREEALLOCATION	(0x1)
#define MPOOL_FIXEDALLOCATION	(0x2)

#define	MPOOL_ALIGNCHECK		(0x1 << 4)
#define MPOOL_USEPOINTERS       (0x1 << 5)
#define MPOOL_UNMAPPED			(0x1 << 6)
#define MPOOL_INTEGRATEHDR		(0x1 << 7)

enum{
	MPOOL_FREE = 0x1,
	MPOOL_HALF = 0x2,
	MPOOL_FULL = 0x4
};


/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */

/*
 * @brief   Creates a new memory pool
 * @param   start	- start address of the memory pool
 * 			end		- end address of the memory pool
 * 			size	- size of the memory objects to be allocated
 * 			align	- alignment of the memory pool and the memory objects
 * 			flags	- control flags
 * @retval  Pointer to the allocated memory pool or null if an error occurred
 */
mPool_t *mPoolCreate(ptr_t start, ptr_t end, size_t size, uint32_t align, ulong_t flags);

/*
 * @brief   Initializes the given memory pool
 * @param   mPool	- pointer to the memory pool
 * 			start	- start address of the memory pool
 * 			end		- end address of the memory pool
 * 			size	- size of the memory objects to be allocated
 * 			align	- alignment of the memory pool and the memory objects
 * 			flags	- control flags
 * @retval  Pointer to the allocated memory pool or null if an error occurred
 */
int32_t mPoolInit(mPool_t *mPool, ptr_t start, ptr_t end, size_t size, uint32_t align, ulong_t flags);

/*
 * @brief   Destroys the memory pool
 * @param   mpool	- memory pool used to allocate a memory object
 * @retval  Success
 */
int32_t mPoolDestroy(mPool_t *mPool);

/*
 * @brief   Allocates a memory object
 * @param   mpool	- memory pool used to allocate a memory object
 * @retval  Pointer to the allocated memory object or null if an error occurred
 */
ptr_t mPoolMemoryAlloc(mPool_t *mPool);

/*
 * @brief   Deallocates a memory object
 * @param   mpool	- memory pool owner of the memory object
 * 			obj		- pointer to the base of the memory object
 * @retval  No return
 */
void mPoolMemoryFree(mPool_t *mPool, ptr_t ptr);

/*
 * @brief   Allocates a memory object of the required size only if the memory
 * 			pool allows different sizes allocation. Other wise if the memory
 * 			pool only allows a fixed size allocations the routine discards
 * 			the passed size an allocates a fixed size memory object
 * @param   mpool	- memory pool used to allocate a memory object
 * 			size	- size of the memory
 * @retval  Pointer to the allocated memory object or null if an error occurred
 */
ptr_t MemoryBlockAlloc(mPool_t *mPool, uint32_t size);

/*
 * @brief   Deallocates a memory object. It can be used for all memory pools
 * 			configurations
 * @param   mpool	- memory pool owner of the memory object
 * 			obj		- pointer to the base of the memory object
 * 			size	- memory object size. Discarded if the memory pool only
 * 					  allow fixed size allocations
 * @retval  No return
 */
void MemoryBlockFree(mPool_t *mPool, ptr_t ptr, uint32_t size);

/*
 * @brief   Routine to get the memory pool state
 * @param   mPool	- pointer to the memory pool
 *
 * @retval  MPOOL_FREE if the memory pool still has memory available
 * 			MPOOL_FULL if the memory pool still has no more memory available
 */
uint32_t mPoolState(mPool_t *pool);


#ifdef __cplusplus
    }
#endif

#endif // MPOOL_H
