ARCH_DIR = ..
MEMORY_DIR = ../../memory
KERNEL_DIR = ../../kernel
LIB_DIR = ../../lib

INCLUDES = -Iinclude -I$(ARCH_DIR)/include -I$(KERNEL_DIR)/include -I$(LIB_DIR)/include -I$(MEMORY_DIR)/include

all: boot cache mmu gic arch archS entry syscalls atomic spinlock elf scu cache_c
	$(LD) -r boot.o cache.o cache_c.o mmu.o gic.o arch.o archS.o atomic.o \
	entry.o syscalls.o spinlock.o elf.o scu.o -o ../../arch.o
	rm *.o

boot:
	$(CC) $(CFLAGS) boot.S $(INCLUDES) -o boot.o

scu:
	$(CC) $(CFLAGS) scu.S $(INCLUDES) -o scu.o

cache:
	$(CC) $(CFLAGS) cache.S $(INCLUDES) -o cache.o
	
cache_c:
	$(CC) $(CFLAGS) cache.c $(INCLUDES) -o cache_c.o

mmu:
	$(CC) $(CFLAGS) mmu.c $(INCLUDES) -o mmu.o

gic:
	$(CC) $(CFLAGS) gic.c $(INCLUDES) -o gic.o

arch:
	$(CC) $(CFLAGS) arch.c $(INCLUDES) -o arch.o

archS:
	$(CC) $(CFLAGS) arch.S $(INCLUDES) $(VARIANT) -o archS.o

entry:
	$(CC) $(CFLAGS) entry.S $(INCLUDES) -o entry.o
	
syscalls:
	$(CC) $(CFLAGS) syscalls.S $(INCLUDES) -o syscalls.o
	
atomic:
	$(CC) $(CFLAGS) atomic.S $(INCLUDES) -o atomic.o

spinlock:
	$(CC) $(CFLAGS) spinlock.S $(INCLUDES) -o spinlock.o

elf:
	$(CC) $(CFLAGS) elf.c $(INCLUDES) -o elf.o