/**
 * @file        vstack.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        06 January, 2020
 * @brief       Virtual Stack Mapper header file
*/

#ifndef VSTACK_H
#define VSTACK_H


/* Includes ----------------------------------------------- */
#include <types.h>
#include <mmtypes.h>
#include <vmap.h>


/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */



/* Exported types ----------------------------------------- */

// Mock stack types
typedef vSpace_t	vStack_t;
typedef vManager_t	sManager_t;


/* Exported functions ------------------------------------- */

/* @brief	Same as vManagerCreate but it will only create Stack Type Virtual Managers
 * @param	pgt		- page table that the virtual space manager will use to map memory
 *  		vBase	- base virtual address of the virtual space that the VM will handle
 *  		vTop	- top virtual address of the virtual space that the VM will handle
 *
 * @retval	Pointer to the Virtual Stack Manager handler
 */
sManager_t *sManagerCreate(pgt_t pgt, vaddr_t vBase, vaddr_t vTop);

/* @brief	Same as vManagerInitialize but it will only initialize Stack Type Virtual Managers
 * @param	sm		- Pointer to the Virtual Stack Manager
 * 			pgt		- page table that the virtual space manager will use to map memory
 *  		vBase	- base virtual address of the virtual space that the VM will handle
 *  		vTop	- top virtual address of the virtual space that the VM will handle
 *
 * @retval	Pointer to the Virtual Stack Manager handler
 */
int32_t sManagerInitialize(sManager_t *sm, pgt_t pgt, vaddr_t vBase, vaddr_t vTop);

/* @brief	Same as vManagerDestroy but should only be used for Virtual Stack Manager
 *
 * @param	sm		- Pointer to the Virtual Stack Manager
 *
 * @retval	Success
 */
int32_t sManagerDestroy(sManager_t *sm);

/* @brief	Same as vManagerSetUnmapHandler but should only be used for Virtual Stack Manager
 *
 * @param	sm		- Pointer to the Virtual Stack Manager
 * 			arg		- optional argument to be passed to the unmapHandler
 * 			unmapHandler	- pointer to the unmapHandler function
 *
 * @retval	Success
 */
int32_t sManagerSetUnmapHandler(sManager_t *sm, void *arg, int32_t (*unmapHandler)(void*, pmm_t*));

/* @brief	Routine to reserve a stack frame with the specified size
 *
 * @param	vm		- Pointer to the Virtual Space Manager
 * 			size	- Size of the stack frame
 *
 * @retval	Pointer to the virtual stack handler
 */
vStack_t *vStackGet(sManager_t *sm, size_t size);

/* @brief	Routine to release a stack frame. All memory mapped in this stack frame
 * 			will be unmaped
 *
 * @param	vstack	- Pointer to the virtual stack (stack frame) being released
 *
 * @retval	Success
 */
int32_t vStackFree(vStack_t *vstack);

/* @brief	Routine expand the mapped stack with the specified size
 *
 * @param	vstack	- Pointer to the virtual stack (stack frame)
 * 			pAddr	- Address of the physical memory being mapped
 * 			size	- Size of the physical memory block being mapped
 * 			mapType	- Attributes of the memory being mapped
 *
 * @retval	Virtual address of the new mapped section
 * 			NULL in case of error
 */
vaddr_t vStackMap(vStack_t *vstack, paddr_t pAddr, size_t size, ulong_t mapType);

/* @brief	Routine to contract the mapped stack with the specified size
 *
 * @param	vstack	- Pointer to the virtual stack (stack frame)
 * 			size	- Size of the section being unmapped
 *
 * @retval	Virtual address of the current stack base address
 */
vaddr_t vStackUnmap(vStack_t *vstack, size_t size);


#endif // VSTACK_H
