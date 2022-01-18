#include <cache.h>
#include <asm.h>

/* CCSIDR */
#define CCSIDR_LINE_SIZE_OFFSET		0
#define CCSIDR_LINE_SIZE_MASK		0x7

#define CONFIG_SYS_CACHELINE_SIZE 64

static uint32_t get_ccsidr()
{
	uint32_t ccsidr;
	/* Read current CP15 Cache Size ID Register */
	asm volatile ("mrc p15, 1, %0, c0, c0, 0" : "=r" (ccsidr));
	return ccsidr;
}

void FlushDcacheRange(ptr_t addr, size_t size)
{
	uint32_t ccsidr = get_ccsidr();
	uint32_t line_len = ((ccsidr & CCSIDR_LINE_SIZE_MASK) >> CCSIDR_LINE_SIZE_OFFSET) + 2;
	/* Converting from words to bytes */
	line_len += 2;
	/* converting from log2(linelen) to linelen */
	line_len = 1 << line_len;

	// Align start to cache line boundary
	uint32_t mva = ((uint32_t)addr & ~(line_len - 1));
	uint32_t stop = (uint32_t)addr + size;

	for (; mva < stop; mva += line_len)
	{
		/* DCCIMVAC - Clean & Invalidate data cache by MVA to PoC */
		asm volatile ("mcr p15, 0, %0, c7, c14, 1" : : "r" (mva));
	}

	dsb();
	isb();
}
