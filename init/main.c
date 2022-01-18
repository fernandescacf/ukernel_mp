#include <types.h>
#include <memmgr.h>
#include <kvspace.h>
#include <loader.h>
#include <scheduler.h>
#include <procmgr.h>
#include <system.h>
#include <rfs.h>
#include <isr.h>
#include <sleep.h>

#include <board.h>
#include <arch.h>

#include <string.h>
#include <uart.h>
// System call used For initial tests

#include <spinlock.h>

spinlock_t lock = 0;

void DebugOut(const char *str)
{
	spinlock(&lock);
	if(str)
	{
		puts(str);
	}
	spinunlock(&lock);
}
// System call used For initial tests
int32_t DebugIn(char *str)
{
	if(str == NULL)
	{
		return -1;
	}
	gets(str);

	return (int32_t)strlen(str);
}

char *itoa(uint32_t num, char *str, uint32_t base)
{
    uint32_t i = 0, skip = 0, rem, len = 0, n;

	if(num == 0)
	{
		str[0] = '0';
		str[1] = '\0';
		return str;
	}

    if(num < 0 && base == 10)
    {
        num = -num;
        str[0] = '-';
        len = skip = 1;
    }

    n = num;
    while (n != 0)
    {
        len++;
        n /= base;
    }

    for ( ; (i + skip) < len; i++)
    {
        rem = num % base;
        num = num / base;
        str[len - (i + 1)] = (rem > 9)? (rem-10) + 'a' : rem + '0';
    }

    str[len] = '\0';

    return str;
}

char _str[20] = {0};

process_t * AbortHandler(uint32_t type, uint32_t pc, uint32_t sp, uint32_t lr)
{
	if(type == 1)
	{
		puts("\n===> Abort!!! Process: ");
	}
	else if(type == 2)
	{
		puts("\n===> Undef!!! Process: ");
	}
	else if(type == 3)
	{
		puts("\n===> Prefetch!!! Process: ");
	}

	if(SchedGetRunningProcess())
	{
		putc(SchedGetRunningProcess()->pid + '0');
	}

	puts("\nPC: ");
	puts(itoa(pc, _str, 16));
	puts("\nSP: ");
	puts(itoa(sp, _str, 16));
	puts("\nLR: ");
	puts(itoa(lr, _str, 16));

	if(SchedGetRunningProcess() == NULL)
	{
		while(1);
	}

	return SchedGetRunningProcess();
}

void *RunningTaskTcb()
{
	return SchedGetRunningTask()->memory.registers;
}

#ifdef USE_ABORT_HANDLER

void *AbortDataHandler(uint32_t cause, vaddr_t faddr)
{
	task_t *task = SchedGetRunningTask();

	if(cause & (0x1 << 11))
	{
		if(faddr < task->memory.sp && faddr > (vaddr_t)(((uint32_t)task->memory.sp) - task->memory.spMaxSize))
		{
			// Expand stack
			if(TaskExpandStack(task, PAGE_SIZE) == E_OK)
			{
				return task->memory.registers;
			}
		}
	}

	puts("\nProcess ");
	puts(itoa(task->parent->pid, _str, 10));
	puts("Task ");
	puts(itoa((task->tid) & 0xFFFF, _str, 10));
	puts(" Failed: Data Abort @ 0x");
	puts(itoa((uint32_t)faddr, _str, 16));

	return NULL;
}

#else

void AbortDataHandler(uint32_t cause, vaddr_t faddr, uint32_t* registers)
{
	(void)cause;
	(void)faddr;

	char str[12];

	task_t *task = SchedGetRunningTask();
	process_t* process = SchedGetRunningProcess();

	spinlock(&lock);

	getc();

	puts("\nDATA ABORT!!!");
	puts("\nCPU: ");
	puts(itoa(RUNNING_CPU, str, 10));
	puts(" Process ");
	puts(itoa(task->parent->pid, str, 10));
	puts("/");
	puts(itoa((uint32_t)process, str, 16));
	puts(" Task ");
	puts(itoa((task->tid) & 0xFFFF, str, 10));
	puts(" State ");
	puts(itoa(task->state, str, 10));
	puts(" Substate ");
	puts(itoa(task->subState, str, 10));

	puts("\nRegisters: @ 0x");
	puts(itoa((uint32_t)registers, str, 16));
	puts("\nR0: 0x");
	puts(itoa(registers[16], str, 16));
	puts("\nR1: 0x");
	puts(itoa(registers[15], str, 16));
	puts("\nR2: 0x");
	puts(itoa(registers[14], str, 16));
	puts("\nR3: 0x");
	puts(itoa(registers[13], str, 16));
	puts("\nR4: 0x");
	puts(itoa(registers[12], str, 16));
	puts("\nR5: 0x");
	puts(itoa(registers[11], str, 16));
	puts("\nR6: 0x");
	puts(itoa(registers[10], str, 16));
	puts("\nR7: 0x");
	puts(itoa(registers[9], str, 16));
	puts("\nR8: 0x");
	puts(itoa(registers[8], str, 16));
	puts("\nR9: 0x");
	puts(itoa(registers[7], str, 16));
	puts("\nR10: 0x");
	puts(itoa(registers[6], str, 16));
	puts("\nR11: 0x");
	puts(itoa(registers[5], str, 16));
	puts("\nR12: 0x");
	puts(itoa(registers[4], str, 16));
	puts("\nPC: 0x");
	puts(itoa(registers[2], str, 16));
	puts("\nCPSR: 0x");
	puts(itoa(registers[3], str, 16));
	puts("\nSYS SP: 0x");
	puts(itoa(registers[1], str, 16));
	puts("\nSYS LR: 0x");
	puts(itoa(registers[0], str, 16));
}

#endif

void svc_dump(uint32_t svc)
{
	task_t *task = SchedGetRunningTask();
	DebugOut("\nCPU: ");
	DebugOut(itoa(RUNNING_CPU, _str, 10));
	DebugOut(" Process ");
	DebugOut(itoa(task->parent->pid, _str, 10));
	DebugOut(" Task: ");
	DebugOut(itoa((task->tid) & 0xFFFF, _str, 10));
	DebugOut("\nSVC: ");
	DebugOut(itoa(svc, _str, 10));
}

void* FaultDumper(uint32_t fault, vaddr_t addr, vaddr_t user_sp, vaddr_t user_lr)
{
	char __str[11];

	DebugOut("\nCPU: ");
	DebugOut(itoa(RUNNING_CPU, __str, 10));

	task_t *task = SchedGetRunningTask();
	if(task != NULL)
	{
		DebugOut(" Process ");
		DebugOut(itoa(task->parent->pid, __str, 10));
		DebugOut(" Task: ");
		DebugOut(itoa((task->tid) & 0xFFFF, __str, 10));
	}
	DebugOut("\nFailed: ");

	switch(fault)
	{
	case 0:
		DebugOut("Data Abort");
		break;
	case 1:
		DebugOut("Pre-fetch Abort");
		break;
	case 2:
		DebugOut("Undefined Abort");
		break;
	}

	DebugOut("\nUser SP: 0x");
	DebugOut(itoa((uint32_t)user_sp, __str, 16));
	DebugOut("\nUser LR: 0x");
	DebugOut(itoa((uint32_t)user_lr, __str, 16));
	DebugOut("\nUser PC: 0x");
	DebugOut(itoa((uint32_t)addr, __str, 16));

	return NULL;
}

void kernelMain(bootLayout_t *bootLayout)
{
	// Initialize the Raw File System
	RfsInit((vaddr_t)bootLayout->rfs.base, bootLayout->rfs.size);

	// First thing initialize the memory system to be able allocate memory
	MemoryManagerInit(bootLayout);

	// Register devices
	RfsRegisterDevices();

	// Initialize the kernel virtual space manager
	VirtualSpaceInit();

	UartOpen();

	DebugOut("\n****NeoKernel****");
	DebugOut("\nVersion: ");
	DebugOut(RfsGetVersion());
	DebugOut("\nArch: ");
	DebugOut(RfsGetArch());
	DebugOut("\nBoard: ");
	DebugOut(RfsGetMach());

	DebugOut("\n\nInitialize loader");

	BoardEarlyInit();

	// Initialize elf loader
	LoaderInitialize();

	DebugOut("\nInitialize Scheduler");
	// Initialize Scheduler  NOTE: Will unlock secondary cpus
	SchedulerInit(1000);

	// Initialize sleep handler
	SleepInit();

	DebugOut("\nInitialize Process Manager");
	// Initialize Process Manager
	ProcManagerInit();

	DebugOut("\nInitialize System Manager");
	// Initialize kernel System Manager
	SystemInit();

	// Run start up script from RFS to initialize the user processes
	RfsRunStartupScript();

	DebugOut("\nInitialize Interrupts");
	InterruptHandlerInit();

	// Once Scheduler is resume we no longer need the kernel stack
	// So it can be reused by the exception handlers

	DebugOut("\nStart Scheduler");
	// Enable scheduling
	SchedulerStart();
}

void kernelMainSec()
{
//	char string[11];
//	DebugOut("\nKernel start CPU: ");
//	DebugOut(itoa(RUNNING_CPU, string, 10));

	BoardSecCpuInit();

	SchedulerStart();
}

