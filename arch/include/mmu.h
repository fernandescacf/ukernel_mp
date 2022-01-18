/**
 * @file        mmu.h
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        December 21, 2019
 * @brief       Memory Management Unit BSP header file
*/

#ifndef MMU_H
#define MMU_H


/* Includes ----------------------------------------------- */
#include <types.h>
#include <misc.h>


/* Exported constants ------------------------------------- */

#define PAGE_SIZE				4096

#define CPOLICY_STRONGLY_ORDERED	0
#define CPOLICY_UNCACHED			1
#define CPOLICY_WRITETHROUGH		2
#define CPOLICY_WRITEBACK			3
#define CPOLICY_WRITEALLOC			4
#define CPOLICY_DEVICE_SHARED		5
#define CPOLICY_DEVICE_PRIVATE		6

#define APOLICY_NANA			0	// No access
#define APOLICY_RWNA			1	// Kernel read an write, User no access
#define APOLICY_RWRO			2	// Kernel read an write, User read only
#define APOLICY_RWRW			3	// Kernel read an write, User read an write
#define APOLICY_RONA			4	// Kernel read only, User no access
#define APOLICY_RORO			5	// Kernel read only, User read only


/* Exported macros ---------------------------------------- */



/* Exported types ----------------------------------------- */

typedef void * pgt_t;

typedef struct
{
	uint8_t		cpolicy;
	uint8_t		apolicy;
	uint8_t		shared;
	uint8_t		executable;
	uint8_t		global;
}memCfg_t;


/* Exported functions ------------------------------------- */

/*
 * @brief   Call to initialize the MMU Page table allocators.
 * 			All page table allocations are handled by the BSP code.
 * 			The MMU BSP will have to ask the Memory Manager to get memory
 * 			for the page table allocations.
 * 			Note that this call will be done before the Memory Manager is
 * 			fully initialized.
 * @param   None
 * @retval  Success
 */
int32_t mmuInitialization(void);

/*
 * @brief   Synchronize all memory system, caches and TLBs
 * @param   None
 * @retval  No Return
 */
void MemorySynchronize(void);

/*
 * @brief   Get Kernel L1 page table
 * @param   None
 * @retval  Kernel L1 page table
 */
pgt_t MemoryKernelPageTableGet(void);

/*
 * @brief   Synchronize the specified virtual address space
 * @param   v_addr - base virtual address
 * 			size - size of the address space
 * @retval  No Return
 */
void MemoryVmaSynchronize(vaddr_t v_addr, uint32_t size, uint32_t pid);

/*
 * @brief   Maps the specified virtual address space with the specified memory configuration
 * @param   pgt - page table
 *          v_addr - virtual address
 *          size - size of the address space
 *          memCfg - Memory configuration
 * @retval  No Return
 */
void MemoryMap(pgt_t pgt, paddr_t p_addr, vaddr_t v_addr, uint32_t size, memCfg_t *memCfg);

/*
 * @brief   Unmaps the specified virtual address space
 * @param   pgt - page table
 *          v_addr - virtual address
 *          size - size of the address space
 * @retval  No Return
 */
void MemoryUnmap(pgt_t pgt, vaddr_t v_addr, uint32_t size);

/*
 * @brief   Flushes the page table. All lower level page tables will be deallocated.
 * @param   pgt - page table
 * @retval  No return
 */
void MemoryMapClean(pgt_t pgt);

/*
 * @brief   Translate a virtual address to a physical address
 * @param   pgt - page table
 *          v_addr - virtual address
 * @retval  Returns the physical address
 */
paddr_t MemoryVirtual2physical(pgt_t pgt, vaddr_t v_addr);

/*
 * @brief   Allocates a new level 1 page table.
 * 			Note that if there is different sizes of level 1 page tables only the page
 * 			tables with the size used for the processes will be allocated
 * @param   none
 * @retval  Returns the page table
 */
pgt_t PageTableAlloc(void);

/*
 * @brief   Deallocates the specified page table.
 * 			Note that before the deallocation the function MemoryMapClean will be called
 * 			for the specified page table
 * @param   pgt - page table
 * @retval  No return
 */
void PageTableDealloc(pid_t pid, pgt_t pgt);

#endif // MMU_H
