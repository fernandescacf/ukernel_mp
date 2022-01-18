/**
 * @file        buddy.h
 * @author      Carlos Fernandes
 * @version     3.0
 * @date        02 September, 2020
 * @brief       Buddy System Header File
*/

#ifndef BUDDY_H
#define BUDDY_H

#ifdef __cplusplus
    extern "C" {
#endif

/* Includes ----------------------------------------------- */
#include <types.h>
#include <klock.h>


/* Exported types ----------------------------------------- */
typedef struct mblock
{
    struct mblock *next;
    struct mblock *prev;
}mBlock_t;

typedef struct
{
	klock_t		lock;
	ptr_t		pAddr;
	ptr_t		vAddr;
	uint32_t	size;
	uint32_t	offset;
	uint32_t	availableMemory;
	mBlock_t	*mBlocks[11];
}buddy_t;


/* Exported constants ------------------------------------- */
#define MAX_ORDER		10			// 4k -> 4MB

#ifndef PAGE_SIZE
	#define PAGE_SIZE		0x1000
#endif

/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */

/*
 * @brief   Create a buddy system handler.
 *          Note: only call this function after dynamic memory allocator is running
 * @param   pAddr - Base physical address to be tracked
 *          vAddr - Corresponding virtual address
 *          size - size of the memory block to be tracked
 *          offset - offset from vAddr to be excluded by the buddy
 * @retval  Buddy system handler
 */
buddy_t *buddySystemCreate(ptr_t pAddr, ptr_t vAddr, uint32_t size, uint32_t offset);

/*
 * @brief   Initializes the buddy system
 * @param   buddy - buddy system handler
 * @retval  Success
 */
int32_t buddyInit(buddy_t *buddy);

/*
 * @brief   Allocates a memory page with the specified size
 * @param   buddy - buddy system handler
 *          size - memory allocation size in base 2
 * @retval  Returns pointer to the allocated memory page
 */
ptr_t buddyGetMemory(buddy_t *buddy, uint32_t size);

/*
 * @brief   Deallocates the specified memory page
 * @param   buddy - buddy system handler
 *          memory - memory page
 * 			size - memory size
 * @retval  No return
 */
void buddyFreeMemory(buddy_t *buddy, ptr_t memory, uint32_t size);

#ifdef __cplusplus
    }
#endif

#endif // BUDDY_H
