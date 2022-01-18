/**
 * @file        interrupt.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        22 June, 2020
 * @brief		Board Specific Interrupts Handler Interface header file
*/

#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#ifdef __cplusplus
    extern "C" {
#endif


/* Includes ----------------------------------------------- */
#include <types.h>


/* Exported constants ------------------------------------- */


/* Exported types ----------------------------------------- */


/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */


void InterruptEnable(uint32_t irq);

void InterruptDisable(uint32_t irq);

void InterruptSetTarget(uint32_t irq, uint32_t target, uint32_t set);

int32_t InterruptSetPriority(uint32_t irq, uint32_t priority);

void InterruptGenerate(uint32_t irq, uint32_t cpu);

void InterruptGenerateSelf(uint32_t irq);

void InterruptDecode(uint32_t irqData, uint32_t *irq, uint32_t *source);

void InterruptUnpend(uint32_t irq);

void InterruptAcknowledge(uint32_t irq);

void InterruptEnd(uint32_t irq);

void InterruptDispache(uint32_t irq, uint32_t source, void* (*handler)(void*, uint32_t), void *arg);


#ifdef __cplusplus
    }
#endif

#endif /* _INTERRUPT_H_ */
