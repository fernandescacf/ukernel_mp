/**
 * @file        cache.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        December 20, 2019
 * @brief       Cache BSP Definition Header File
*/

#ifndef _CACHE_H
#define _CACHE_H


/* Includes ----------------------------------------------- */
#include <types.h>


/* Exported constants ------------------------------------- */



/* Exported types ----------------------------------------- */



/* Exported macros ---------------------------------------- */



/* Exported functions ------------------------------------- */

/*
 * @brief   Invalidate all instruction caches to PoU and flushes branch target cache
 * @param   None
 * @retval  No Return
 */
void InvalidateIcache(void);

/*
 * @brief   Invalidate Data Cache
 * @param   None
 * @retval  No Return
 */
void InvalidateDcache(void);

/*
 * @brief   Flushes Data Cache
 * @param   None
 * @retval  No Return
 */
void FlushDcache(void);

void FlushDcacheRange(ptr_t addr, size_t size);

/*
 * @brief   Invalidate Data and Instruction Caches
 * @param   None
 * @retval  No Return
 */
void InvalidateCaches(void);

/*
 * @brief   Flushes Data and Instruction Caches
 * @param   None
 * @retval  No Return
 */
void FlushCaches(void);

#endif /* _CACHE_H */
