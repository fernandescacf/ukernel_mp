ARCH_DIR = ../arch
MEMORY_DIR = ../memory
LIB_DIR = ../lib

INCLUDES = -Iinclude -I$(ARCH_DIR)/include -I$(MEMORY_DIR)/include -I$(LIB_DIR)/include

all: procmgr process task loader scheduler ipc system mutex sem rfs isr sleep cond klock rwlock
	$(LD) -r procmgr.o process.o task.o loader.o scheduler.o ipc.o \
	isr.o system.o mutex.o sem.o cond.o rfs.o sleep.o klock.o rwlock.o -o ../kernel.o
	rm *.o

procmgr:
	$(CC) $(CFLAGS) procmgr.c $(INCLUDES) -o procmgr.o

process:
	$(CC) $(CFLAGS) process.c $(INCLUDES) -o process.o

task:
	$(CC) $(CFLAGS) task.c $(INCLUDES) -o task.o

loader:
	$(CC) $(CFLAGS) loader.c $(INCLUDES) -o loader.o

scheduler:
	$(CC) $(CFLAGS) scheduler.c $(INCLUDES) -o scheduler.o

ipc:
	$(CC) $(CFLAGS) ipc_2.c $(INCLUDES) -o ipc.o
	
isr:
	$(CC) $(CFLAGS) isr.c $(INCLUDES) -o isr.o

system:
	$(CC) $(CFLAGS) system.c $(INCLUDES) -o system.o
	
mutex:
	$(CC) $(CFLAGS) mutex.c $(INCLUDES) -o mutex.o
	
sem:
	$(CC) $(CFLAGS) semaphore.c $(INCLUDES) -o sem.o

cond:
	$(CC) $(CFLAGS) cond.c $(INCLUDES) -o cond.o

rfs:
	$(CC) $(CFLAGS) rfs.c $(INCLUDES) -o rfs.o

sleep:
	$(CC) $(CFLAGS) sleep.c $(INCLUDES) -o sleep.o

klock:
	$(CC) $(CFLAGS) klock.c $(INCLUDES) -o klock.o

rwlock:
	$(CC) $(CFLAGS) rwlock.c $(INCLUDES) -o rwlock.o
