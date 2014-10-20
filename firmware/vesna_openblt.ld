MEMORY
{
	rom (rx) : ORIGIN = 0x08006000, LENGTH = 512K - 0x6000
	ram (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

SECTIONS
{
	. = ORIGIN(rom);

	.text : {
		*(.vectors)	/* Vector table */
		_openblt_crc = .;
		LONG(0)
		*(.text*)	/* Program code */
		. = ALIGN(4);
		*(.rodata*)	/* Read-only data */
		. = ALIGN(4);
		_etext = .;
	} >rom
}

INCLUDE libopencm3_stm32f1.ld
