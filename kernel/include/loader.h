/**
 * @file        loader.h
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        28 January, 2020
 * @brief       Loader Definition Header File
*/

#ifndef _LOADER_H_
#define _LOADER_H_


/* Includes ----------------------------------------------- */
#include <types.h>
#include <mmtypes.h>
#include <glist.h>



/* Exported types ----------------------------------------- */

typedef struct
{
	vaddr_t		(*entry)(void*);
	vaddr_t 	(*exit)(void*);

	vaddr_t		baseAddr;
	vaddr_t		topAddr;

	struct
	{
		vaddr_t		addr;
		size_t		size;
		glist_t 	memory;
	}text;

	struct
	{
		vaddr_t		addr;
		size_t		size;
	}data;

	struct
	{
		vaddr_t		addr;
		size_t		size;
	}bss;

}loadInf_t;

typedef struct
{
	loadInf_t *load;

	struct
	{
		glist_t 	dataMemory;
	}private;

}exec_t;

/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */

/*
 * @brief   Initialize Loader
 *
 * @param   None
 *
 * @retval  Return Success
 */
int32_t LoaderInitialize(void);

/*
 * @brief   Load an elf file into memory
 *
 * @param   raw -	Raw pointer to the location of the elf file in RAM
 * 			cmd -	Execution command use to run the elf file
 * 			exec -	Exec structure to be filed with the load info
 *
 * @retval  Return Success
 */
int32_t LoaderLoadElf(void *raw, char *cmd, exec_t *exec);

/*
 * @brief   Unload an elf file. Only if there are no more references to it, it
 * 			will be unloaded
 *
 * @param   exec -	Exec structure to referent to the elf
 *
 * @retval  Return Success
 */
int32_t LoaderUnloadElf(exec_t *exec);


#endif /* _LOADER_H_ */
