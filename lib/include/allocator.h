/**
 * @file        allocator.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        02 April, 2020
 * @brief       Fixed size allocator. All allocated memory as an
 * 				associated ID that can be used to track the memory.
 * 				Supports capacity expansion and purge of free memory.
*/

#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H

#ifdef __cplusplus
    extern "C" {
#endif

/* Includes ----------------------------------------------- */
#include <types.h>
#include <rwlock.h>


/* Exported types ----------------------------------------- */

typedef struct allocator
{
	struct allocator *next;
	void *data;
	void *free;
	uint16_t size;
	uint16_t nfree;
	uint16_t maxId;
	uint16_t allocSize;
	uint32_t flags;
	rwlock_t lock;
}allocator_t;


/* Exported constants ------------------------------------- */

#define ALLOCATOR_CLEAN_MEMORY  (1 << 0)
#define ALLOCATOR_ALLOW_EXPAND  (1 << 1)
#define ALLOCATOR_PURGE         (1 << 2)


/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */

/*
 * @brief   Initialize the allocator with the specified size.
 * 			All allocated objects will be of the specified size.
 *
 * @param   allocator - allocator handler
 * 			size      - Initial size of the allocator
 * 			allocSize - Object allocated size
 * 			flags     - Allocator control flags
 *
 * @retval  Return Success
 */
int32_t AllocatorInit(allocator_t *allocator, uint16_t size, uint16_t allocSize, uint32_t flags);

/*
 * @brief   Routine to allocate a memory object.
 * 			If ALLOCATOR_ALLOW_EXPAND is set the allocator will allocate
 * 			more memory to respond to the request.
 * 			If ALLOCATOR_CLEAN_MEMORY is set the memory of the allocated
 * 			object will be clean.
 *
 * @param   allocator - allocator handler
 * 			id        - used to return the associated id
 *
 * @retval  Returns object address or NULL in case of error
 */
void *AllocatorGet(allocator_t *allocator, int32_t *id);

/*
 * @brief   Routine to return an allocated object to the allocator.
 * 			If ALLOCATOR_PURGE is set the allocator will free
 * 			allocated memory if possible.
 *
 * @param   allocator - allocator handler
 * 			obj       - pointer to object being returned
 *
 * @retval  Returns success
 */
int32_t AllocatorFree(allocator_t *allocator, void *obj);

/*
 * @brief   Routine to return an allocated object to the allocator using its ID.
 * 			If ALLOCATOR_PURGE is set the allocator will free
 * 			allocated memory if possible.
 *
 * @param   allocator - allocator handler
 * 			id        - ID of object being returned
 *
 * @retval  Returns success
 */
int32_t AllocatorFreeId(allocator_t *allocator, int32_t id);

/*
 * @brief   Routine to convert an object ID to its base address.
 *
 * @param   allocator - allocator handler
 * 			id        - ID of object
 *
 * @retval  Returns object address or NULL in case of error
 */
void *AllocatorToAddr(allocator_t *allocator, int32_t id);

/*
 * @brief   Routine to convert an object address to its associated ID.
 *
 * @param   allocator - allocator handler
 * 			obj       - pointer to object
 *
 * @retval  Returns object ID or -1 in case of error
 */
int32_t AllocatorToId(allocator_t *allocator,  void *obj);

/*
 * @brief   Routine to destroy an allocator and free all the resources
 * 			allocated by it
 *
 * @param   allocator - allocator handler
 *
 * @retval  No return
 */
void AllocatorDestroy(allocator_t *allocator);

#ifdef __cplusplus
    }
#endif

#endif // _ALLOCATOR_H
