/**
 * @file        elf.h
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        30 January, 2020
 * @brief       Elf Definition Header File
*/

#ifndef _ELF_H_
#define _ELF_H_


/* Includes ----------------------------------------------- */
#include <types.h>


/* Exported constants ------------------------------------- */

// Sections Permissions
enum
{
	PF_X = 0x00000001,
	PF_W = 0x00000002,
	PF_R = 0x00000004
};


// segment types
enum
{
	PT_NULL = 0,				// Program header table entry unused
	PT_LOAD,					// Loadable program segment
	PT_DYNAMIC,					// Dynamic linking information
	PT_INTERP,					// Program interpreter
	PT_NOTE,					// Auxiliary information
	PT_SHLIB,					// Reserved, unspecified semantics
	PT_PHDR,					// Entry for header table itself
	PT_NUM,						// Number of p types
};


/* Exported types ----------------------------------------- */

typedef struct
{
    void		*elf_hdr;       // elf header
    void		*sct_hdr;       // first sections header
    void		*seg_hdr;		// first segment header
    uint8_t		*shstrtab;      // sections name table
    uint32_t	sct_count;      // number of sections
    uint32_t	seg_count;		// number of segments
}elf_t;

typedef struct
{
    int8_t		*name;			// name
    uint32_t	type;			// type
    uint32_t	flags;			// flags
    vaddr_t		addr;			// address
    size_t		size;			// size
    int8_t		*data;			// data
}section_t;

typedef struct
{
    uint32_t	type;			// type
    uint32_t	flags;			// flags
    uint32_t	entry;			// entry number in the segments array
    vaddr_t		addr;			// address
    size_t		size_file;		// file memory size
    size_t		size_mem;		// total memory size
    int8_t		*data;			// data
}segment_t;


/* Exported macros ---------------------------------------- */



/* Exported functions ------------------------------------- */

/*
 * @brief   Parses the raw elf file to a know generic format
 *
 * @param   raw - raw pointer to the beginning of the elf file
 * 			elf	- parsed elf file
 *
 * @retval  Success
 */
int32_t ElfParse(void *raw, elf_t *elf);

/*
 * @brief   Routine to iterate through all segments (there should only be two)
 *
 * @param   elf 	- elf structure
 * 			segment	- current segment data (it will be overwrite with the data from the next segment)
 *
 * @retval  Return Success or -1 if it couldn't get the next segment
 */
int32_t ElfGetNextSegment(elf_t *elf, segment_t *segment);

/*
 * @brief   Find and get the section data with the specified name
 *
 * @param   elf 	- elf structure
 * 			name	- section name
 * 			section	- structure that will contain the section data
 *
 * @retval  Return Success
 */
int32_t ElfFindSection(elf_t *elf, const char *name, section_t *section);

/*
 * @brief   Find and get the symbol value
 *
 * @param   elf 	- elf structure
 * 			value	- where the symbol value will be returned
 * 			name	- symbol name
 *
 * @retval  Return Success
 */
int32_t ElfGetSymbolValue(elf_t *elf, void *value, const char *name);


#endif /* _ELF_H_ */
