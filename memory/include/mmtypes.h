/**
 * @file        mmtypes.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        03 January, 2020
 * @brief       Memory Types Definition Header File
*/

#ifndef _MMTYPES_H_
#define _MMTYPES_H_

#ifdef __cplusplus
    extern "C" {
#endif


/* Includes ----------------------------------------------- */
#include <types.h>
#include <glist.h>


/* Exported types ----------------------------------------- */

typedef struct Pmm
{
	ptr_t		addr;			// base address
	uint32_t	size;			// size of the memory page
	struct Pmm  *next,*prev;	// iteration pointers
}pmm_t;

typedef struct
{
	paddr_t		addr;
	size_t		size;
	glistNode_t	node;
}mmobj_t;

/* Exported constants ------------------------------------- */

#define UNMAPPED		((ptr_t)(-1))

/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */


#ifdef __cplusplus
    }
#endif

#endif /* _MMTYPES_H_ */
