/**
 * @file        rfs.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        28 May, 2020
 * @brief       Raw File System implementation
*/

/* Includes ----------------------------------------------- */
#include <rfs.h>
#include <procmgr.h>
#include <devices.h>


/* Private types ------------------------------------------ */
typedef struct
{
    uint32_t type;
    uint32_t version;
    uint32_t arch;
    uint32_t machine;
    size_t   fs_size;
    uint32_t script_off;
    uint32_t script_cmds;
    uint32_t ram_off;
	uint32_t irq_off;
    uint32_t devices_off;
    uint32_t devices_count;
    uint32_t names_off;
    uint32_t names_size;
    uint32_t files_off;
    uint32_t files_count;
}header_t;

typedef struct
{
    uint32_t type;
    uint16_t prio;
    uint16_t priv;
    uint32_t file_off;
    uint32_t cmd_off;
}cmd_t;

typedef struct
{
    uint32_t addr;
    size_t   size;
}ram_t;

typedef struct
{
    uint32_t priv;
    size_t   shared;
}intr_t;

typedef struct
{
    uint32_t addr;
    size_t   size;
    uint32_t access;
    uint32_t name_off;
}device_t;

typedef struct
{
    uint32_t type;          // exec, lib, file
    size_t   size;
    uint32_t data_off;
    uint32_t name_off;
}file_t;


/* Private constants -------------------------------------- */
#define RFS_TYPE	0xCACFCACF

#define EXEC_TYPE	0x1
#define LIB_TYPE	0x2
#define OBJ_TYPE	0x3



/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */

static struct
{
	header_t	*hdr;
	cmd_t		*cmds;
	ram_t		*ram;
	intr_t      *intr;
	device_t	*devices;
	file_t		*files;
	char        *strings;
	bool_t		locked;
}Rfs;


/* Private function prototypes ---------------------------- */

char *RfsGetString(uint32_t offset)
{
	if(offset >= Rfs.hdr->names_size)
	{
		return NULL;
	}

	return &Rfs.strings[offset];
}

file_t *RfsGetFile(uint32_t offset)
{

	return (file_t *)((uint32_t)Rfs.hdr + offset);
}


/* Private functions -------------------------------------- */

int32_t RfsInit(vaddr_t rfs, size_t size)
{
	// Validate Raw File System object
	if(((header_t*)rfs)->type != RFS_TYPE)
	{
		return E_INVAL;
	}

	// Get RFS header
	Rfs.hdr = (header_t*)rfs;

	// Correct size to respect the page size boundaries
//	if(Rfs.hdr->fs_size != size)
	{
		Rfs.hdr->fs_size = ALIGN_UP(size, PAGE_SIZE);
	}

	// Get RFS script commands
	Rfs.cmds = (cmd_t*)((uint32_t)Rfs.hdr + Rfs.hdr->script_off);

	// Get RAM information
	Rfs.ram = (ram_t*)((uint32_t)Rfs.hdr + Rfs.hdr->ram_off);

	// Get Interrupt information
	Rfs.intr = (intr_t*)((uint32_t)Rfs.hdr + Rfs.hdr->irq_off);

	// Get supported Devices
	Rfs.devices = (device_t*)((uint32_t)Rfs.hdr + Rfs.hdr->devices_off);

	// Get Raw File System owned Files
	Rfs.files = (file_t*)((uint32_t)Rfs.hdr + Rfs.hdr->files_off);

	// Get Strings Table
	Rfs.strings = (char*)((uint32_t)Rfs.hdr + Rfs.hdr->names_off);

	// Set Raw File System as unlocked to enable access
	Rfs.locked = FALSE;

	return E_OK;
}

int32_t RfsGetRamInfo(paddr_t *addr, size_t *size)
{
	*addr = (paddr_t)Rfs.ram->addr;
	*size = Rfs.ram->size;

	return E_OK;
}

int32_t RfsGetInterruptInfo(uint32_t *priv, uint32_t *shared)
{
	*priv = Rfs.intr->priv;
	*shared = Rfs.intr->shared;

	return E_OK;
}

int32_t RfsRunStartupScript()
{
	int32_t i = 0;
	for (; i < Rfs.hdr->script_cmds; i++)
	{
		file_t *file = RfsGetFile(Rfs.cmds[i].file_off);

		if(Rfs.cmds[i].type == EXEC_TYPE && file->type == EXEC_TYPE)
		{
			spawn_attr_t attr = {Rfs.cmds[i].prio, Rfs.cmds[i].priv, FALSE, TRUE};

			ProcSpawn
			(
					(void *)((uint32_t)Rfs.hdr + file->data_off),
					RfsGetString(Rfs.cmds[i].cmd_off),
					&attr,
					0,
					NULL
			);
		}
	}

	return E_OK;
}

int32_t RfsRegisterDevices()
{
	device_t *device = Rfs.devices;
	int32_t i = 0;
	for (; i < Rfs.hdr->devices_count; i++)
	{
		DeviceRegister((paddr_t)device[i].addr, device[i].size, RfsGetString(device[i].name_off));
	}

	return E_OK;
}

char* RfsGetVersion()
{
	return RfsGetString(Rfs.hdr->version);
}

char* RfsGetArch()
{
	return RfsGetString(Rfs.hdr->arch);
}

char* RfsGetMach()
{
	return RfsGetString(Rfs.hdr->machine);
}

int32_t RfsGet(vaddr_t *addr, size_t *size)
{
	// Check if it's locked! We can only get the RFS one time
	if(Rfs.locked == TRUE)
	{
		return E_ERROR;
	}

	*addr = (vaddr_t)(Rfs.hdr);
	*size = Rfs.hdr->fs_size;

	Rfs.locked = TRUE;

	return E_OK;
}

