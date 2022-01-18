/**
 * @file        vmap.h
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        03 January, 2020
 * @brief       Virtual Memory Mapper header file
*/

#ifndef VMAP_H
#define VMAP_H


/* Includes ----------------------------------------------- */
#include <types.h>
#include <mmtypes.h>
#include <mpool.h>
#include <vpage.h>

#include <klock.h>

/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */



/* Exported types ----------------------------------------- */

typedef enum
{
	VMANAGER_FIXED,
	VMANAGER_STACK,
	VMANAGER_NORMAL
}vMgrType_t;

struct vSpace;
typedef struct vSpace vSpace_t;

typedef struct
{
    pgt_t		pgt;							// page table used to translate between virtual and physical memory
    vMgrType_t	type;							// virtual manager type
    mPool_t		*vSpacePool;    				// memory pool that manages the virtual address space
    vSpace_t	*vSpaceList;  					// reserved spaces list
    vPage_t     *fixedPages;					// list of virtual pages used for fixed mappings
    ulong_t		flags;							// Control Flags
    void		*arg;       					// argument passed to the unmap_handler function
    int32_t     (*unmapHandler)(void*, pmm_t*);	// Function responsible to recover the physical memory assigned to a memory page
    klock_t     lock;
}vManager_t;

struct vSpace
{
    vSpace_t    *next,*prev;
    vManager_t  *owner;
    vaddr_t      base;
    vaddr_t      top;
    vaddr_t      freePtr;
    vPage_t     *vPageList;
};


/* Exported functions ------------------------------------- */

/* @brief	Routine to create a new virtual space manager. This routine
 * 			will create and initialize the new virtual space manager
 * @param	pgt		- page table that the virtual space manager will use to map memory
 *  		vBase	- base virtual address of the virtual space that the VM will handle
 *  		vTop	- top virtual address of the virtual space that the VM will handle
 *  		type	- type of Virtual Space Manager
 *
 * @retval	Pointer to the Virtual Space Manager handler
 */
vManager_t *vManagerCreate(pgt_t pgt, vaddr_t vBase, vaddr_t vTop, vMgrType_t type);

/* @brief	Routine to initialize the a virtual space manager not created using vManagerCreate
 * @param	vm		- Pointer to the Virtual Space Manager
 * 			pgt		- page table that the virtual space manager will use to map memory
 *  		vBase	- base virtual address of the virtual space that the VM will handle
 *  		vTop	- top virtual address of the virtual space that the VM will handle
 *  		type	- type of Virtual Space Manager
 *
 * @retval	Pointer to the Virtual Space Manager handler
 */
int32_t vManagerInitialize(vManager_t *vm, pgt_t pgt, vaddr_t vBase, vaddr_t vTop, vMgrType_t type);

/* @brief	Routine to destroy virtual space manager. This routine will unmap all is virtual space
 * 			the unmaphandler function if set will also be called to free all physical memory
 * 			that was being mapped
 * @param	vm		- Pointer to the Virtual Space Manager
 *
 * @retval	Success
 */
int32_t vManagerDestroy(vManager_t *vm);

/* @brief	Routine to set the unmaphandler function
 * @param	vm		- Pointer to the Virtual Space Manager
 * 			arg		- optional argument to be passed to the unmapHandler
 * 			unmapHandler	- pointer to the unmapHandler function
 *
 * @retval	Success
 */
int32_t vManagerSetUnmapHandler(vManager_t *vm, void *arg, int32_t (*unmapHandler)(void*, pmm_t*));

/* @brief	Routine to reserve a virtual space with the specified size
 *
 * @param	vm		- Pointer to the Virtual Space Manager
 * 			size	- Size of the virtual space being reserved
 *
 * @retval	Pointer to the virtual space handler
 */
vSpace_t *vSpaceReserve(vManager_t *vm, size_t size);

/* @brief	Routine to release a virtual space. All memory mapped in this virtual space
 * 			will be unmaped
 *
 * @param	vSpace	- Pointer to the virtual space being released
 *
 * @retval	Success
 */
int32_t vSpaceRelease(vSpace_t *vSpace);

/* @brief	Routine to map a complete virtual space. Note that to use this function
 * 			the all virtual space has to be unmapped
 *
 * @param	vSpace	- Pointer to the virtual space being released
 * 			pAddr	- Address of the physical memory being mapped
 * 			mapType	- Attributes of the memory being mapped
 *
 * @retval	Base virtual address where the physical memory was mapped
 * 			NULL in case of error
 */
vaddr_t vSpaceMap(vSpace_t *vSpace, paddr_t pAddr, ulong_t mapType, memCfg_t *memcfg);

/* @brief	Routine to unmap a complete virtual space
 *
 * @param	vSpace	- Pointer to the virtual space being released
 *
 * @retval	Base virtual address of the reserved virtual space
 */
vaddr_t vSpaceUnmap(vSpace_t *vSpace);

/* @brief	Routine to map a section of the virtual space. The expansion of the mapping
 * 			(top-down or bottom-up)  will be done according to the Virtual Space Manager type
 *
 * @param	vSpace	- Pointer to the virtual space being released
 * 			pAddr	- Address of the physical memory being mapped
 * 			size	- Size of the physical memory block being mapped
 * 			mapType	- Attributes of the memory being mapped
 *
 * @retval	Base virtual address where the physical memory was mapped
 * 			NULL in case of error
 */
vaddr_t vSpaceMapSection(vSpace_t *vSpace, paddr_t pAddr, size_t size, ulong_t mapType, memCfg_t *memcfg);

/* @brief	Routine to unmap a section of a virtual space. The contraction of the mapping
 * 			(top-down or bottom-up)  will be done according to the Virtual Space Manager type
 *
 * @param	vSpace	- Pointer to the virtual space being released
 * 			size	- Size of the section being unmapped
 *
 * @retval	Virtual address of the reserved virtual space free sections
 */
vaddr_t vSpaceUnmapSection(vSpace_t *vSpace, size_t size);

/* @brief	Routine to map a physical memory block without the need of reserving
 * 			a virtual space
 *
 * @param	vm		- Pointer to the Virtual Space Manager
 * 			pAddr	- Address of the physical memory being mapped
 * 			size	- Size of the physical memory block being mapped
 * 			mapType	- Attributes of the memory being mapped
 *
 * @retval	Base virtual address where the physical memory was mapped
 * 			NULL in case of error
 */
vaddr_t vMap(vManager_t *vm, paddr_t pAddr, size_t size, ulong_t mapType, memCfg_t *memcfg);

/* @brief	Routine to unmap a virtual space area that wasn't mapped using
 * 			a reserved virtual space
 *
 * @param	vm		- Pointer to the Virtual Space Manager
 * 			vaddr	- Address of the virtual address space being unmapped
 *
 * @retval	Success
 */
int32_t vUnmap(vManager_t *vm, vaddr_t vaddr);

#endif // VMAP_H
