/**
 * @file        vstack.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        06 January, 2020
 * @brief       Virtual Stack Mapper Implementation
*/

/* Includes ----------------------------------------------- */
#include <vstack.h>


/* Private types ------------------------------------------ */



/* Private constants -------------------------------------- */



/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */



/* Private functions -------------------------------------- */

/**
 * sManagerCreate Implementation (See header file for description)
*/
sManager_t *sManagerCreate(pgt_t pgt, vaddr_t vBase, vaddr_t vTop)
{
	return (sManager_t *)vManagerCreate(pgt, vBase, vTop, VMANAGER_STACK);
}

/**
 * sManagerInitialize Implementation (See header file for description)
*/
int32_t sManagerInitialize(sManager_t *sm, pgt_t pgt, vaddr_t vBase, vaddr_t vTop)
{
	return vManagerInitialize((vManager_t *)sm, pgt, vBase, vTop, VMANAGER_STACK);
}

/**
 * sManagerDestroy Implementation (See header file for description)
*/
int32_t sManagerDestroy(sManager_t *sm)
{
	return vManagerDestroy((vManager_t *)sm);
}

/**
 * sManagerSetUnmapHandler Implementation (See header file for description)
*/
int32_t sManagerSetUnmapHandler(sManager_t *sm, void *arg, int32_t (*unmapHandler)(void*, pmm_t*))
{
	return vManagerSetUnmapHandler((vManager_t *)sm, arg, unmapHandler);
}

/**
 * vStackGet Implementation (See header file for description)
*/
vStack_t *vStackGet(sManager_t *sm, size_t size)
{
	return (vStack_t *)vSpaceReserve((vManager_t *)sm, size);
}

/**
 * vStackFree Implementation (See header file for description)
*/
int32_t vStackFree(vStack_t *vstack)
{
	return vSpaceRelease((vSpace_t *)vstack);
}

/**
 * vStackMap Implementation (See header file for description)
*/
vaddr_t vStackMap(vStack_t *vstack, paddr_t pAddr, size_t size, ulong_t mapType)
{
	return vSpaceMapSection((vSpace_t *)vstack, pAddr, size, mapType, NULL);
}

/**
 * vStackUnmap Implementation (See header file for description)
*/
vaddr_t vStackUnmap(vStack_t *vstack, size_t size)
{
	return vSpaceUnmapSection((vSpace_t *)vstack, size);
}
