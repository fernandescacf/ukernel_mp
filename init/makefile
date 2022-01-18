ARCH_DIR = ../arch
MEMORY_DIR = ../memory
KERNEL_DIR = ../kernel
LIB_DIR = ../lib

INCLUDES = -Iinclude -I$(ARCH_DIR)/include -I$(ARCH_DIR)/$(BOARD)/include\
		   -I$(KERNEL_DIR)/include -I$(ARCH_DIR)/arm/include -I$(MEMORY_DIR)/include -I$(LIB_DIR)/include

all: main
	$(LD) -r main.o -o ../init.o
	rm *.o

main:
	$(CC) $(CFLAGS) main.c $(INCLUDES) -o main.o
