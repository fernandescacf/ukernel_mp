COMPILER = arm-none-eabi

ARCH = arm
VERSION = v7-a
CPU = cortex-a9

#BOARD = ve
#VARIANT = -DARM_FVP
#VARIANT = -DQEMU
#BOARD = sunxi
#VARIANT = -DH3

BOARD_CONFIG = -DBOARD_$(BOARD)

#CFLAGS = -c -g -Wall -fomit-frame-pointer -nostdlib -march=$(ARCH)$(VERSION)
CFLAGS = -c -Wall -fomit-frame-pointer -nostdlib -mcpu=cortex-a7
CC = $(COMPILER)-gcc
LD = $(COMPILER)-ld

ELF_FLAGS = -g -fno-common -fno-builtin -ffreestanding -nostdinc -fno-strict-aliasing
ELF_FLAGS += -mno-thumb-interwork -fno-stack-protector -fno-toplevel-reorder
ELF_FLAGS += -Wno-format-nonliteral -Wno-format-security

CFLAGS += $(ELF_FLAGS)
CFLAGS += $(BOARD_CONFIG)

export CFLAGS
export CC
export LD
export ARCH
export BOARD
export VARIANT

LD_DIR = arch/$(ARCH)/mach/$(BOARD)

TARGET = ukernel

.PHONY: arch
.PHONY: board
.PHONY: kernel
.PHONY: memory
.PHONY: lib
.PHONY: init
.PHONY: user

all: arch init memory kernel lib $(BOARD) $(TARGET) bin


arch:
	$(MAKE) -C arch/$(ARCH)

kernel:
	$(MAKE) -C kernel

init:
	$(MAKE) -C init
	
memory:
	$(MAKE) -C memory
	
lib:
	$(MAKE) -C lib

user:
	$(MAKE) -C apps

$(BOARD):
	$(MAKE) -C arch/$(ARCH)/mach/$(BOARD)


$(TARGET):
	$(CC) -nostartfiles -T $(LD_DIR)/lscript.ld \
	arch.o board.o init.o memory.o kernel.o lib.o -o $(TARGET).elf
	
bin:
	$(COMPILER)-objcopy -O binary $(TARGET).elf $(TARGET).bin

	rm *.o
	@echo 'Finished building'
