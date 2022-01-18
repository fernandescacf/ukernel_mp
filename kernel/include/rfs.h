/**
 * @file        rfs.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        28 May, 2020
 * @brief       Raw File System Definition Header File
*/

#ifndef _RFS_H_
#define _RFS_H_


/* Includes ----------------------------------------------- */
#include <types.h>



/* Exported types ----------------------------------------- */



/* Exported constants ------------------------------------- */



/* Exported macros ---------------------------------------- */



/* Private functions -------------------------------------- */



/* Exported functions ------------------------------------- */

int32_t RfsInit(vaddr_t rfs, size_t size);

int32_t RfsGetRamInfo(paddr_t *addr, size_t *size);

int32_t RfsGetInterruptInfo(uint32_t *priv, uint32_t *shared);

int32_t RfsRunStartupScript();

int32_t RfsRegisterDevices();

char* RfsGetVersion();

char* RfsGetArch();

char* RfsGetMach();

int32_t RfsGet(vaddr_t *addr, size_t *size);

#endif /* _RFS_H_ */
