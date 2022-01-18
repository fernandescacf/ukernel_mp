/* NUMBER OF CPUS USED */
N_CPUS = 1;

/* STACKS SIZES */
IRQ_STACK_SIZE = 4k * N_CPUS;
KERNEL_STACK_SIZE = 4k * N_CPUS;
SCHEDULER_STACK_SIZE = 1k * N_CPUS;
CRITICAL_STACK_SIZE = 1k * N_CPUS;
IDLE_STACK_SIZE = (16 * 4) * N_CPUS;

/* ENTRY POINT */
ENTRY(_start)


MEMORY
{
	/* RAM */
	DDR (rwx) : ORIGIN = 0x80004000, LENGTH = 0x20000000
}

/* SEGMENTS */
PHDRS
{
	text PT_LOAD;
  	data PT_LOAD;
  	stack PT_NULL;
  	rfs PT_LOAD;
}

/* SECTIONS */
SECTIONS
{
	/* Kernel Virtual Space Base Address */
	KernelVirtualBase = 0x80000000;

	/* Start up code and text */
	.text : ALIGN(4096) {
		__Kimage_start = .;
		_text_start = .;
			arch.o (.text);
			*(.text);
			*(.text.startup);
		_text_end = .;
	} > DDR : text
	
	/* Rodara section: read only data*/
	.rodata : ALIGN(4) {
		_rodata_start = .;
			*(.rodata)
			*(.rodata.*)
		_rodata_end = .;
	} > DDR : text
	
	/* Data section: read write only data */
	.data : ALIGN(4096) {
		_data_start = .;
			*(.data)
			*(.data.*)
		_data_end = .;
	} > DDR : data
	
	/* Uninitialized data section */
	.bss : ALIGN(4) {
		_bss_start = .;
			*(.bss)
			*(.bss.*)
			*(COMMON)
		_bss_end = .;
	} > DDR : data
	
	/* OS Stacks section */
	.stacks : ALIGN(8) {
		/* Main kernel stack */
		__kernel_stack_base = .;
		. += KERNEL_STACK_SIZE;
		. = ALIGN (8);
		__kernel_stack = .;
		
		/* Idle stack */
		__idle_stack_base = .;
		. += IDLE_STACK_SIZE;
		. = ALIGN (8);
		__idle_stack = .;
	} > DDR : stack
	
	__Kimage_end = ALIGN(4096);
/*	
	rfs_section : ALIGN(4096){
		rfs_address = .;
		*(rfs_section);
	} > DDR : rfs
*/
}
