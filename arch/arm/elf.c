/**
 * @file        elf.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        30 January, 2020
 * @brief       Elf ARM specific implementation
*/

/* Includes ----------------------------------------------- */
#include <elf.h>
#include <string.h>


/* Private constants -------------------------------------- */
#define EI_NIDENT	16

#define ELFMAG0		0x7F	// e_ident[EI_MAG0]
#define ELFMAG1		'E'		// e_ident[EI_MAG1]
#define ELFMAG2		'L'		// e_ident[EI_MAG2]
#define ELFMAG3		'F'		// e_ident[EI_MAG3]
#define ELFCLASS32	(1)     // 32-bit Architecture
#define ELFDATA2LSB	(1)     // Little Endian
#define EV_CURRENT	(1)     // ELF Current Version
#define ET_EXEC		(2)     // Executable file
#define EM_ARM		(40)    // ARM machine

enum Elf_Ident
{
	EI_MAG0         = 0,	// 0x7F
	EI_MAG1         = 1,	// 'E'
	EI_MAG2         = 2,	// 'L'
	EI_MAG3         = 3,	// 'F'
	EI_CLASS        = 4,	// Architecture (32/64)
	EI_DATA         = 5,	// Byte Order
	EI_VERSION      = 6,	// ELF Version
	EI_OSABI        = 7,	// OS Specific
	EI_ABIVERSION	= 8,	// OS Specific
	EI_PAD          = 9 	// Padding
};

enum ShT_Types
{
	SHT_NULL		= 0,	// Null section
	SHT_PROGBITS	= 1,	// Program information
	SHT_SYMTAB		= 2,	// Symbol table
	SHT_STRTAB		= 3,	// String table
	SHT_RELA		= 4,	// Relocation (w/ addend)
	SHT_NOBITS		= 8,	// Not present in file
	SHT_REL			= 9		// Relocation (no addend)
};

enum ShT_Attributes
{
	SHF_WRITE	= 0x01,		// Writable section
	SHF_ALLOC	= 0x02,		// Exists in memory
	SHF_EXECINSTR = 0x04
};

/* Private types ------------------------------------------ */
typedef unsigned int	Elf32_Addr;
typedef unsigned short	Elf32_Half;
typedef unsigned int	Elf32_Off;
typedef signed int		Elf32_Sword;
typedef unsigned int	Elf32_Word;

typedef struct
{
	unsigned char e_ident[EI_NIDENT];  // elf identification
	Elf32_Half e_type;                 // object file type
	Elf32_Half e_machine;              // target architecture
	Elf32_Word e_version;              // object file version
	Elf32_Addr e_entry;                // code entry address
	Elf32_Off e_phoff;                 // program header table's file offset in bytes
	Elf32_Off e_shoff;                 // section header table's file offset in bytes
	Elf32_Word e_flags;                // processor-specific flags
	Elf32_Half e_ehsize;               // ELF header's size in bytes
	Elf32_Half e_phentsize;            // size in bytes of one entry in the file's program header
	Elf32_Half e_phnum;                // number of entries in the program header table (program header table's size = e_phentsize * e_phnum)
	Elf32_Half e_shentsize;            // section header's size in bytes
	Elf32_Half e_shnum;                // number of entries in the section header table (section header table's size = e_shentsize * e_shnum)
	Elf32_Half e_shstrndx;             // section header table index of the entry associated with the section name string table
} Elf32_Ehdr;

typedef struct
{
	Elf32_Word		p_type;			/* entry type */
	Elf32_Off		p_offset;		/* offset */
	Elf32_Addr		p_vaddr;		/* virtual address */
	Elf32_Addr		p_paddr;		/* physical address */
	Elf32_Word		p_filesz;		/* file size */
	Elf32_Word		p_memsz;		/* memory size */
	Elf32_Word		p_flags;		/* flags */
	Elf32_Word		p_align;		/* memory & file alignment */
} Elf32_Phdr;

typedef struct
{
	Elf32_Word sh_name;
	Elf32_Word sh_type;
	Elf32_Word sh_flags;
	Elf32_Addr sh_addr;
	Elf32_Off  sh_offset;
	Elf32_Word sh_size;
	Elf32_Word sh_link;
	Elf32_Word sh_info;
	Elf32_Word sh_addralign;
	Elf32_Word sh_entsize;
} Elf32_Shdr;

typedef struct
{
	Elf32_Word		st_name;
	Elf32_Addr		st_value;
	Elf32_Word		st_size;
	unsigned char	st_info;
	unsigned char	st_other;
	Elf32_Half		st_shndx;
}Elf32_Sym;

/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */

/*
 * @brief   Checks if the specified elf file is valid
 *
 * @param   hdr - elf header
 *
 * @retval  Success
 */
static int32_t ElfCheck(Elf32_Ehdr *hdr);

/*
 * @brief   Checks if elf file format is supported
 *
 * @param   hdr - elf header
 *
 * @retval  Success
 */
static uint32_t ElfSupported(Elf32_Ehdr *hdr);

/*
 * @brief   Parse the elf header to the know generic structure
 *
 * @param   hdr - elf header
 * 			elf - parsed structure
 *
 * @retval  None
 */
static void ElfHeaderParse(Elf32_Ehdr *hdr, elf_t *elf);

/*
 * @brief   Converts the section header in a section_t structure
 * @param   section - pointer to the section structure
 * 			elf		- pointer to the elf structure
 * 			sct		- section number
 * @retval  No return
 */
static void SectionParse(section_t *section, elf_t *elf, uint32_t sct);

/*
 * @brief   Converts the segment header in a segment_t structure
 * @param   segment - pointer to the section structure
 * 			elf		- pointer to the elf structure
 * 			seg		- segment number
 * @retval  No return
 */
static void SegmentParse(segment_t *segment, elf_t *elf, uint32_t seg);

/*
 * @brief   Converts elf section to specific elf section type
 *
 * @param   elf		- pointer to the elf structure
 * 			sct		- section number
 *
 * @retval  Elf32_Shdr pointer to the section
 */
inline static Elf32_Shdr *GetSection(elf_t *elf, uint32_t sct)
{
	return &((Elf32_Shdr*)(elf->sct_hdr))[sct];
}

/*
 * @brief   Converts elf segment to specific elf segment type
 *
 * @param   elf		- pointer to the elf structure
 * 			seg		- section number
 *
 * @retval  Elf32_Phdr pointer to the segment
 */
inline static Elf32_Phdr *GetSegment(elf_t *elf, uint32_t seg)
{
	return &((Elf32_Phdr*)(elf->seg_hdr))[seg];
}

/* Private functions -------------------------------------- */

/**
 * ElfCheck Implementation (See Private function prototypes section)
*/
int32_t ElfCheck(Elf32_Ehdr *hdr)
{
    if(hdr->e_ident[EI_MAG0] != ELFMAG0) return E_INVAL;
    if(hdr->e_ident[EI_MAG1] != ELFMAG1) return E_INVAL;
    if(hdr->e_ident[EI_MAG2] != ELFMAG2) return E_INVAL;
    if(hdr->e_ident[EI_MAG3] != ELFMAG3) return E_INVAL;
    return E_OK;
}

/**
 * ElfSupported Implementation (See Private function prototypes section)
*/
uint32_t ElfSupported(Elf32_Ehdr *hdr)
{
    if(ElfCheck(hdr)) return E_INVAL;
    if(hdr->e_ident[EI_CLASS] != ELFCLASS32) return E_INVAL;
    if(hdr->e_ident[EI_DATA] != ELFDATA2LSB) return E_INVAL;
    if(hdr->e_machine != EM_ARM) return E_INVAL;
    if(hdr->e_ident[EI_VERSION] != EV_CURRENT) return E_INVAL;
    if(hdr->e_type != ET_EXEC) return E_INVAL;
    return E_OK;
}

/**
 * ElfHeaderParse Implementation (See Private function prototypes section)
*/
void ElfHeaderParse(Elf32_Ehdr *hdr, elf_t *elf)
{
	elf->elf_hdr = hdr;
	elf->sct_count = hdr->e_shnum;
	elf->sct_hdr = (Elf32_Shdr*)((uint32_t)hdr + hdr->e_shoff);
	elf->shstrtab = (uint8_t*)((uint32_t)hdr + GetSection(elf, hdr->e_shstrndx)->sh_offset);
	elf->seg_hdr = (Elf32_Phdr*)((uint32_t)hdr + hdr->e_phoff);
	elf->seg_count = hdr->e_phnum;
}

/**
 * SectionParse Implementation (See Private function prototypes section)
*/
void SectionParse(section_t *section, elf_t *elf, uint32_t sct)
{
	Elf32_Shdr *shdr = GetSection(elf, sct);
	section->name = (int8_t*)&elf->shstrtab[shdr->sh_name];
	section->type = shdr->sh_type;
	section->flags = shdr->sh_flags;
	section->size = shdr->sh_size;
	section->addr = (vaddr_t)shdr->sh_addr;
	section->data = (int8_t*)((uint32_t)(elf->elf_hdr) + shdr->sh_offset);
}

/**
 * SegmentParse Implementation (See Private function prototypes section)
*/
void SegmentParse(segment_t *segment, elf_t *elf, uint32_t seg)
{
	Elf32_Phdr *phdr = GetSegment(elf, seg);
	segment->addr = (vaddr_t)phdr->p_vaddr;
	segment->flags = phdr->p_flags;
	segment->type = phdr->p_type;
	segment->size_file = phdr->p_filesz;
	segment->size_mem = phdr->p_memsz;
	segment->data = (int8_t*)((uint32_t)(elf->elf_hdr) + phdr->p_offset);
}

/**
 * ElfParse Implementation (See header file for description)
*/
int32_t ElfParse(void *raw, elf_t *elf)
{
	if(!raw || !elf)
	{
		return E_INVAL;
	}

	Elf32_Ehdr *hdr = (Elf32_Ehdr*)raw;

	if(ElfSupported(hdr))
	{
		memset(elf, 0x0, sizeof(elf_t));
		return E_INVAL;
	}

	ElfHeaderParse(hdr, elf);

	return E_OK;
}

/**
 * ElfGetNextSegment Implementation (See header file for description)
*/
int32_t ElfGetNextSegment(elf_t *elf, segment_t *segment)
{
	if(!segment)
	{
		return E_INVAL;
	}

	if(segment->entry < elf->seg_count)
	{
		uint32_t seg = ((segment->type == 0) ? (0) : (segment->entry + 1));
		SegmentParse(segment, elf, seg);
		return E_OK;
	}
	else
	{
		memset(segment, 0x0, sizeof(segment_t));
		return E_NO_RES;
	}
}

/**
 * ElfFindSection Implementation (See header file for description)
*/
int32_t ElfFindSection(elf_t *elf, const char *name, section_t *section)
{
	if(!name || !section)
	{
		return E_INVAL;
	}

	uint32_t count = 0;
	while(count < elf->sct_count && strcmp(&elf->shstrtab[GetSection(elf, count)->sh_name], name))
	{
		count++;
	}

	if(count == elf->sct_count)
	{
		memset(section, 0x0, sizeof(section_t));
		return E_SRCH;
	}
	else
	{
		SectionParse(section, elf, count);
		return E_OK;
	}
}

/**
 * ElfGetSymbolValue Implementation (See header file for description)
*/
int32_t ElfGetSymbolValue(elf_t *elf, void *value, const char *name)
{
	section_t symtab;
	section_t strtab;
	Elf32_Sym *symbol;

	if(ElfFindSection(elf, ".symtab", &symtab) != E_OK)
	{
		return E_ERROR;
	}
	if(ElfFindSection(elf, ".strtab", &strtab) != E_OK)
	{
		return E_ERROR;
	}

	symbol = (Elf32_Sym*)symtab.data;
	while(strcmp(&strtab.data[symbol->st_name],name))
	{
		symbol = (Elf32_Sym*)((uint32_t)symbol + sizeof(Elf32_Sym));
	}
	*((Elf32_Addr *)value) = symbol->st_value;

	// TODO: ELF Get symbol value: missing case if symbol is not found

	return E_OK;;
}
