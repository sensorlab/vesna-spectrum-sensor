BINARY = spectrum-sensor

VERSION		= $(shell git describe --always)

PREFIX		?= arm-none-eabi
CC		= $(PREFIX)-gcc
LD		= $(PREFIX)-gcc
OBJCOPY		= $(PREFIX)-objcopy
CFLAGS		+= -Os -g -Wall -Wextra -I$(TOOLCHAIN_DIR)/include -Iplatform -Idevices -I.\
		   -fno-common -mcpu=cortex-m3 -mthumb -msoft-float -MD -DSTM32F1 \
		   -DVERSION=\"$(VERSION)\"
LDSCRIPT	?= vesna.ld
LDFLAGS		+= -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group \
		   -L$(TOOLCHAIN_DIR)/lib -L$(TOOLCHAIN_DIR)/lib/stm32/f1 \
		   -T$(LDSCRIPT) -nostartfiles -Wl,--gc-sections \
		   -mthumb -march=armv7 -mfix-cortex-m3-ldrd -msoft-float
OBJS		+= main.o device.o buffer.o task.o platform/timer.o platform/rtc.o
LIBS		+= -lopencm3_stm32f1

OPENOCD		?= openocd
OPENOCD_PARAMS  ?= -f interface/olimex-arm-usb-ocd.cfg -f target/stm32f1x.cfg

ifeq ($(MODEL),sne-crewtv)
	LIBS += -ltda18219
	OBJS += platform/tda18219.o platform/ad8307.o devices/device-tda18219.o
	CFLAGS += -DTUNER_TDA18219 -DMODEL_SNE_CREWTV
	MODEL_OK = ok
endif

ifeq ($(MODEL),sne-ismtv-uhf)
	LIBS += -ltda18219
	OBJS += platform/tda18219.o platform/ad8307.o devices/device-tda18219.o
	CFLAGS += -DTUNER_TDA18219 -DMODEL_SNE_ISMTV_UHF
	MODEL_OK = ok
endif

ifeq ($(MODEL),sne-ismtv-868)
	OBJS += platform/cc.o devices/device-cc.o
	CFLAGS += -DTUNER_CC -DMODEL_SNE_ISMTV_868
	MODEL_OK = ok
endif

ifeq ($(MODEL),sne-ismtv-2400)
	OBJS += platform/cc.o devices/device-cc.o
	CFLAGS += -DTUNER_CC -DMODEL_SNE_ISMTV_2400
	MODEL_OK = ok
endif

ifeq ($(MODEL),snr-trx-868)
	OBJS += platform/cc.o devices/device-cc.o
	CFLAGS += -DTUNER_CC -DMODEL_SNR_TRX_868
	MODEL_OK = ok
endif

ifeq ($(MODEL),snr-trx-2400)
	OBJS += platform/cc.o devices/device-cc.o
	CFLAGS += -DTUNER_CC -DMODEL_SNR_TRX_2400
	MODEL_OK = ok
endif

ifeq ($(MODEL),dummy)
	OBJS += devices/device-dummy.o
	CFLAGS += -DTUNER_NULL
	MODEL_OK = ok
endif

all: $(BINARY).elf

%.bin: %.elf
	$(OBJCOPY) -Obinary $(*).elf $(*).bin

%.elf: $(OBJS) $(LDSCRIPT)
	$(LD) -o $(*).elf $(OBJS) $(LIBS) $(LDFLAGS)

%.o: %.c check-model
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

check-model:
ifndef MODEL_OK
	$(error Please select hardware model with MODEL environment)
endif

test:
	$(MAKE) -C tests test

.PHONY: clean check-model test

-include $(OBJS:.o=.d)
