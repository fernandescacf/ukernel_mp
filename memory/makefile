ARCH_DIR = ../arch
KERNEL_DIR = ../kernel
LIB_DIR = ../lib

INCLUDES = -Iinclude -I$(ARCH_DIR)/include -I$(KERNEL_DIR)/include -I$(LIB_DIR)/include

all: memmgr zone vpage vmap kvspace vstack zonedirect buddy zoneindirect devices mpool kheap mmap
	$(LD) -r memmgr.o zone.o vpage.o vmap.o kvspace.o vstack.o zonedirect.o buddy.o \
	devices.o zoneindirect.o mpool.o kheap.o mmap.o -o ../memory.o
	rm *.o

memmgr:
	$(CC) $(CFLAGS) memmgr.c $(INCLUDES) -o memmgr.o

zone:
	$(CC) $(CFLAGS) zone.c $(INCLUDES) -o zone.o

vmap:
	$(CC) $(CFLAGS) virtual/vmap.c $(INCLUDES) -o vmap.o
	
vstack:
	$(CC) $(CFLAGS) virtual/vstack.c $(INCLUDES) -o vstack.o
	
kvspace:
	$(CC) $(CFLAGS) virtual/kvspace.c $(INCLUDES) -o kvspace.o

vpage:
	$(CC) $(CFLAGS) virtual/vpage.c $(INCLUDES) -o vpage.o

zonedirect:
	$(CC) $(CFLAGS) zonedirect.c $(INCLUDES) -o zonedirect.o

buddy:
	$(CC) $(CFLAGS) buddy_2.c $(INCLUDES) -o buddy.o

devices:
	$(CC) $(CFLAGS) devices.c $(INCLUDES) -o devices.o

zoneindirect:
	$(CC) $(CFLAGS) virtual/zoneindirect.c $(INCLUDES) -o zoneindirect.o

mpool:
	$(CC) $(CFLAGS) mpool.c $(INCLUDES) -o mpool.o
	
kheap:
	$(CC) $(CFLAGS) kheap.c $(INCLUDES) -o kheap.o

mmap:
	$(CC) $(CFLAGS) mmap.c $(INCLUDES) -o mmap.o