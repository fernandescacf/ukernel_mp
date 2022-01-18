/**
 * @file        kvspace.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        14 May, 2016
 * @brief       Kernel Virtual Address Space Manager Definition Header File
*/

#ifndef KVSPACE_H_
#define KVSPACE_H_

#ifdef __cplusplus
    extern "C" {
#endif

/* Includes ----------------------------------------------- */
#include <types.h>
#include <vmap.h>


/* Exported types ----------------------------------------- */


/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */

/*
 * @brief   Initializes kernel virtual manager
 * @param   None
 * @retval  Success
 */
int32_t VirtualSpaceInit();

/*
 * @brief   Maps physical memory in the kernel virtual space
 * @param   paddr -	Physical memory base address
 * 			size  - Memory size
 * @retval  Virtual address
 */
vaddr_t VirtualSpaceMmap(paddr_t paddr, size_t size);

/*
 * @brief   Maps device memory in the kernel virtual space
 * @param   paddr -	Physical memory base address
 * 			size  - Memory size
 * @retval  Virtual address
 */
vaddr_t VirtualSpaceIomap(paddr_t paddr, size_t size);

/*
 * @brief   Unmaps memory from the kernel virtual space
 * @param   vaddr -	Virtual memory base address
 * @retval  No return
 */
void VirtualSpaceUnmmap(vaddr_t vaddr);


#ifdef __cplusplus
    }
#endif

#endif /* KVSPACE_H_ */
