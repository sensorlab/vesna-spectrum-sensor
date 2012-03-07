BINARY = spectrum-sense

PREFIX		?= arm-none-eabi
CC		= $(PREFIX)-gcc
LD		= $(PREFIX)-gcc
OBJCOPY		= $(PREFIX)-objcopy
CFLAGS		+= -Os -g -Wall -Wextra -I$(TOOLCHAIN_DIR)/include \
		   -fno-common -mcpu=cortex-m3 -mthumb -msoft-float -MD -DSTM32F1
LDSCRIPT	?= vesna.ld
LDFLAGS		+= -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group \
		   -L$(TOOLCHAIN_DIR)/lib -L$(TOOLCHAIN_DIR)/lib/stm32/f1 \
		   -T$(LDSCRIPT) -nostartfiles -Wl,--gc-sections \
		   -mthumb -march=armv7 -mfix-cortex-m3-ldrd -msoft-float
OBJS		+= main.o spectrum.o
LIBS		+= -lopencm3_stm32f1

OPENOCD		?= openocd
OPENOCD_PARAMS  ?= -f interface/olimex-arm-usb-ocd.cfg -f target/stm32f1x.cfg

ifeq ($(MODEL),sne-crew)
	LIBS += -ltda18219
	OBJS += dev-tda18219.o
	CFLAGS += -DMODEL_TDA18219
else
	OBJS += dev-null.o
	CFLAGS += -DMODEL_NULL
endif

all: $(BINARY).bin

%.bin: %.elf
	$(OBJCOPY) -Obinary $(*).elf $(*).bin

%.elf: $(OBJS) $(LDSCRIPT)
	$(LD) -o $(*).elf $(OBJS) $(LIBS) $(LDFLAGS)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -DMODEL=$(MODEL) -o $@ -c $<

clean:
	rm -f *.o
	rm -f *.d
	rm -f *.elf
	rm -f *.bin

%.u: %.bin
	$(OPENOCD) $(OPENOCD_PARAMS) -c "\
		init; \
		reset halt; \
		poll; \
		stm32f1x mass_erase 0; \
		flash write_bank 0 $< 0; \
		reset run; \
		shutdown \
	"

.PHONY: clean

-include $(OBJS:.o=.d)
