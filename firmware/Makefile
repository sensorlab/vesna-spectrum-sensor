BINARY = spectrum-sensor

VERSION		= $(shell git describe --always)

PREFIX		?= arm-none-eabi
CC		= $(PREFIX)-gcc
LD		= $(PREFIX)-gcc
OBJCOPY		= $(PREFIX)-objcopy
CFLAGS		+= -Os -g -Wall -Wextra -I$(TOOLCHAIN_DIR)/include -Iplatform -Idevices -I.\
		   -fno-common -mcpu=cortex-m3 -mthumb -msoft-float -MD -DSTM32F1
LDSCRIPT	?= vesna.ld
LDFLAGS		+= -Wl,--start-group -lc -lm -lgcc -lnosys -Wl,--end-group \
		   -L$(TOOLCHAIN_DIR)/lib -L$(TOOLCHAIN_DIR)/lib/stm32/f1 \
		   -T$(LDSCRIPT) -nostartfiles -Wl,--gc-sections \
		   -mthumb -march=armv7 -mfix-cortex-m3-ldrd -msoft-float
OBJS		+= average.o main.o device.o buffer.o task.o calibration.o platform/timer.o platform/rtc.o platform/rcc.o
LIBS		+= -lopencm3_stm32f1

OPENOCD		?= openocd
OPENOCD_PARAMS  ?= -f interface/olimex-arm-usb-ocd.cfg -f target/stm32f1x.cfg

include Makefile.conf

all: $(BINARY).elf

%.bin: %.elf
	$(OBJCOPY) -Obinary $(*).elf $(*).bin

%.elf: $(OBJS) $(LDSCRIPT)
	$(LD) -o $(*).elf $(OBJS) $(LIBS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o *.d *.elf *.bin
	rm -f platform/*.o platform/*.d
	rm -f devices/*.o devices/*.d
	$(MAKE) -C tests clean

%.u: %.elf
	$(OPENOCD) $(OPENOCD_PARAMS) -c "\
		reset_config trst_and_srst; \
		init; \
		reset halt; \
		poll; \
		flash write_image erase $< 0 elf; \
		reset run; \
		shutdown \
	"

version.h: force
	echo "#define VERSION \"$(VERSION)\"" > version.h

force:

test:
	$(MAKE) -C tests test

.PHONY: clean force test

-include $(OBJS:.o=.d)
