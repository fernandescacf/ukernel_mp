/**
 * @file        kheap.h
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        16 November, 2020
 * @brief       Kernel Heap Definition Header File
*/


#ifndef KHEAP_H_
#define KHEAP_H_

#ifdef __cplusplus
    extern "C" {
#endif

/* Includes ----------------------------------------------- */
#include "types.h"


/* Exported types ----------------------------------------- */


/* Exported constants ------------------------------------- */
#define KHEAP_DEFAULT_SIZE      4096

/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */

/*
 * @brief   Initializes kernel heap with the specified size
 * @param   size - Kernel heap initial size
 *          grow - Grow rate
 * @retval  Success
 */
int32_t kheapInit(size_t size, size_t grow);

/**
 * @brief   Allocates a block of size bytes of memory
 *          returning a pointer to the beginning of the block
 * @param   size - Size of the memory block in bytes
 * @retval  Pointer to the memory block allocated
 */
ptr_t kmalloc(size_t size);

/**
 * @brief   Deallocate a block of memory
 * @param   ptr - Pointer to a memory block
 *          size - Size of the memory block in bytes
 * @retval  No return value
 */
void kfree(ptr_t ptr, size_t size);

#ifdef __cplusplus
    }
#endif

#endif /* KHEAP_H_ */
