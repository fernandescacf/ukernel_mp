/**
 * @file        memmgr.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        22 December, 2019
 * @brief       Kernel Memory Manager header file
*/

#ifndef MEMMGR_H
#define MEMMGR_H


/* Includes ----------------------------------------------- */
#include <types.h>
#include <zone.h>


/* Exported constants ------------------------------------- */



/* Exported macros ---------------------------------------- */



/* Exported types ----------------------------------------- */
typedef struct
{
	struct
	{
		ptr_t	base;
		ptr_t	end;
	}text;

	struct
	{
		ptr_t	base;
		ptr_t	end;
	}data;

	struct
	{
		ptr_t	base;
		ptr_t	end;
	}stack;

	struct
	{
		ptr_t	base;
		size_t	size;
	}rfs;

}bootLayout_t;


/* Exported functions ------------------------------------- */

/* @brief	Routine to initialize the memory manager. All memory
 * 			zones will be available after the Memory Manager is
 * 			initialized.
 *
 * @param	None
 *
 * @retval	Pointer to the memory zone handler
 */

int32_t MemoryManagerInit(bootLayout_t *bootLayout);

/* @brief	Routine to allocate a memory block with the specified size from a
 * 			memory zone of the specified type (in case of indirect type memory
 * 			from a direct memory zone may be allocated and the memory address
 * 			will be convert from virtual to physical)
 *			Note: Device zones are not be allowed. Use MemoryDeviceGet instead
 *
 * @param	size	- size of the memory
 * 			type	- type of the memory zone
 *
 * @retval	Pointer to the allocated memory or NULL if the request couldn't be fulfilled
 */
ptr_t MemoryGet(size_t size, ZoneType_t type);

/* @brief	Routine to allocate a memory block with the specified size and alignment
 *
 * @param	size	- size of the memory block
 * 			align	- base address alignment
 * 			type	- type of the memory zone
 *
 * @retval	Pointer to the allocated memory or NULL if the request couldn't be fulfilled
 */
ptr_t MemoryGetAligned(size_t size, ulong_t align, ZoneType_t type);

/* @brief	Routine to return allocated memory to the memory zones.
 * 			Note that this function can be used for normal memory or device memory
 *
 * @param	addr	- base address of the allocated memory
 * 			size	- size of the allocated memory
 *
 * @retval	No Return
 */
void MemoryFree(ptr_t addr, size_t size);

/* @brief	Routine to convert from logical or virtual to a physical address
 *
 * @param	addr	- logical (virtual) address
 *
 * @retval	Physical address or NULL in case of error
 */
ptr_t MemoryL2P(ptr_t laddr);

/* @brief	Routine to convert from physical to a logical or virtual address
 *
 * @param	addr	- physical address
 *
 * @retval	Logical/virtual address or NULL in case of error
 */
ptr_t MemoryP2L(ptr_t paddr);

bool_t MemoryIsLogicalAddr(vaddr_t vaddr);

/* @brief	Routine to get the total ram installed
 *
 * @param	No parameters
 *
 * @retval	Total ram installed
 */
uint32_t RamGetTotal(void);

/* @brief	Routine to get the total ram available (not allocated)
 *
 * @param	No parameters
 *
 * @retval	Total ram available (not allocated)
 */
uint32_t RamGetAvailable(void);

/* @brief	Routine to get the total ram currently in use (allocated)
 *
 * @param	No parameters
 *
 * @retval	Total ram currently in use (allocated)
 */
uint32_t RamGetUsage(void);

#endif // MEMMGR_H
