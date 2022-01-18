ARCH_DIR = ../../..
MEMORY_DIR = ../../../../memory
KERNEL_DIR = ../../../../kernel
LIB_DIR = ../../../../lib

INCLUDES = -Iinclude -I$(ARCH_DIR)/include -I$(ARCH_DIR)/arm/include \
		   -I$(KERNEL_DIR)/include -I$(MEMORY_DIR)/include -I$(LIB_DIR)/include

BOARD_CONIG += $(VARIANT)

all: uart ve timer
	$(LD) -r uart.o ve.o timer.o -o ../../../../board.o
	rm *.o

timer:
	$(CC) $(CFLAGS) privtimer.c $(INCLUDES) $(BOARD_CONIG) -o timer.o

uart:
	$(CC) $(CFLAGS) pl011_uart.c $(INCLUDES) $(BOARD_CONIG) -o uart.o

ve:
	$(CC) $(CFLAGS) board.c $(INCLUDES) $(BOARD_CONIG) -o ve.o