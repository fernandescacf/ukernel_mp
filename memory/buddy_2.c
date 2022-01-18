/**
 * @file        buddy.c
 * @author      Carlos Fernandes
 * @version     3.0
 * @date        02 September, 2020
 * @brief       Buddy system implementation
*/

/* Includes ----------------------------------------------- */
#include <buddy.h>
#include <kheap.h>
#include <string.h>


/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */


/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */

buddy_t _rawBuddy;


/* Private function prototypes ---------------------------- */

/*
 * @brief   Convert a page size to a base two value
 * @param  	size - page size
 * @retval  Base two value
 */
uint32_t GetOrder(uint32_t size)
{
	size = ((size & (-size)) >> 13);
	uint32_t order = 0;
	while(size  && order < MAX_ORDER){
		order++;
		size >>= 1;
	}
	return order;
}

/*
 * @brief   Get from a non log2 size the max base 2 order
 * @param  	size - page size
 * @retval  Base two value
 */
uint32_t GetMaxOrder(uint32_t size)
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

ptr_t BlockGet(mBlock_t **Blocks, uint32_t order)
{
	if(Blocks[order] != NULL)
	{
		// Remove Head
		ptr_t addr = (ptr_t)Blocks[order];
		Blocks[order] = Blocks[order]->next;
		if(Blocks[order]) { Blocks[order]->prev = NULL; }
		return addr;
	}

	return NULL;
}

void BlockRemove(mBlock_t **Blocks, mBlock_t *mBlock, uint32_t order)
{
	if(mBlock->prev == NULL)
	{
		Blocks[order] = mBlock->next;
	}
	else
	{
		mBlock->prev->next = mBlock->next;
	}

	if(mBlock->next != NULL)
	{
		mBlock->next->prev = mBlock->prev;
	}


	mBlock->next = NULL;
	mBlock->prev = NULL;
}

bool_t IsBuddy(ptr_t left, ptr_t right, uint32_t order)
{
	// Left has to be order + 1 aligned
	if(GetOrder((uint32_t)left) <= order)
	{
		return FALSE;
	}

	// left and right have to be contiguous
	if(((uint32_t)left + (PAGE_SIZE << order)) != (uint32_t)right)
	{
		return FALSE;
	}

	return TRUE;
}

void BlockInsert(mBlock_t **Blocks, mBlock_t *mBlock, uint32_t order)
{
	// Get the first memory block with the same order
	mBlock_t *iterator = Blocks[order];

	// Handler insert Head
	if(iterator == NULL)
	{
		Blocks[order] = mBlock;
		mBlock->next = NULL;
		mBlock->prev = NULL;
		return;
	}

	// Sort Insert
	while(iterator)
	{
		// Insert back
		if((uint32_t)iterator > (uint32_t)mBlock)
		{
			mBlock->next = iterator;
			mBlock->prev = iterator->prev;
			if(iterator->prev != NULL)
			{
				iterator->prev->next = mBlock;
			}
			else
			{
				// Insert in the list head
				Blocks[order] = mBlock;
			}
			iterator->prev = mBlock;
			// Block is inserted return
			return;
		}
		// Insert front
		if(iterator->next == NULL)
		{
			mBlock->next = NULL;
			mBlock->prev = iterator;
			iterator->next = mBlock;
			// Block is inserted return
			return;
		}
		// Advance one block
		iterator = iterator->next;
	}
}

void BlockInsertMerge(mBlock_t **Blocks, mBlock_t *mBlock, uint32_t order)
{
	// Get the first memory block with the same order
	mBlock_t *iterator = Blocks[order];

	// Handler insert Head (no merge needed)
	if(iterator == NULL)
	{
		Blocks[order] = mBlock;
		mBlock->next = NULL;
		mBlock->prev = NULL;
		return;
	}

	// Sort Insert
	while(iterator)
	{
		// Insert back
		if((uint32_t)iterator > (uint32_t)mBlock)
		{
			if(IsBuddy(mBlock, iterator, order) == TRUE)
			{
				// Remove and reinsert
				BlockRemove(Blocks, iterator, order);
				BlockInsertMerge(Blocks, mBlock, order + 1);
				return;
			}

			if(iterator->prev && (IsBuddy(iterator->prev, mBlock, order) == TRUE))
			{
				// Remove iterator->prev and reinsert
				mBlock_t *prev = iterator->prev;
				BlockRemove(Blocks, prev, order);
				BlockInsertMerge(Blocks, prev, order + 1);
				return;
			}

			// Not a buddy so just insert
			mBlock->next = iterator;
			mBlock->prev = iterator->prev;
			if(iterator->prev != NULL)
			{
				iterator->prev->next = mBlock;
			}
			else
			{
				// Insert in the list head
				Blocks[order] = mBlock;
			}
			iterator->prev = mBlock;
			// Block is inserted return
			return;
		}
		// Insert front
		if(iterator->next == NULL)
		{
			if(IsBuddy(iterator, mBlock, order) == TRUE)
			{
				// Remove and reinsert
				BlockRemove(Blocks, iterator, order);
				BlockInsertMerge(Blocks, iterator, order + 1);
				return;
			}

			// Not a buddy so just insert
			mBlock->next = NULL;
			mBlock->prev = iterator;
			iterator->next = mBlock;
			// Block is inserted return
			return;
		}
		// Advance one block
		iterator = iterator->next;
	}
}

/* Private functions -------------------------------------- */

/**
 * buddySystemCreate Implementation (See header file for description)
*/
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

	KlockInit(&buddy->lock);

	return buddy;
}

/**
 * buddyInit Implementation (See header file for description)
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
		order = GetOrder(base);
		// See if we do not cross the top address
		base += (PAGE_SIZE << order);
		while(base > end){
			// Get the higher order that fits into the memory block
			order--;
			base -= (PAGE_SIZE << order);
		}
		// Insert memory block into the buddy system
		BlockInsertMerge(buddy->mBlocks, mblock, order);
	}
	return 0;
}

/**
 * buddyGetMemory Implementation (See header file for description)
*/
ptr_t buddyGetMemory(buddy_t *buddy, uint32_t size)
{
	uint32_t status;

	// Is size supported
	if(size > (PAGE_SIZE << MAX_ORDER))
	{
		return NULL;
	}

	uint32_t order = GetOrder(size);

	// Check if the size we receive is log2
	if(size & ~(PAGE_SIZE << order))
	{
		return NULL;
	}

	Klock(&buddy->lock, &status);

	ptr_t addr = BlockGet(buddy->mBlocks, order);

	if(addr == NULL)
	{
		addr = buddyGetMemory(buddy, (size << 1));

		if(addr == NULL)
		{
			Kunlock(&buddy->lock, &status);
			return NULL;
		}

		// Insert top half of this memory block (right buddy)
		BlockInsert(buddy->mBlocks, (mBlock_t*)((uint32_t)addr + size), order);
	}

	Kunlock(&buddy->lock, &status);

	return addr;
}

/**
 * buddyFreeMemory Implementation (See header file for description)
*/
void buddyFreeMemory(buddy_t *buddy, ptr_t memory, uint32_t size)
{
	uint32_t status;
	Klock(&buddy->lock, &status);

	while(size)
	{
		uint32_t addr_order = GetOrder((uint32_t)memory);
		uint32_t size_order = GetMaxOrder((uint32_t)size);

		if(size_order >= addr_order)
		{
			BlockInsertMerge(buddy->mBlocks,(mBlock_t*)memory, addr_order);
			size -= (PAGE_SIZE << addr_order);
			memory = (ptr_t)((uint32_t)memory + (PAGE_SIZE << addr_order));
		}
		else
		{
			BlockInsertMerge(buddy->mBlocks,(mBlock_t*)memory, size_order);
			size -= (PAGE_SIZE << size_order);
			memory = (ptr_t)((uint32_t)memory + (PAGE_SIZE << size_order));
		}
	}

	Kunlock(&buddy->lock, &status);
}
