ARCH_DIR = ../../..
MEMORY_DIR = ../../../../memory
KERNEL_DIR = ../../../../kernel
LIB_DIR = ../../../../lib

INCLUDES = -Iinclude -I$(ARCH_DIR)/include -I$(ARCH_DIR)/arm/include \
		   -I$(KERNEL_DIR)/include -I$(MEMORY_DIR)/include -I$(LIB_DIR)/include

all: uart h3 timer
	$(LD) -r uart.o h3.o timer.o -o ../../../../board.o
	rm *.o

uart:
	$(CC) $(CFLAGS) uart.c $(INCLUDES) -o uart.o

timer:
	$(CC) $(CFLAGS) timer.c $(INCLUDES) $(BOARD_CONIG) -o timer.o

h3:
	$(CC) $(CFLAGS) board.c $(INCLUDES) $(BOARD_CONIG) -o h3.o