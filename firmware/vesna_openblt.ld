MEMORY
{
	rom (rx) : ORIGIN = 0x08006000, LENGTH = 512K - 0x6000
	ram (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

INCLUDE libopencm3_stm32f1.ld
