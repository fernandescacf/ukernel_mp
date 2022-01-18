/**
 * @file        board.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        17 June, 2020
 * @brief       Board Specific Functionalities Interface header file
*/

#ifndef _BOARD_H_
#define _BOARD_H_

#ifdef __cplusplus
    extern "C" {
#endif


/* Includes ----------------------------------------------- */
#include <types.h>


/* Exported types ----------------------------------------- */


/* Exported constants ------------------------------------- */



/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */

int32_t BoardEarlyInit();

int32_t BoardInterruptCtrlInit(vaddr_t isr_sp);

int32_t BoardSystemTimerInit();

int32_t BoardTestInterrupts();

uint32_t BoardGetCpus();

int32_t BoardSecCpuInit();

#ifdef __cplusplus
    }
#endif

#endif
