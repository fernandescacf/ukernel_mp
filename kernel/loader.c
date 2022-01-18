/**
 * @file        loader.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        28 January, 2020
 * @brief       Loader implementation
*/

/* Includes ----------------------------------------------- */

#include <loader.h>
#include <elf.h>
#include <kvspace.h>
#include <memmgr.h>
#include <kheap.h>
#include <string.h>


/* Private types ------------------------------------------ */

typedef struct
{
	glistNode_t	node;
	loadInf_t	info;
	uint16_t	refs;
	uint16_t	cmdLen;
	char		cmd[1];
}loadImg_t;

typedef struct
{
	size_t		cmdLen;
	const char	*cmd;
}imgComp_t;

/* Private constants -------------------------------------- */



/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */

static struct
{
	glist_t		imgs;
}loaderHandler;


/* Private function prototypes ---------------------------- */

/*
 * @brief   Helper function to be used by the Loader imgs list to sort
 * 			the list
 *
 * @param   c -	not used
 * 			n -	not usec
 *
 * @retval  Always 0 -> always insert in the head
 */
static int32_t ExecListSort(glistNode_t *c, glistNode_t *n)
{
	// Always insert in the list head
	(void)c;
	(void)n;
    return 0;
}

/*
 * @brief   Helper function to be used by the Loader imgs list to find
 * 			images already loaded
 *
 * @param   current -	current node being compared in the list
 * 			match -		imgComp_t that stores the data to compare
 *
 * @retval  Comparation result
 */
static int32_t ExecListCmp(glistNode_t *current, void *match)
{
	loadImg_t *cImg = GLISTNODE2TYPE(current, loadImg_t, node);
	imgComp_t *cmd = (imgComp_t *)match;

	if(cImg->cmdLen != (uint16_t)cmd->cmdLen)
	{
		return -1;
	}
	else
	{
		return memcmp(cImg->cmd, cmd->cmd, cmd->cmdLen);
	}
}

/*
 * @brief   Routine to load the elf data to RAM
 *
 * @param   data -		Pointer to elf data
 * 			fileSize -	Size of the data present in the elf file
 * 			totalSize - Total size (present in the elf file and memory set to zero)
 * 			memory -	memory list to stored the memory allocated to load
 * 						the segment
 *
 * @retval  Return Success
 */
static int32_t LoadMemory(int8_t *data, size_t fileSize, size_t totalSize, glist_t *memory);

/*
 * @brief   Routine to load a segment to memory
 *
 * @param   segment -	segment to be loaded
 * 			memory -	memory list to stored the memory allocated to load
 * 						the segment
 *
 * @retval  Pointer to the image info or NULL if not found
 */
static int32_t LoadSegment(segment_t *segment, glist_t *memory);

/*
 * @brief   Routine to check if the elf is already loaded
 *
 * @param   len -	elf name length
 * 			cmd -	execution command
 *
 * @retval  Pointer to the image info or NULL if not found
 */
static loadImg_t *LoaderSearchImg(uint16_t len, const char *cmd)
{
	imgComp_t cmp;
	cmp.cmdLen = (size_t)len;
	cmp.cmd = cmd;

	return GLISTNODE2TYPE(GlistGetObject(&loaderHandler.imgs, (void *)&cmp), loadImg_t, node);
}

/*
 * @brief   Routine to free allocated memory
 *
 * @param   memory -	list of memory to be free
 *
 * @retval  No return
 */
static void LoaderFreeMemory(glist_t *memory);

/*
 * @brief   Routine to allocate memory
 *
 * @param   size -	size of the memory being allocated
 *
 * @retval  Return a memory object or NULL in case of error
 */
static mmobj_t *LoaderGetMemoryObj(size_t size);

/*
 * @brief   Routine to extract the elf name length
 *
 * @param   cmd -	Execution command
 *
 * @retval  Return elf name length
 */
inline static uint16_t ElfNameLength(const char *cmd)
{
	uint16_t len;
	for(len = 0; ((cmd[len] != ' ') && (cmd[len] != '\0')); len++);
	return len;
}


/* Private functions -------------------------------------- */

/**
 * LoaderInitialize Implementation (See header file for description)
*/
int32_t LoaderInitialize(void)
{
	(void)GlistInitialize(&loaderHandler.imgs, GList);
	(void)GlistSetSort(&loaderHandler.imgs, ExecListSort);
	(void)GlistSetCmp(&loaderHandler.imgs, ExecListCmp);

	return E_OK;
}

/**
 * LoaderLoadElf Implementation (See header file for description)
*/
int32_t LoaderLoadElf(void *raw, char *cmd, exec_t *exec)
{
	elf_t elf;
	segment_t segment;

	if(cmd == NULL || exec == NULL)
	{
		return E_INVAL;
	}

	if(ElfParse(raw, &elf) != E_OK)
	{
		return E_INVAL;
	}

	uint16_t len = ElfNameLength(cmd);

	loadImg_t *img = LoaderSearchImg(len, cmd);

	if(img == NULL)
	{
		img = (loadImg_t*)kmalloc(sizeof(loadImg_t) + len);
		memset(img, 0x0, sizeof(loadImg_t));

		if(ElfGetSymbolValue(&elf, &img->info.entry, "_start") != E_OK)
		{
			kfree(img, sizeof(*img));
			return E_INVAL;
		}

		if(ElfGetSymbolValue(&elf, &img->info.exit, "_exit") != E_OK)
		{
			kfree(img, sizeof(*img));
			return E_INVAL;
		}

		if(ElfGetSymbolValue(&elf, &img->info.baseAddr, "_text_start") != E_OK)
		{
			kfree(img, sizeof(*img));
			return E_INVAL;
		}

		if(ElfGetSymbolValue(&elf, &img->info.topAddr, "_bss_end") != E_OK)
		{
			kfree(img, sizeof(*img));
			return E_INVAL;
		}

		section_t textSection;
		if(ElfFindSection(&elf, ".text", &textSection) != E_OK)
		{
			kfree(img, sizeof(*img));
			return E_INVAL;
		}

		img->info.text.addr = textSection.addr;
		img->info.text.size = textSection.size;

		section_t dataSection;
		if(ElfFindSection(&elf, ".data", &dataSection) == E_OK)
		{
			img->info.data.addr = dataSection.addr;
			img->info.data.size = dataSection.size;
		}

		section_t bssSection;
		if(ElfFindSection(&elf, ".bss", &bssSection) == E_OK)
		{
			img->info.bss.addr = bssSection.addr;
			img->info.bss.size = bssSection.size;
		}

		// Segment structure has to be initialize with zeros
		memset(&segment, 0x0, sizeof(segment));
		// Get first segment it should be the .text and .rodata segment
		if(ElfGetNextSegment(&elf, &segment) != E_OK)
		{
			kfree(img, sizeof(*img));
			return E_INVAL;
		}

		// Initialize text.memory list as FIFO (always insert on the tail and get from head)
		(void)GlistInitialize(&img->info.text.memory, GFifo);
		if(LoadSegment(&segment, &img->info.text.memory) != E_OK)
		{
			kfree(img, sizeof(*img));
			return E_INVAL;
		}

		img->cmdLen = len;
		(void)memcpy(img->cmd, cmd, len);

		(void)GlistInsertObject(&loaderHandler.imgs, &img->node);
	}
	else
	{
		// Segment structure has to be initialize with zeros
		memset(&segment, 0x0, sizeof(segment));
		// Skip text segment
		if(ElfGetNextSegment(&elf, &segment) != E_OK)
		{
			return E_INVAL;
		}
	}

	// Initialize dataMemory list as FIFO (always insert on the tail and get from head)
	(void)GlistInitialize(&exec->private.dataMemory, GFifo);
	if(ElfGetNextSegment(&elf, &segment) == E_OK)
	{
		if(LoadSegment(&segment, &exec->private.dataMemory) != E_OK)
		{
			exec->load = NULL;
			if(img->refs == 0)
			{
				GlistRemoveSpecific(&img->node);
				LoaderFreeMemory(&img->info.text.memory);
				kfree(img, sizeof(*img));
			}

			return E_INVAL;
		}
	}

	exec->load = &img->info;

	img->refs += 1;

	return E_OK;
}

/**
 * LoaderUnloadElf Implementation (See header file for description)
*/
int32_t LoaderUnloadElf(exec_t *exec)
{
	// Get loadImg structure. GLISTNODE2TYPE is from glist but does the trick :-)
	loadImg_t *img = GLISTNODE2TYPE(exec->load, loadImg_t, info);

	img->refs -= 1;

	// Unload process private memory
	LoaderFreeMemory(&exec->private.dataMemory);

	// If there are no more references delete the elf image
	if(img->refs == 0)
	{
		GlistRemoveSpecific(&img->node);
		LoaderFreeMemory(&img->info.text.memory);
		kfree(img, sizeof(*img) + img->cmdLen);
	}

	return E_OK;;
}

/**
 * LoadSegment Implementation (See Private function prototypes section for description)
*/
int32_t LoadSegment(segment_t *segment, glist_t *memory)
{
	switch(segment->type)
	{
	case PT_LOAD:						// Loadable program segment
		return LoadMemory(segment->data, segment->size_file, segment->size_mem, memory);
		break;
	case PT_INTERP:						// Program interpreter
		// Eventually do something with this!!!!
		break;
	case PT_NULL:						// Program header table entry unused
	case PT_DYNAMIC:					// Dynamic linking information
	case PT_NOTE:						// Auxiliary information
	case PT_SHLIB:						// Reserved, unspecified semantics
	case PT_PHDR:						// Entry for header table itself
	case PT_NUM:						// Number of p types
		break;
	}

	return E_OK;
}

/**
 * LoadMemory Implementation (See Private function prototypes section for description)
*/
int32_t LoadMemory(int8_t *data, size_t fileSize, size_t totalSize, glist_t *memory)
{
	size_t loaded = 0;

	while(loaded < fileSize)
	{
		// 1) Get memory in Page size blocks
		mmobj_t *obj = LoaderGetMemoryObj(PAGE_SIZE);
		if(obj == NULL)
		{
			LoaderFreeMemory(memory);
			return E_NO_RES;
		}
		// 2) Temporarily map memory to perform the copy
		vaddr_t vaddr = VirtualSpaceMmap(obj->addr, PAGE_SIZE);
		// 3) Check if we have enough data to fill a page
		size_t copySize = (((fileSize - loaded) >= PAGE_SIZE) ? PAGE_SIZE :  (fileSize - loaded));
		// 4) Copy data to page
		memcpy(vaddr, data, copySize);
		// 5) If we didn't fill the whole page set remaining memory to zeros
		if(copySize < PAGE_SIZE)
		{
			memset((void*)((uint32_t)(vaddr) + copySize), 0x0, (PAGE_SIZE - copySize));
		}
		// 6) Unmap page
		VirtualSpaceUnmmap(vaddr);
		// 7) Update data pointer
		data = (int8_t *)((uint32_t)(data) + PAGE_SIZE);
		// 8) Insert page in the memory list (always insert on the list tail)
		(void)GlistInsertObject(memory, &obj->node);
		// 9) Update loaded memory
		loaded += PAGE_SIZE;
	}

	while(loaded < totalSize)
	{
		// 1) Get memory in Page size blocks
		mmobj_t *obj = LoaderGetMemoryObj(PAGE_SIZE);
		if(obj == NULL)
		{
			LoaderFreeMemory(memory);
			return E_NO_RES;
		}
		// 2) Temporarily map memory to perform the copy
		vaddr_t vaddr = VirtualSpaceMmap(obj->addr, PAGE_SIZE);
		// 3) Set page memory to zeros
		memset(vaddr, 0x0, PAGE_SIZE);
		// 4) Unmap page
		VirtualSpaceUnmmap(vaddr);
		// 5) Insert page in the memory list (always insert on the list tail)
		(void)GlistInsertObject(memory, &obj->node);
		// 6) Update loaded memory
		loaded += PAGE_SIZE;
	}

	return E_OK;
}

/**
 * LoaderFreeMemory Implementation (See Private function prototypes section for description)
*/
void LoaderFreeMemory(glist_t *memory)
{
	while(memory->count > 0)
	{
		mmobj_t *obj = GLISTNODE2TYPE(GlistRemoveObject(memory, NULL), mmobj_t, node);
		MemoryFree(obj->addr, obj->size);
		kfree(obj, sizeof(*obj));
	}
}

/**
 * LoaderGetMemoryObj Implementation (See Private function prototypes section for description)
*/
mmobj_t *LoaderGetMemoryObj(size_t size)
{
	mmobj_t *obj = (mmobj_t*)kmalloc(sizeof(mmobj_t));
	if(obj == NULL)
	{
		return NULL;
	}
	obj->size = size;
	obj->addr =  (paddr_t)MemoryGet(obj->size, ZONE_INDIRECT);
	if(obj->addr == NULL)
	{
		kfree(obj, sizeof(*obj));
		return NULL;
	}
	return obj;
}
