/**
 * @file        vmap.h
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        December 28, 2019
 * @brief       Virtual Memory Mapper header file
*/

#ifndef VPAGE_H
#define VPAGE_H


/* Includes ----------------------------------------------- */
#include <types.h>
#include <mmtypes.h>
#include <mmu.h>
#include <klock.h>


/* Exported constants ------------------------------------- */

enum
{
	PAGE_KERNEL_TEXT	= 0,
	PAGE_KERNEL_DATA,
	PAGE_KERNEL_DEVICE,
	PAGE_USER_TEXT,
	PAGE_USER_DATA,
	PAGE_USER_DEVICE,
	PAGE_FAULT,
	PAGE_CUSTOM
};

/* Exported macros ---------------------------------------- */



/* Exported types ----------------------------------------- */

struct vPage;
typedef struct vPage vPage_t;

struct vPage{
    vPage_t	 *next;
    vaddr_t	 vAddr;
    size_t	 size;
    pmm_t	 *pPageList;
    ulong_t	 mapType;
    memCfg_t *memcfg;
};


/* Exported functions ------------------------------------- */

/* @brief	Routine to map a memory section without the allocation virtual page handlers
 * 			It uses directly the MMU functions to map or unmap memory
 * @param	pgt		- Page Table pointer
 * 			pAddr	- Physical address of the memory being mapped
 * 			vAddr	- Virtual address where the memory will be mapped
 *			size	- Size of the memory block
 *			mapType	- Attributes of the memory being mapped
 * @retval	Success
 */
uint32_t vPageMapMemory(klock_t* lock, pgt_t pgt, paddr_t pAddr, vaddr_t vAddr, size_t size, ulong_t mapType, memCfg_t *memcfg);

/* @brief	Routine to map a memory block into the virtual address space.
 * @param	pgt		- Page Table pointer
 * 			pAddr	- Physical address of the memory being mapped
 * 			vAddr	- Virtual address where the memory will be mapped
 *			size	- Size of the memory block
 *			mapType	- Attributes of the memory being mapped
 * @retval	A virtual page handler (has the info about the virtual address space and physical memory)
 */
vPage_t *vPageMap(klock_t* lock, pgt_t pgt, paddr_t pAddr, vaddr_t vAddr, size_t size, ulong_t mapType, memCfg_t *memcfg);

/* @brief	Routine to unmap a memory block from the virtual address space.
 * @param	pgt		- Page Table pointer
 * 			vPage	- Virtual page handler referent to the memory being unmaped
 * 			unmapHandler - Function to be called to handle the unmaped physical memory
 * 			arg	- Optional argument to pass to the unmap function handler
 * @retval	Success
 */
int32_t vPageUnmap(klock_t* lock, pgt_t pgt, vPage_t *vPage, void *arg, int32_t (*unmapHandler)(void*, pmm_t*));


#endif // VPAGE_H
