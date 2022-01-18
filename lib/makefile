ARCH_DIR= ../arch
MEMORY_DIR = ../memory
KERNEL_DIR = ../kernel

INCLUDES = -Iinclude -I$(KERNEL_DIR)/include -I$(ARCH_DIR)/include -I$(MEMORY_DIR)/include

all: string glist vector allocator
	$(LD) -r string.o glist.o vector.o allocator.o -o ../lib.o
	rm *.o

string:
	$(CC) $(CFLAGS) $(ARCH_DIR)/$(ARCH)/string.S $(INCLUDES) -o string.o

glist:
	$(CC) $(CFLAGS) glist.c $(INCLUDES) -o glist.o
	
vector:
	$(CC) $(CFLAGS) vector.c $(INCLUDES) -o vector.o

allocator:
	$(CC) $(CFLAGS) allocator.c $(INCLUDES) -o allocator.o
