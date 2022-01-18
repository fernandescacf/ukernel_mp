/**
 * @file        armv7.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        16 August, 2018
 * @brief       Arm v7 Definition Header File
*/

#ifndef _ARMV7_H_
#define _ARMV7_H_

#define SCTLR_M			(1<<0) 					// SCTLR.M bit (MMU)
#define SCTLR_A			(1<<1) 					// SCTLR.A bit (Strict aligment checking)
#define SCTLR_D			(1<<2) 					// SCTLR.D bit (Data cache)
#define SCTLR_W			(1<<3) 					// SCTLR.W bit (Write buffer)
#define SCTLR_Z			(1<<11) 				// SCTLR.Z bit (Branch prediction)
#define SCTLR_I			(1<<12) 				// SCTLR.I bit (Instruction cache)
#define SCTLR_V			(1<<13) 				// SCTLR.V bit (Low exception vectors)
#define SCTLR_U			(1<<22) 				// SCTLR.U bit (Unaligned data access)
#define SCTLR_XP		(1<<23) 				// SCTLR.XP bit (Extended page tables)


#define USR_MODE		0x10
#define FIQ_MODE		0x11
#define IRQ_MODE 		0x12
#define SVC_MODE 		0x13
#define ABT_MODE		0x17
#define UNDEF_MODE 		0x1B
#define SYS_MODE 		0x1F

#define ABT_BIT			(1<<8)
#define IRQ_BIT			(1<<7)
#define FIQ_BIT			(1<<6)


#endif /* _ARMV7_H_ */
