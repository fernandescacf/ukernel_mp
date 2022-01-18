/**
 * @file        buddy.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        24 December, 2016
 * @brief       Buddy system implementation
*/

/* Includes ----------------------------------------------- */
#include <buddy.h>
#include <kheap.h>
#include <string.h>


/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */


/* Private macros ----------------------------------------- */

#define GET_ORDER(x)		mBlockOrder(x)


/* Private variables -------------------------------------- */

buddy_t _rawBuddy;


/* Private function prototypes ---------------------------- */

/*
 * @brief   Inserts a memory page in the free pages list
 * @param   page - memory page
 * 			order - memory page size
 * @retval  No return
 */
void mBlockInsert(mBlock_t **mBlocks, mBlock_t *mBlock, uint32_t order);

/*
 * @brief   Tries to merge to contiguous pages if the resulting
 * 			memory page is aligned to the next order
 * @param   page - memory page
 * 			order - page size
 * @retval  No return
 */
void mBlockMerge(mBlock_t **mBlocks, mBlock_t *mBlock, uint32_t order);

/*
 * @brief   Convert a page size to a base two value
 * @param  	size - page size
 * @retval  Base two value
 */
uint32_t mBlockOrder(uint32_t size);


/* Private functions -------------------------------------- */

buddy_t *buddySystemCreate(ptr_t pAddr, ptr_t vAddr, uint32_t size, uint32_t offset)
{
	buddy_t *buddy = (buddy_t*)kmalloc(sizeof(buddy_t));

	if(!buddy){
		buddy = &_rawBuddy;
	}

	memset(buddy,0x0,sizeof(buddy_t));

	buddy->pAddr = pAddr;
	buddy->vAddr = vAddr;
	buddy->size = size;
	buddy->offset = offset;
	buddy->availableMemory = size - offset;

	return buddy;
}

/**
 * buddy_system_init Implementation (See header file for description)
*/
int32_t buddyInit(buddy_t *buddy)
{
	uint32_t 	order = 0;
	mBlock_t	*mblock = NULL;
	uint32_t	base = (uint32_t)((uint32_t)buddy->vAddr + buddy->offset);
	uint32_t	end = base + buddy->availableMemory;
	// Determine the higher order possible to speedup map the block into the buddy system
	while(base < end){
		mblock = (mBlock_t*)base;
		// Get the memory block alignment
		order = GET_ORDER(base);
		// See if we do not cross the top address
		base += (PAGE_SIZE << order);
		while(base > end){
			// Get the higher order that fits into the memory block
			order--;
			base -= (PAGE_SIZE << order);
		}
		// Insert memory block into the buddy system
		mBlockInsert(buddy->mBlocks,mblock,order);
	}
	return 0;
}

/**
 * K_Page_merge Implementation (See header file for description)
*/
void mBlockMerge(mBlock_t **Blocks, mBlock_t *mBlock, uint32_t order)
{
	if(order >= MAX_ORDER){
		return;
	}
    uint32_t size = (PAGE_SIZE << order);

    if(mBlock->prev != NULL && ((uint32_t)mBlock->prev + size) == (uint32_t)mBlock){
        /* Only merge the pages if new page has the right alignment*/
        if(((uint32_t)mBlock->prev) << (20 - order)) return;
        /* If prev and page are contiguous merge them */
        if(mBlock->prev->prev == NULL){
        	Blocks[order] = mBlock->next;
            if(mBlock->next){
            	mBlock->next->prev = NULL;
            }
        }
        else{
        	mBlock->prev->prev->next = mBlock->next;
            if(mBlock->next){
            	mBlock->next->prev = mBlock->prev->prev;
            }
        }
        /* Insert new page in the next order list */
        mBlockInsert(Blocks,mBlock->prev,order+1);
    }
    else if(mBlock->next != NULL && ((uint32_t)mBlock + size) == (uint32_t)mBlock->next){
        /* Only merge the pages if new page has the right alignment*/
        if(((uint32_t)mBlock) << (20 - order)) return;
        /* If page and next are contiguous merge them */
        if(mBlock->prev == NULL){
        	Blocks[order] = mBlock->next->next;
            if(mBlock->next->next){
            	mBlock->next->next->prev = NULL;
            }
        }
        else{
        	mBlock->prev->next = mBlock->next->next;
            if(mBlock->next->next){
            	mBlock->next->next->prev = mBlock->prev;
            }
        }
        /* Insert new page in the next order list */
        mBlockInsert(Blocks,mBlock,order+1);
    }
}

/**
 * K_Page_insert Implementation (See header file for description)
*/
void mBlockInsert(mBlock_t **Blocks, mBlock_t *mBlock, uint32_t order)
{
    /* Get the first memory block with the same order */
    mBlock_t *iterator = Blocks[order];
    /* Set the memory block size */
    mBlock->size = (PAGE_SIZE << order);
    /* Check if there isn't any memory block with the same order */
    if(iterator == NULL){
        /* Set the new memory block as the first for the specified order */
    	Blocks[order] = mBlock;
    	mBlock->next = NULL;
    	mBlock->prev = NULL;
        return;
    }
    while(iterator){
        /* The page list is ordered by low address first high address last */
        if((uint32_t)iterator > (uint32_t)mBlock){
            if(iterator->prev){
                iterator->prev->next = mBlock;
            }
            else{
                /* Insert in the list head */
            	Blocks[order] = mBlock;
            }
            mBlock->prev = iterator->prev;
            mBlock->next = iterator;
            iterator->prev = mBlock;
            /* Check if is possible to merge the new page with a contiguous page */
            mBlockMerge(Blocks,mBlock,order);
            return;
        }
        else if(iterator->next == NULL){
            /* Insert in the end of the list */
            iterator->next = mBlock;
            mBlock->prev = iterator;
            mBlock->next = NULL;
            /* Check if is possible to merge the new page with a contiguous page */
            mBlockMerge(Blocks,mBlock,order);
            return;
        }
        else{
            iterator = iterator->next;
        }
    }
}

uint32_t mBlockOrder(uint32_t size)
{
	size = ((size & (-size)) >> 13);
	uint32_t order = 0;
	while(size  && order < 10){
		order++;
		size >>= 1;
	}
	return order;
}

/**
 * K_Page_get Implementation (See header file for description)
*/
ptr_t buddyGetMemory(buddy_t *buddy, uint32_t size)
{
	uint32_t order = GET_ORDER(size);
	ptr_t *addr = (ptr_t)NULL;
	mBlock_t **Blocks = buddy->mBlocks;
    if(Blocks[order]){
        addr = (ptr_t)Blocks[order];
        Blocks[order] = Blocks[order]->next;
		if(Blocks[order]){Blocks[order]->prev = NULL;}
    }
    else{
        uint32_t i;
		for(i = order + 1; i < MAX_ORDER && Blocks[i]==NULL; i++);
        if(i <= MAX_ORDER){
        	mBlock_t *get = Blocks[i];
            addr = (ptr_t)get;
            Blocks[i] = Blocks[i]->next;
            if(Blocks[i]){Blocks[i]->prev = NULL;}
            do{
                i--;
                get->size = get->size >> 1;
                uint32_t base = ((uint32_t)get + get->size);
                mBlockInsert(Blocks,(mBlock_t*)base,i);
            }while(order < i);
        }
    }
    return addr;
}

uint32_t maxorder(uint32_t size)
{
	uint32_t count = 0;
	size >>= 13;

	while(size && count < 10)
	{
		size >>= 1;
		count++;
	}

	return count;
}

void buddyFreeMemory(buddy_t *buddy, ptr_t memory, uint32_t size)
{
	while(size)
	{
		uint32_t addr_order = GET_ORDER((uint32_t)memory);
		uint32_t size_order = maxorder((uint32_t)size);

		if(size_order >= addr_order)
		{
			mBlockInsert(buddy->mBlocks,(mBlock_t*)memory, addr_order);
			size -= (4096 * (addr_order + 1));
			memory = (ptr_t)((uint32_t)memory + (4096 * (addr_order + 1)));
		}
		else
		{
			mBlockInsert(buddy->mBlocks,(mBlock_t*)memory, size_order);
			size -= (4096 * (size_order + 1));
			memory = (ptr_t)((uint32_t)memory + (4096 * (size_order + 1)));
		}
	}
}

/**
 * K_Page_free Implementation (See header file for description)
*/
/*
void buddyFreeMemory(buddy_t *buddy, ptr_t memory, uint32_t size)
{
	uint32_t order = GET_ORDER(size);
	mBlockInsert(buddy->mBlocks,(mBlock_t*)memory,order);
}
*/
