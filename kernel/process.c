/**
 * @file        process.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        16 August, 2018
 * @brief       Process Manager implementation
*/

/* Includes ----------------------------------------------- */

#include <process.h>
#include <task.h>

#include <string.h>
#include <kheap.h>
#include <memmgr.h>

#include <misc.h>
#include <mmu.h>
#include <arch.h>
#include <isr.h>

#include <mutex.h>
#include <semaphore.h>

#include <scheduler.h>
#include <ipc_2.h>

/* Private types ------------------------------------------ */


/* Private constants -------------------------------------- */

#define MAIN_TASK_ID		(0)
#define TASKS_POOL_SIZE		(4 * sizeof(task_t))
#define TASKS_POOL_FLAGS	(ALLOCATOR_CLEAN_MEMORY | ALLOCATOR_ALLOW_EXPAND)

/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */

static int32_t ProcessUnmapHandler(void *vm, pmm_t *mem)
{
	(void)vm;
	MemoryFree(mem->addr, mem->size);
	return E_OK;
}

static uint32_t ProcessMapSection(pgt_t pgt, glist_t *memory, vaddr_t baseAddr, ulong_t mapType, memCfg_t *memcfg)
{
	mmobj_t *obj = GLISTNODE2TYPE(GlistGetObject(memory, NULL) , mmobj_t, node);
	vaddr_t vaddr = baseAddr;

	uint32_t mapped = 0;

	while(NULL != obj)
	{
		vPageMapMemory(
				NULL,
				pgt,
				obj->addr,
				vaddr,
				obj->size,
				mapType,
				memcfg);

		mapped += ALIGN_UP(obj->size, PAGE_SIZE);
		vaddr = (vaddr_t)((uint32_t)vaddr + obj->size);
		obj = GLIST_NEXT(&obj->node, mmobj_t, node);
	}

	return mapped;
}

int32_t PrivObjectCmp(glistNode_t* node, void* addr)
{
	pobj_t* obj = GLISTNODE2TYPE(node, pobj_t, node);

	return (obj->vaddr - addr);
}

int32_t SharedRefObjectCmp(glistNode_t *node, void *addr)
{
	sref_t* obj = GLISTNODE2TYPE(node, sref_t, node);

	return (obj->map.vaddr - addr);
}

int32_t DeviceObjectCmp(glistNode_t *node, void *addr)
{
	dev_obj_t *obj = GLISTNODE2TYPE(node, dev_obj_t, node);

	return (obj->vaddr - addr);
}

static int32_t ProcessMemoryInit(process_t *proc, procAttr_t *attr)
{
	// Get a Page Table for the process
	proc->Memory.pgt = PageTableAlloc();

	// Generate Tasks stack base address and area size
	// Get stack memory area size
	size_t stackArea = attr->stacksSize * attr->maxTasks;
	// Get stack area base address
	proc->Memory.stacksBase = (vaddr_t)(attr->virtualSpaceSize - stackArea);

	// Initialize Stacks Handler
	(void)sManagerInitialize(
			&proc->Memory.stacksManager,
			proc->Memory.pgt,
			proc->Memory.stacksBase,
			(vaddr_t)((uint32_t)proc->Memory.stacksBase + stackArea));
	// Set Unmap handler for the Stacks
	(void)sManagerSetUnmapHandler(&proc->Memory.stacksManager, NULL, ProcessUnmapHandler);

	// Generate mmap area base address
	// Generate .mmap base address: next Megabyte aligned address after .data top address
	proc->Memory.mmapBase = (vaddr_t)ALIGN_UP((uint32_t)proc->exec.load->topAddr, 0x00100000);

	// Initialize Mmap Handler
	(void)vManagerInitialize(
			&proc->Memory.mmapManager,
			proc->Memory.pgt,
			proc->Memory.mmapBase,
			proc->Memory.stacksBase,
			VMANAGER_NORMAL);
	// We don't want to automatically release the memory
	// We have the memory in mmap tracked in the priv and shared lists

	GlistInitialize(&proc->Memory.privList, GList);
	GlistSetSort(&proc->Memory.privList, GlisFifotSort);
	GlistSetCmp(&proc->Memory.privList, PrivObjectCmp);

	GlistInitialize(&proc->Memory.sharedList, GList);
	GlistSetSort(&proc->Memory.sharedList, GlisFifotSort);
	GlistSetCmp(&proc->Memory.sharedList, SharedRefObjectCmp);

	GlistInitialize(&proc->Memory.devicesList, GList);
	GlistSetSort(&proc->Memory.devicesList, GlisFifotSort);
	GlistSetCmp(&proc->Memory.devicesList, DeviceObjectCmp);

	// Map .text section
	proc->Memory.memUsed += ProcessMapSection(
			proc->Memory.pgt,
			&proc->exec.load->text.memory,
			proc->exec.load->text.addr,
			PAGE_USER_TEXT, NULL);

	// Map .data + .bss sections
	proc->Memory.memUsed += ProcessMapSection(
			proc->Memory.pgt,
			&proc->exec.private.dataMemory,
			((proc->exec.load->data.addr != NULL) ? (proc->exec.load->data.addr) : (proc->exec.load->bss.addr)),
			PAGE_USER_DATA, NULL);

	return E_OK;
}

static void ProcessCleanPrivateMemory(glist_t* privList)
{
	while(!GLIST_EMPTY(privList))
	{
		pobj_t* priv = GLISTNODE2TYPE(GlistRemoveFirst(privList), pobj_t, node);

		while(--priv->parts >= 0)
		{
			MemoryFree((ptr_t)priv->memory[priv->parts].data, priv->memory[priv->parts].size);
		}

		vSpaceRelease(priv->vspace);
		kfree(priv->memory, sizeof(*priv->memory));
		kfree(priv->memcfg, sizeof(*priv->memcfg));
		kfree(priv, sizeof(*priv));
	}
}

static void ProcessCleanSharedMemory(process_t* process, glist_t* sharedList)
{
	while(!GLIST_EMPTY(sharedList))
	{
		sref_t* sref = GLISTNODE2TYPE(GlistRemoveFirst(sharedList), sref_t, node);

		// Update reference counter of shared object
		sref->shared->refs -= 1;

		vSpaceRelease(sref->map.vspace);

		clink_t* link = (clink_t*)VectorPeek(&process->connections, sref->coid);
		if(link != NULL)
		{
			// Connection may be null if the process is terminating
			link->refs--;
			link->privMap = NULL;
		}
		kfree(sref->map.memcfg, sizeof(*sref->map.memcfg));

		kfree(sref, sizeof(*sref));
	}
}

static void ProcessCleanDevices(process_t *process, glist_t *devicesList)
{
	while(!GLIST_EMPTY(devicesList))
	{
		ProcessCleanDevice
		(
				process,
				GLISTNODE2TYPE(GlistGetFirst(devicesList), dev_obj_t, node)
		);
	}
}

/* Private functions -------------------------------------- */

/**
 * ProcessInit Implementation (See header file for description)
*/
int32_t ProcessInit(process_t *proc, procAttr_t *attr, const char *argv)
{
	// We get the process handler already with the pid set and all other fields set to NULL

	// Set process privilege level
	proc->privilege = attr->privilege;

	// Initialize process memory handlers
	(void)ProcessMemoryInit(proc, attr);

	AllocatorInit(&proc->tasksPool, TASKS_POOL_SIZE, sizeof(task_t), TASKS_POOL_FLAGS);
	GlistInitialize(&proc->tasks, GFifo);

	// Set Main Task attributes
	taskAttr_t taskAttr;
	taskAttr.priority = attr->priority;
	taskAttr.detached = FALSE;
	taskAttr.stackSize = attr->stacksSize;

	// Create Main Task
	ProcessTaskCreate(proc, &taskAttr, (void*)argv, proc->exec.load->entry, proc->exec.load->exit, TRUE);

	// Initialize Child Process list
	GlistInitialize(&proc->childprocs, GFifo);

	// Initialize mutexs list
	GlistInitialize(&proc->mutexs, GFifo);
	// Initialize semaphores list
	GlistInitialize(&proc->semaphores, GFifo);

	// The process starts without any channel created
	(void)VectorInit(&proc->channels, 0);

	// Process connections
	(void)VectorInit(&proc->connections, 3);
	// Connection zero is reserved for system
	VectorInsertAt(&proc->connections, NULL, 0);

	GlistInitialize(&proc->pendingTasks, GFifo);

	return E_OK;
}

int32_t ProcessCopyConnectionsRange(process_t* parent, process_t* child, uint32_t fd_count, int32_t* fd_map)
{
	if(fd_count == 0)
	{
		return ProcessCopyConnections(parent, child);
	}

	uint32_t index = 1;
	for( ; index < fd_count; index++)
	{
		if(fd_map[index] == -1)
		{
			continue;
		}

		clink_t* connection = (clink_t *)VectorPeek(&parent->connections, fd_map[index]);

		if((connection == NULL) || (connection->connection->flags & CONNECTION_PRIVATE))
		{
			continue;
		}

		clink_t* clink = (clink_t*)kmalloc(sizeof(clink_t));

		clink->connection = connection->connection;
		clink->flags = connection->flags;
		clink->refs = 1;
		clink->pid = child->pid;
		// TODO: In the future we will also copy the mappings
		clink->privMap = NULL;
		GlistInsertObject(&clink->connection->clinks, &clink->node);

		VectorInsertAt(&child->connections, clink, index);
	}

	return E_OK;
}

int32_t ProcessCopyConnections(process_t* parent, process_t* child)
{
	uint32_t count = VectorUsage(&parent->connections) - 1;
	uint32_t index = 1;
	for( ; count > 0; index++)
	{
		clink_t* connection = (clink_t *)VectorPeek(&parent->connections, index);

		if(connection == NULL)
		{
			continue;
		}

		count--;

		// If connection is set as private with do not copy it
		if(connection->connection->flags & CONNECTION_PRIVATE)
		{
			continue;
		}

		clink_t* clink = (clink_t*)kmalloc(sizeof(clink_t));

		clink->connection = connection->connection;
		clink->flags = connection->flags;
		clink->refs = 1;
		clink->pid = child->pid;
		clink->coid = (int32_t)index;
		// TODO: In the future we will also copy the mappings
		clink->privMap = NULL;
		GlistInsertObject(&clink->connection->clinks, &clink->node);

		if(VectorInsertAt(&child->connections, clink, index) != index)
		{
			// This should never happen!!!
			return E_ERROR;
		}
	}

	return E_OK;
}

task_t *ProcessTaskCreate(process_t *process, taskAttr_t *taskAttr, void *arg, void *entry, void* exit, bool_t mainTask)
{
	int32_t id;
	task_t *task = (task_t*)AllocatorGet(&process->tasksPool, &id);

	if(task == NULL)
	{
		return NULL;
	}

	task->parent = process;
	task->tid = ((process->pid << 16) | (uint32_t)id);

	// Set Task parameters
	taskParam_t taskParam;
	taskParam.arg = arg;
	taskParam.entry = entry;
//	taskParam.exit = process->exec.load->exit;
	taskParam.exit = exit;

	if(TaskInit(task, taskAttr, &taskParam) != E_OK)
	{
		AllocatorFree(&process->tasksPool, task);
		return NULL;
	}

	const char *argv = ((mainTask == TRUE) ? (arg) : NULL);

	if(TaskSetStack(&process->Memory.stacksManager, PAGE_SIZE, task, argv) != E_OK)
	{
		AllocatorFree(&process->tasksPool, task);
		return NULL;
	}

	GlistInsertObject(&process->tasks, &task->siblings);

	return task;
}

/**
 * ProcessStart Implementation (See header file for description)
*/
int32_t ProcessStart(process_t *proc)
{
	// At this point the process if fully initialized so we can add the main task to the Ready List
	task_t *task = AllocatorToAddr(&proc->tasksPool, MAIN_TASK_ID);

	SchedAddTask(task);

	return E_OK;
}

/**
 * ProcessAddChild Implementation (See header file for description)
*/
int32_t ProcessAddChild(process_t *parent, process_t *child)
{
	child->parent = parent;
	return GlistInsertObject(&parent->childprocs, &child->siblings);
}

/**
 * ProcessIsMainTask Implementation (See header file for description)
*/
bool_t ProcessIsMainTask(uint32_t tid)
{
	return ((tid & 0xFFFF) == MAIN_TASK_ID);
}

/**
 * ProcessMemoryClean Implementation (See header file for description)
*/
void ProcessMemoryClean(process_t *process)
{
	// Clean Tasks
	while(!GLIST_EMPTY(&process->tasks))
	{
		task_t *task = GLISTNODE2TYPE(GlistRemoveObject(&process->tasks, NULL), task_t, siblings);
		TaskClean(task);
	}

	AllocatorDestroy(&process->tasksPool);

	// Release all private allocated memory
	ProcessCleanPrivateMemory(&process->Memory.privList);

	// Release all shared allocated memory
	ProcessCleanSharedMemory(process, &process->Memory.sharedList);

	// Release all allocated devices
	ProcessCleanDevices(process, &process->Memory.devicesList);

	// Clean MMAP section
	vManagerDestroy(&process->Memory.mmapManager);

	// Clean Stacks section
	sManagerDestroy(&process->Memory.stacksManager);

	// Free MMU Page Table
	PageTableDealloc(process->pid, process->Memory.pgt);
}

/**
 * ProcessTerminate Implementation (See header file for description)
*/
void ProcessTerminate(process_t *process)
{
	// While we are stopping tasks block the scheduler
	uint32_t status;
	SchedLock(&status);

	// Stop Blocked Tasks (not the ready ones)
	task_t *task = GLIST_FIRST(&process->tasks, task_t, siblings);
	while(task)
	{
		if(task != SchedGetRunningTask()) TaskTerminate(task, NULL, TRUE);
		task = GLIST_NEXT(&task->siblings, task_t, siblings);
	}

	// Stop all process tasks still running and clean scheduler queue
	SchedKillProcessTasks(process);

	SchedUnlock(&status);

	// Wait for all tasks to be terminated
	while(process->tasksRunning > 1)
	{
		SchedYield();
	}

	// At this point we can start to clean allocated resources

	// We start by closing the IPC channels and connections to avoid conflicts
	// With ongoing communications trying to resume tasks blocked in the IPC

	// Close all open IPC Channels
	uint32_t count = VectorUsage(&process->channels);
	uint32_t index = 0;
	for( ; count > 0; index++)
	{
		channel_t *channel = (channel_t *)VectorPeek(&process->channels, index);

		if(channel == NULL)
		{
			continue;
		}

		count--;

		//ChannelDestroy(index);
		ker_ChannelDestroy(process, channel);
	}

	// Close all open IPC Connections remove 1 to skip the system connection
	count = VectorUsage(&process->connections) - 1;
	index = 1;
	for( ; count > 0; index++)
	{
		clink_t* connection = (clink_t *)VectorPeek(&process->connections, index);

		if(connection == NULL)
		{
			continue;
		}

		count--;

		//ConnectDetach(index);
		ker_ConnectDetach(process, connection, TRUE);
	}

	// Clean allocated memory from vectors
	VectorFree(&process->channels);
	VectorFree(&process->connections);

	// Release any allocated interrupt service
	task = GLIST_FIRST(&process->tasks, task_t, siblings);
	while(task)
	{
		InterruptClean(task);
		task = GLIST_NEXT(&task->siblings, task_t, siblings);
	}

	// For mutexs and semaphores we can just free the allocated handlers
	while(!GLIST_EMPTY(&process->mutexs))
	{
		kfree(GLISTNODE2TYPE(GlistRemoveFirst(&process->mutexs), mutex_t, pnode), sizeof(mutex_t));
	}

	while(!GLIST_EMPTY(&process->semaphores))
	{
		kfree(GLISTNODE2TYPE(GlistRemoveFirst(&process->semaphores), sem_t, node), sizeof(sem_t));
	}

	// Unblock pending tasks from parent process
	while(!GLIST_EMPTY(&process->pendingTasks))
	{
		task = GLISTNODE2TYPE(GlistRemoveFirst(&process->pendingTasks), task_t, node);
		task->ret = E_OK;
		SchedAddTask(task);
	}
}

/**
 * ProcessGetTask Implementation (See header file for description)
*/
task_t *ProcessGetTask(process_t *process, uint32_t tid)
{
	task_t *task = (task_t*)AllocatorToAddr(&process->tasksPool, (tid & 0xFFFF));

	if((task != NULL) && (task->tid == tid))
	{
		return task;
	}

	return NULL;
}

vaddr_t ProcessRegisterPrivMemory(process_t* process, mbv_t* memory, int32_t parts, size_t size, memCfg_t* memcfg)
{
	pobj_t* privobj = (pobj_t*)kmalloc(sizeof(pobj_t));
	privobj->memory = memory;
	privobj->parts = parts;
	privobj->size = size;
	// Get Virtual space from the mmap area
	privobj->vspace = vSpaceReserve(&process->Memory.mmapManager, size);
	privobj->vaddr = privobj->vspace->base;
	privobj->memcfg = memcfg;
	privobj->refs = 0;

	int32_t i;
	for(i = 0; i < parts; i++)
	{
		vSpaceMapSection(privobj->vspace, (paddr_t)memory[i].data, memory[i].size, PAGE_CUSTOM, memcfg);
	}

	// Insert in private memory objects list
	GlistInsertObject(&process->Memory.privList, &privobj->node);

	return privobj->vaddr;
}

//sref_t* ProcessRegisterShareMemory(process_t* process, uint32_t coid, mbv_t* memory, int32_t parts, size_t size, memCfg_t* memcfg)
sref_t* ProcessRegisterShareMemory(process_t* process, uint32_t coid, sobj_t* sobj, memCfg_t* memcfg)
{
	sref_t* sref = (sref_t*)kmalloc(sizeof(sref_t));
	sref->coid = coid;
	sref->shared = sobj;
	sref->map.size = sobj->obj->size;
	sref->map.vspace = vSpaceReserve(&process->Memory.mmapManager, sref->map.size);
	sref->map.vaddr = sref->map.vspace->base;
	sref->map.memcfg = memcfg;

	int32_t i;
	for(i = 0; i < sobj->obj->parts; i++)
	{
		vSpaceMapSection(sref->map.vspace, (paddr_t)sobj->obj->memory[i].data, sobj->obj->memory[i].size, PAGE_CUSTOM, memcfg);
	}

	// Insert in shared memory objects list
	GlistInsertObject(&process->Memory.sharedList, &sref->node);

	// Update references to this shared object
	sobj->refs++;

	return sref;
}

dev_obj_t *ProcessRegisterDevice(process_t *process, dev_t *device, memCfg_t *memcfg)
{
	dev_obj_t *devobj = (dev_obj_t*)kmalloc(sizeof(dev_obj_t));
	devobj->dev = device;
	// There is no guarantee that the device address and size respects the page size limits
	paddr_t alignAddr = (paddr_t)ALIGN_DOWN(((uint32_t)device->addr), PAGE_SIZE);
	size_t  alignSize = ROUND_UP(device->size, PAGE_SIZE);
	// Get Virtual space from the mmap area
	devobj->vspace = vSpaceReserve(&process->Memory.mmapManager, alignSize);
	devobj->memcfg = memcfg;
	// Map device
	devobj->vaddr = vSpaceMap(devobj->vspace, alignAddr, PAGE_CUSTOM, memcfg);

	// Adjust virtual address in case base address for the device is not page aligned
	devobj->vaddr = (vaddr_t)(((uint32_t)devobj->vaddr) + (((uint32_t)device->addr) & (PAGE_SIZE - 1)));

	// Insert in shared memory objects list
	GlistInsertObject(&process->Memory.devicesList, &devobj->node);

	return devobj;
}

int32_t ProcessCleanPrivateObject(pobj_t* obj)
{
	// Check if this object is being shared, if yes do not free it
	if(obj->refs > 0)
	{
		return E_BUSY;
	}

	if(GlistRemoveSpecific(&obj->node) != E_OK)
	{
		return E_INVAL;
	}

	uint32_t i = obj->parts;
	while(--obj->parts >= 0)
	{
		MemoryFree((ptr_t)obj->memory[obj->parts].data, obj->memory[obj->parts].size);
	}

	vSpaceRelease(obj->vspace);
	kfree(obj->memory, sizeof(*obj->memory) * i);
	kfree(obj->memcfg, sizeof(*obj->memcfg));
	kfree(obj, sizeof(*obj));

	return E_OK;
}

int32_t ProcessCleanSharedRef(process_t* process, sref_t* obj)
{
	if(GlistRemoveSpecific(&obj->node) != E_OK)
	{
		return E_INVAL;
	}

	// Unmap all memory
	vSpaceRelease(obj->map.vspace);

	// Update shared object references
	obj->shared->refs -= 1;

	clink_t* link = (clink_t*)VectorPeek(&process->connections, obj->coid);
	// Remove object from connection
	link->privMap = NULL;

	// Clean memory
	kfree(obj->map.memcfg, sizeof(*obj->map.memcfg));

	kfree(obj, sizeof(*obj));

	return E_OK;
}

int32_t ProcessCleanDevice(process_t* process, dev_obj_t* device)
{
	if(GlistRemoveSpecific(&device->node) != E_OK)
	{
		return E_INVAL;
	}

	vSpaceRelease(device->vspace);

	DeviceFree(device->dev);

	kfree(device->memcfg, sizeof(*device->memcfg));

	kfree(device, sizeof(*device));

	return E_OK;
}
