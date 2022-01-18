/**
 * @file        mmu.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        December 21, 2019
 * @brief       Memory Management Unit drive
*/


/* Includes ----------------------------------------------- */
#include <mmu.h>
#include <cache.h>
#include <asm.h>

#include <klock.h>
#include <spinlock.h>

#include <memmgr.h>
#include <string.h>

/* Private types ------------------------------------------ */

typedef ptr_t	pgt2_t;

/* Private constants -------------------------------------- */

//PAGE_SIZE is defined in the mmu.h
#define LARGE_PAGE_SIZE		0X10000
#define SECTION_SIZE		0x100000
#define LARGE_SECTION_SIZE	0x1000000

#define PAGE_SHIFT			(12)
#define SUPERSECTION_SHIFT	(24)
#define SECTION_SHIFT		(20)
#define LARGEPAGE_SHIFT		(16)
#define SMALLPAGE_SHIFT		(12)

// Page tables sizes and alignments
#define L1PGT_SIZE		16384
#define L1PGT_ALIGN		16384
#define L1PGT_USR_SIZE	8192		// For user process we only need half of the page table
#define L1PGT_USR_ALIGN	16384		// But we need to keep the 16k alignment
#define L2PGT_SIZE		1024
#define L2PGT_ALIGN		1024

// L1 Page Table Entries
#define FAULT			(0x0)
#define L2_PGT			(0x1)
#define SECTION			(0x2)
#define SUPERSECTION	((1 << 18)|(0x2))
// L2 Page Table Entries
#define LARGEPAGE		(0x1)
#define SMALLPAGE		(0x2)

/*
 * MMU SECTION FLAGS:
 * 	flags[2]	->	B	// Write Buffer
 * 	flags[3]	->	C	// Cache
 * 	flags[4]	->	XN	// Execute never
 * 	flags[11:10]->	AP	// Access permission
 * 	flags[14:12]->	TEX // Type extension
 * 	flags[15]	->	APX // Access permission
 * 	flags[16]	->	S	// Sharable
 * 	flags[17]	->	nG	// Not Global
 */
// CAUTION: This configuration is only valid for the L1 Page Table entries
#define MMU_B		(1 << 2)
#define MMU_C		(1 << 3)
#define MMU_XN		(1 << 4)
#define MMU_AP0		(1 << 10)
#define MMU_AP1		(1 << 11)
#define MMU_TEX0	(1 << 12)
#define MMU_TEX1	(1 << 13)
#define MMU_TEX2	(1 << 14)
#define MMU_AP2		(1 << 15)
#define MMU_S		(1 << 16)
#define MMU_nG		(1 << 17)

/* Private macros ----------------------------------------- */

#define SHARABLE_SET(x)			(x |= MMU_S)
#define LOCAL_SET(x)			(x |= MMU_nG)
#define NO_EXEC_SET(x)			(x |= MMU_XN)
#define CPOLICY_SET(x,cp)		(x |= cp)
#define APERMITION_SET(x,ap)	(x |= ap)

/*
 * MMU LARGEPAGE FLAGS:
 * 	flags[2]	->	B	// Write Buffer
 * 	flags[3]	->	C	// Cache
 * 	flags[5:4]->	AP	// Access permission
 * 	flags[9]	->	APX // Access permission
 * 	flags[10]	->	S	// Sharable
 * 	flags[11]	->	nG	// Not Global
 * 	flags[14:12]->	TEX // Type extension
 * 	flags[15]	->	XN	// Execute never
 */
#define MMU_FLAGS_LARGEPAGE(f)	\
	((f & 0xC) 					\
	|((f & 0xC00) >> 6)			\
	|(f & 0x7000)				\
	|((f & 0x38000) >> 6)		\
	|((f & 0x10) << 11))

/*
 * MMU SMALLPAGE FLAGS:
 * 	flags[0]	->	XN	// Execute never
 * 	flags[2]	->	B	// Write Buffer
 * 	flags[3]	->	C	// Cache
 * 	flags[5:4]->	AP	// Access permission
 * 	flags[8:6]->	TEX // Type extension
 * 	flags[9]	->	APX // Access permission
 * 	flags[10]	->	S	// Sharable
 * 	flags[11]	->	nG	// Not Global
 */
#define MMU_FLAGS_SMALLPAGE(f)	\
	(((f & 0x10) >> 4)			\
	|(f & 0xC)	 				\
	|((f & 0xC00) >> 6)			\
	|((f & 0x7000) >> 6)		\
	|((f & 0x38000) >> 6))

#define SHIFT_1MB(x)		(x >>= 20)


/* Private variables -------------------------------------- */

static ulong_t cacheCfgs[] =
{									// TEX[2:0] C B
		(0x0),						// 	0 0 0	0 0	-> Strongly-ordered
		(MMU_TEX0),					//	0 0 1	0 0	-> Outer and Inner Non-cacheable
		(MMU_C),					//	0 0 0	1 0	-> Outer and Inner Write-Through, no Write-Allocate
		(MMU_C | MMU_B),			//	0 0 0	1 1	-> Outer and Inner Write-Back, no Write-Allocate
		(MMU_TEX0 | MMU_C | MMU_B), //	0 0 1	1 1	-> Outer and Inner Write-Back, Write-Allocate
		(MMU_B),					//	0 0 0	0 1	-> Shareable Device
		(MMU_TEX1)					// 	0 1 0	0 0	-> Non-shareable Device
};

static ulong_t accessCfgs[] =
{
		(0x0),
		(MMU_AP0),
		(MMU_AP1),
		(MMU_AP0 |MMU_AP1),
		(MMU_AP2),
		(MMU_AP2 | MMU_AP1)
};

static struct
{
	ptr_t	top;
	klock_t lock;
}l2Allocator;

/* Private function prototypes ---------------------------- */

static pgt2_t L2PageTableAlloc(void)
{
	uint32_t status;
	Klock(&l2Allocator.lock, &status);

	if(l2Allocator.top == NULL)
	{
		l2Allocator.top = MemoryGet(PAGE_SIZE, ZONE_DIRECT);

		if(l2Allocator.top != NULL)
		{
			// Ensure memory for page tables are clean
			memset(l2Allocator.top, 0x0, PAGE_SIZE);

			uint32_t *ptr = (uint32_t *)l2Allocator.top;
			int32_t i;
			for(i = 0; i < 3; i++)
			{
				*ptr = (uint32_t)ptr + L2PGT_SIZE;
				ptr = (uint32_t*)(*ptr);
			}
			*ptr = NULL;
		}
		else
		{
			Kunlock(&l2Allocator.lock, &status);
			return NULL;
		}
	}

	pgt2_t pgt = (pgt2_t)l2Allocator.top;
	if(*((uint32_t*)l2Allocator.top) != NULL)
	{
		l2Allocator.top = (ptr_t)(*((uint32_t*)l2Allocator.top));
	}
	else
	{
		l2Allocator.top = NULL;
	}

	Kunlock(&l2Allocator.lock, &status);

//	memset(pgt, 0x0, L2PGT_SIZE);
	(*((uint32_t*)pgt)) = 0x0;

	return pgt;
}

inline static void L2PageTableFlush(pgt2_t pgt2)
{
	memset(pgt2, 0x0, L2PGT_SIZE);
}

static void L2PageTableDealloc(pgt2_t pgt2)
{
	L2PageTableFlush(pgt2);

	uint32_t status;
	Klock(&l2Allocator.lock, &status);

	if(l2Allocator.top != NULL)
	{
		(*((uint32_t*)pgt2)) = (uint32_t)l2Allocator.top;
	}

	l2Allocator.top = (ptr_t)pgt2;

	Kunlock(&l2Allocator.lock, &status);
}

inline static vaddr_t PageTableVirtualAddress(ptr_t pgt)
{
	return (vaddr_t)MemoryP2L(pgt);
}

inline static paddr_t PageTablePhysicalAddress(ptr_t pgt)
{
	return (paddr_t)MemoryL2P(pgt);
}

inline static ulong_t GetPteFlags(ulong_t cpolicy, ulong_t apolicy, uint8_t shared, uint8_t executable, uint8_t global)
{
	ulong_t flags = 0x0;

	CPOLICY_SET(flags, cpolicy);
	APERMITION_SET(flags, apolicy);
	if(shared) SHARABLE_SET(flags);
	if(!global) LOCAL_SET(flags);
	if(!executable) NO_EXEC_SET(flags);

	return flags;
}

/**
 * mmu_16M_page_map Implementation (See header file for description)
*/
void mmu_16M_page_map(ulong_t *pgt, ulong_t p_addr, ulong_t v_addr, uint32_t size, ulong_t flags){
    uint32_t i,j;
    // Set entry flags
    ulong_t pte = flags;
    // first page table entry
    pgt = (ulong_t *)((uint32_t)pgt + (v_addr >> 18));
    pte |= (p_addr & 0xFF000000);
    // Populate page table
    for(i=0;i<size;i++){
    	for(j=0;j<16;j++){
    		*pgt++ = pte + (i<<24);
    	}
    }
}

/**
 * mmu_1M_page_map Implementation (See header file for description)
*/
void mmu_1M_page_map(ulong_t *pgt, ulong_t p_addr, ulong_t v_addr, uint32_t size, ulong_t flags){
    uint32_t i;
    // Set entry flags
    ulong_t pte = flags;
    // first page table entry
    pgt = (ulong_t *)((uint32_t)pgt + (v_addr >> 18));
    pte |= (p_addr & 0xFFF00000);
    // Populate page table
    for(i=0;i<size;i++){
        *pgt++ = pte + (i<<20);
    }
}

/**
 * mmu_64K_page_map Implementation (See header file for description)
*/
void mmu_64K_page_map(ulong_t *pgt, ulong_t p_addr, ulong_t v_addr, uint32_t size, ulong_t flags){
    uint32_t i,j;
    // Set entry flags
    ulong_t pte = flags;
    pte |= (p_addr & 0xFFFF0000);
    // Second page table entry
    pgt = (ulong_t *)((uint32_t)pgt + ((v_addr & 0xFF000) >> 10));
    // Populate page table
    for(i=0;i<size;i++){
        for(j=0;j<16;j++){
            *pgt++ = pte + (i<<16);
        }
    }
}

/**
 * mmu_4K_page_map Implementation (See header file for description)
*/
void mmu_4K_page_map(ulong_t *pgt, ulong_t p_addr, ulong_t v_addr, uint32_t size, ulong_t flags){
    uint32_t i;
    // Set entry flags
    ulong_t pte = flags;
    pte |= (p_addr & 0xFFFFF000);
    // Second page table entry
    pgt = (ulong_t *)((uint32_t)pgt + ((v_addr & 0xFF000) >> 10));
    // Populate page table
    for(i=0;i<size;i++){
        *pgt++ = pte + (i<<12);
    }
}

/**
 * mmuAttachL2pgt Implementation (See header file for description)
*/
void mmuAttachL2pgt(pgt_t pgt, pgt2_t l2pgt, uint32_t v_addr){

	vaddr_t l2paddr = PageTablePhysicalAddress((ptr_t)l2pgt);

	ulong_t pte = ((ulong_t)l2paddr & 0xfffffc00);
    pte |= 0x1;
    pgt += ((v_addr & 0xFFF00000) >> 18);

    *((ulong_t *)pgt) = pte;
}


/* Private functions -------------------------------------- */

/**
 * mmuInitialization Implementation (See header file for description)
*/
int32_t mmuInitialization()
{
	// The MMU was already initialized in the boot process
	// No need to initialize page table allocators since we use the
	// Memory Manager directly
	return E_OK;
}

/**
 * MemoryKernelPageTableGet Implementation (See header file for description)
*/
pgt_t MemoryKernelPageTableGet()
{
	pgt_t pgt = NULL;
	asm volatile(
		"mrc	p15, 0, %[_pgt], c2, c0, 1		\n\t"
		: [_pgt] "=r" (pgt)
	);
	return pgt;
}

/**
 * MemorySynchronize Implementation (See header file for description)
*/
void MemorySynchronize()
{
	uint32_t state = 0;
	critical_lock(&state);

	// Make all data access visible
	dsb();
	// Cache clean operations
	FlushCaches();

	// Invalidate the data and instruction TLBs
	asm volatile("mov	r0, #0");
	//asm volatile("mcr	p15, 0, r0, c8, c7, 0");

	// Invalidate entire unified TLB Inner Shareable
	asm volatile("mcr	p15, 0, r0, c8, c3, 0");

//	asm volatile("mcr   p15, 0, r0, c7, c1, 6");

	InvalidateIcache();

	/* Invalidate entire unified TLB */
	//asm volatile ("mcr p15, 0, %0, c8, c7, 0" : : "r" (0));
	/* Invalidate entire data TLB */
	//asm volatile ("mcr p15, 0, %0, c8, c6, 0" : : "r" (0));
	/* Invalidate entire instruction TLB */
	//asm volatile ("mcr p15, 0, %0, c8, c5, 0" : : "r" (0));

	// Ensure completion of the Invalidation TLB operation
	dsb();
	// Ensure table changes visible to instruction fetch
	isb();

	critical_unlock(&state);
}

/**
 * MemoryVmaSynchronize Implementation (See header file for description)
*/
void MemoryVmaSynchronize(vaddr_t v_addr, uint32_t size, uint32_t pid)
{
	// NOTE: If ASID is zero we are synchronizing kernel memory so do not use the ASID
	//		 only the virtual address and flush the data cache since for kernel we are not
	//		 looking at the cache for TLB entries

	// This operation must be atomic
	uint32_t state = 0;
	critical_lock(&state);

	// Get Page align addresses
	uint32_t start = ((uint32_t)v_addr & ~(PAGE_SIZE - 1));
	uint32_t end = start + (size& ~(PAGE_SIZE - 1));

	// If not specified get current ASID
	if(pid == -1)
	{
		asm volatile("mrc p15, 0, %[pid], c13, c0, 1" : [pid] "=r" (pid) : : );
	}

	// Kernel memory
	if(pid == 0)
	{
		dsb();
		FlushDcache();
	}

	// Create initial MVA
	start |= pid;

	// Make all data access visible
	dsb();

	if(pid != 0)
	{
		for(; start < end; start += PAGE_SIZE)
		{
			asm volatile("mcr p15, 0, %[mva], c8, c3, 1" : : [mva] "r" (start));
		}
	}
	else
	{
		for(; start < end; start += PAGE_SIZE)
		{
			asm volatile("mcr p15, 0, %[mva], c8, c3, 3" : : [mva] "r" (start));
		}
	}

	// Invalidate Instruction cache since it is virtual indexed
	InvalidateIcache();

	dsb();
	isb();

	critical_unlock(&state);
}

/**
 * PageTableAlloc Implementation (See header file for description)
*/
pgt_t PageTableAlloc()
{
	// To ensure the 16k alignment we first allocate a full page table
	pgt_t pgt = (pgt_t)MemoryGetAligned(L1PGT_SIZE, L1PGT_ALIGN, ZONE_DIRECT);
	// Now we return to the Memory Manager the remaining 8k and
	// keep the lower 8k that have the 16k alignment
	MemoryFree((ptr_t)((uint32_t)pgt + L1PGT_USR_SIZE), L1PGT_USR_SIZE);

	memset(pgt, 0x0, L1PGT_USR_SIZE);

	return pgt;
}

/**
 * L1PageTableDealloc Implementation (See header file for description)
*/
void PageTableDealloc(pid_t pid, pgt_t pgt)
{
	MemoryMapClean(pgt);
	MemoryFree((ptr_t)(pgt), L1PGT_USR_SIZE);

	// Invalidate Unified TLB entry by ASID (pid) match Inner Shareable
	asm volatile (
			"dsb									\n\t"
			"mcr    p15, 0, %[_pid], c8, c3, 2		\n\t"
			"isb									\n\t"
			: : [_pid] "r" (pid)
	);
}

/**
 * MemoryMapClean Implementation (See header file for description)
*/
void MemoryMapClean(pgt_t pgt)
{
	/*
	 * This for a big page table is very inefficient!!!
	 * Is better to have a list of l2 page tables and deallocate them
	 * through that list. It may also be possible to have a better track
	 * of the usage of the page table!
	 */
	uint32_t *_pgt = (uint32_t*)pgt;
    /* Check if is necessary to deallocate L2 page tables */
	uint32_t i;
	for(i=0; i < (L1PGT_USR_SIZE >> 2); i++)
	{
		if(_pgt[i] & 0x1)
		{
			uint32_t *l2_pt = PageTableVirtualAddress((pgt_t)(_pgt[i] & 0xfffffc00));
			L2PageTableDealloc((pgt2_t)l2_pt);
		}
		_pgt[i] = 0x0;
	}
}

/**
 * MemoryMap Implementation (See header file for description)
*/
void MemoryMap(pgt_t pgt, paddr_t p_addr, vaddr_t v_addr, uint32_t size, memCfg_t *memCfg)
{
	ulong_t vaddr = (ulong_t)v_addr;
	ulong_t paddr = (ulong_t)p_addr;
	// Configure Page Table Entry flags
	ulong_t flags = GetPteFlags(
			cacheCfgs[memCfg->cpolicy],
			accessCfgs[memCfg->apolicy],
			memCfg->shared,
			memCfg->executable,
			memCfg->global);
	// Align address to the page size
	ulong_t addr = ALIGN_DOWN(vaddr, PAGE_SIZE);
	// Compute new size (in case the base address changed) and align it to the page size
	uint32_t length = ALIGN_UP(size + (vaddr & (PAGE_SIZE - 1)),PAGE_SIZE);
	// Map all memory
	while(length){
		// This algorithm divides the mapping memory in various chunks in order to always
		// choose the the best mapping (Super sections, Sections, Large pages, small pages)
		if((length >> 24) && !(addr << 8)  && !(paddr << 8)){
			// 16MB SUPERSECTION
			ulong_t pflags = (flags | SUPERSECTION);
			// Get the memory size that we can map using super sections
			uint32_t map_size = ALIGN_DOWN(length,LARGE_SECTION_SIZE);
			// Map physical to virtual
			mmu_16M_page_map((ulong_t*)pgt, paddr, addr, (map_size >> SUPERSECTION_SHIFT), pflags);
			// Update all control variables
			addr += map_size;
			paddr += map_size;
			length -= map_size;
		}
		else if((length >> 20) && !(addr << 12) && !(paddr << 12)){
			// 1M SECTION
			ulong_t pflags = (flags | SECTION);
			// Get the memory size that we can map using sections
			uint32_t map_size = ALIGN_DOWN(length,SECTION_SIZE);
			// Get the addr 24 less significant bits
			uint32_t aux = (addr & (LARGE_SECTION_SIZE - 1));
			// Ensure that we don't cross the super section alignment
			// If so only map the memory gap between the current virtual address and the next
			// virtual address with a super section alignment
			map_size = (((aux + map_size) > LARGE_SECTION_SIZE) ? (LARGE_SECTION_SIZE - aux) : (map_size));
			// Map physical to virtual
			mmu_1M_page_map((ulong_t*)pgt, paddr, addr, (map_size >> SECTION_SHIFT), pflags);
			// Update all control variables
			addr += map_size;
			paddr += map_size;
			length -= map_size;
		}
		else{
			// We are mapping a L2 page table entry
			ulong_t pte = (addr >> 20);
			pgt2_t l2pgt;
			// Check if there is already a L2 page table
			if(((ulong_t*)pgt)[pte] == 0x0){
				// If not allocate one
				l2pgt = L2PageTableAlloc();
				// Add entry to the L1 page table
				mmuAttachL2pgt(pgt, l2pgt, addr);
			}
			else{
				// Get the existing L2 page table
				l2pgt = PageTableVirtualAddress((pgt_t)(((ulong_t*)pgt)[pte] & 0xfffffc00));
			}
			if((length >> 16) && !(addr << 16) && !(paddr << 16)){
				// 64KB LARGEPAGE
				ulong_t pflags = MMU_FLAGS_LARGEPAGE(flags) | LARGEPAGE;
				// Get the memory size that we can map using large pages
				uint32_t map_size = ALIGN_DOWN(length,LARGE_PAGE_SIZE);
				// Get the addr 20 less significant bits
				uint32_t aux = (addr & (SECTION_SIZE - 1));
				// Ensure that we don't cross the section alignment
				// If so only map the memory gap between the current virtual address and the next
				// virtual address with a section alignment
				map_size = (((aux + map_size) > SECTION_SIZE) ? (SECTION_SIZE - aux) : (map_size));
				// Map physical to virtual
				mmu_64K_page_map((ulong_t*)l2pgt, paddr, addr, (map_size >> LARGEPAGE_SHIFT),pflags);
				// Update all control variables
				addr += map_size;
				paddr += map_size;
				length -= map_size;
			}
			else{
				// 4KB SMALLPAGE
				ulong_t pflags = MMU_FLAGS_SMALLPAGE(flags) | SMALLPAGE;
				// Get the memory size that we can map using small pages
				uint32_t map_size = ALIGN_DOWN(length,PAGE_SIZE);
				// Get the addr 16 less significant bits
				uint32_t aux = (addr & (LARGE_PAGE_SIZE - 1));
				// Ensure that we don't cross the large page alignment
				// If so only map the memory gap between the current virtual address and the next
				// virtual address with a large page alignment
				map_size = (((aux + map_size) > LARGE_PAGE_SIZE) ? (LARGE_PAGE_SIZE - aux) : (map_size));
				// Map physical to virtual
				mmu_4K_page_map((ulong_t*)l2pgt, paddr, (ulong_t)addr, (map_size >> SMALLPAGE_SHIFT),pflags);
				// Update all control variables
				addr += map_size;
				paddr += map_size;
				length -= map_size;
			}
		}
	}
}

/**
 * MemoryUnmap Implementation (See header file for description)
*/
void MemoryUnmap(pgt_t pgt, vaddr_t v_addr, uint32_t size)
{
	ulong_t vaddr = (ulong_t)v_addr;
	// Adjust virtual address to the minimum page size
	vaddr &= ~(PAGE_SIZE - 1);
	// Get offset from the beginning of the page table
	uint32_t offset = (vaddr >> 20);
	// L1 entries unmap or L2 entries unmap
	if((size >> 20) && !(vaddr << 12))
	{
		uint32_t i;
		for(i=offset;i<((size >> 20) + offset);i++)
		{
			// Check if is necessary to deallocate a L2 page tables
			if(((ulong_t*)pgt)[i] & 0x1)
			{
				vaddr_t l2_pt = PageTableVirtualAddress((ptr_t)(((ulong_t*)pgt)[i] & 0xfffffc00));
				L2PageTableDealloc((pgt2_t)l2_pt);
			}
			// Clean entry
			((ulong_t*)pgt)[i] = 0x0;
		}
		if(size & (SECTION_SIZE - 1))
		{
			// Resolve pending memory sections in a recursive way
			MemoryUnmap(pgt, (vaddr_t)(vaddr + (size & ~(SECTION_SIZE - 1))), (size & (SECTION_SIZE - 1)));
		}
	}
	else{
		// Get L2 Page Table
		ulong_t* l2pgt = PageTableVirtualAddress((ptr_t)(((ulong_t*)pgt)[offset] & 0xfffffc00));

		// Get base entry for the virtual address
		l2pgt += ((vaddr & 0xFF000) >> 12);
		// Check if the memory section pass the 1MB boundary
		uint32_t overflow = ((vaddr & (SECTION_SIZE-1)) + size);
		// If the memory section overflows to the next 1MB section
		if(overflow > SECTION_SIZE)
		{
			// Compute memory section size being unmap in the first section
			uint32_t unmap_size = SECTION_SIZE - (vaddr & (SECTION_SIZE-1));
			uint32_t i;
			// Clear entries
			for(i=0; i<unmap_size; i+=PAGE_SIZE)
			{
				*l2pgt++ = 0x0;
			}
			// Compute the memory size to be unmapped in the next iteration
			size -= unmap_size;
			if(size)
			{
				// Resolve pending memory sections in a recursive way
				MemoryUnmap(pgt, (vaddr_t)(vaddr + unmap_size), size);
			}
		}
		else{
			// Clear entries
			uint32_t i;
			for(i=0; i<size; i+=PAGE_SIZE)
			{
				*l2pgt++ = 0x0;
			}
		}
	}
}

/**
 * MemoryVirtual2physical Implementation (See header file for description)
*/
paddr_t MemoryVirtual2physical(pgt_t pgt, vaddr_t v_addr)
{
	ulong_t vaddr = (ulong_t)v_addr;
	ulong_t p_addr = 0x0;
    //ulong_t *pte = (uint32_t*)((((uint32_t)pgt)&0xFFFFC000)|((vaddr >> 18)&(~0x3)));
    ulong_t *pte = (ulong_t*)(((uint32_t)pgt)+((vaddr >> 18)&(~0x3)));
    uint32_t entry = *pte;
    if(entry & 0x2)
    {
        // 16MB entry
        if(entry & 0x40000)
        {
            p_addr = (entry & 0xFF000000)| (vaddr & 0xFFFFFF);
        }
        else
        {
            // 1MB entry
            p_addr = (entry & 0xFFF00000)| (vaddr & 0xFFFFF);
        }
    }
    else if(entry & 0x1)
    {
        //uint32_t *l2_entry = (uint32_t*)((entry & 0xfffffc00) | ((vaddr & 0xFF000) >> 10));
        ulong_t *l2_entry = PageTableVirtualAddress((pgt_t)((entry & 0xfffffc00) | ((vaddr & 0xFF000) >> 10)));
        entry = *l2_entry;
        if(entry & 0x2)
        {
			// 4KB entry
			p_addr = (entry & 0xFFFFF000) | (vaddr & 0xFFF);
        }
        else if(entry & 0x1)
        {
		// 64KB entry
			p_addr = (entry & 0xFFFF0000) | (vaddr & 0xFFFF);
		}
    }
    return (paddr_t)p_addr;
}
