/**
 * @file        asm.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        December 21, 2019
 * @brief       Processor ASM instructions Definition Header File
*/

#ifndef _ASM_H_
#define _ASM_H_

#ifdef __cplusplus
    extern "C" {
#endif


/* Includes ----------------------------------------------- */


/* Exported types ----------------------------------------- */


/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */

#define dsb()				asm volatile("dsb")
#define dmb()				asm volatile("dmb")
#define isb()				asm volatile("isb")

#define cpsie()				asm volatile ("cpsie i")
#define cpsid()				asm volatile ("cpsid i")

/* Exported functions ------------------------------------- */

#ifdef __cplusplus
    }
#endif

#endif /* _ASM_H_ */
