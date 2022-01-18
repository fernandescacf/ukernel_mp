/**
 * @file        misc.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        18 March, 2018
 * @brief       Misc Definition Header File
*/

#ifndef _MISC_H_
#define _MISC_H_

#ifdef __cplusplus
    extern "C" {
#endif


/* Includes ----------------------------------------------- */
#include <types.h>


/* Exported types ----------------------------------------- */


/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */

#define offsetof(TYPE, MEMBER) ((uint32_t) &((TYPE *)0)->MEMBER)

#define ROUND_UP(m,a)			(((m) + ((a) - 1)) & (~((a) - 1)))
#define ROUND_DOWN(m,a)			((m) & (~((a) - 1)))

#define ALIGN_DOWN(m,a)	((m) & (~(a - 1)))
#define ALIGN_UP(m,a)	(((m) + (a - 1)) & (~(a - 1)))

#define set_wbit(addr, v)	(*((volatile ulong_t *)(addr)) |= (ulong_t)(v))
#define readl(addr)			(*((volatile ulong_t *)(addr)))
#define writel(v, addr)		(*((volatile ulong_t *)(addr)) = (ulong_t)(v))


/* Exported functions ------------------------------------- */

static inline void clrsetbits(volatile void  *mem, uint32_t clr, uint32_t set)
{
	uint32_t __val = readl(mem);
	__val &= ~clr;
	__val |= set;
	writel(__val, mem);
}

static inline void clrbits(volatile void *mem, uint32_t clr)
{
	uint32_t __val = readl(mem);
	__val &= ~clr;
	writel(__val, mem);
}

static inline void setbits(volatile void *mem, uint32_t set)
{
	uint32_t __val = readl(mem);
	__val |= set;
	writel(__val, mem);
}

#ifdef __cplusplus
    }
#endif

#endif /* _MISC_H_ */
