/**
 * @file        proctypes.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        10 April, 2020
 * @brief       Process Manager Types Definition Header File
*/

#ifndef _PROCTYPES_H_
#define _PROCTYPES_H_

#ifdef __cplusplus
    extern "C" {
#endif


/* Includes ----------------------------------------------- */
#include <types.h>
#include <glist.h>
#include <vector.h>
#include <allocator.h>
#include <vstack.h>
#include <vmap.h>
#include <loader.h>


/* Exported types ----------------------------------------- */

typedef struct PROCESS process_t;
typedef struct TASK    task_t;

typedef enum
{
	DEAD = 0,
	RUNNING, READY, BLOCKED
}state_t;

typedef enum
{
	NONE = 0,
	IPC_SEND, IPC_REPLY, IPC_RECEIVE,
	SEMAPHORE, MUTEX, KERNEL_MUTEX, COND,
	SLEEPING, JOINED, INTERRUPT_PENDING, SIGNAL_PENDING
}subState_t;

typedef struct
{
	uint32_t id;
	uint32_t flags;
	int32_t  errno;
	size_t   keysSize;
	void**   keys;
	void*    cleanup;
}tls_t;

typedef struct TASK
{
    process_t*  parent;         // parent process
    glistNode_t siblings;       // siblings tasks
    uint32_t    tid;            // task id -> (pid << 16 | tid)
    state_t     state;          // task state
    subState_t  subState;       // task sub state
    uint16_t    real_prio;      // task priority
    uint16_t    active_prio;    // task active priority
    uint32_t    flags;          // Detached, Privilege Level
    uint64_t    on_time;        // task cpu time used

    glistNode_t node;           // node used to add task to block lists
    void*       block_on;       // where task is blocked
    task_t*     client;         // task we are working on behalf
    int32_t     chid;           // task serving channel
    int32_t     ret;            // pass errors between block and resume

    union
    {
        struct
        {
            // Channel/Connection Info
            int32_t     rcvid;
            int32_t     scoid;
            int32_t     coid;
            // Message Info
            int32_t     type;
            int32_t     code;
            const char* smsg;
            size_t      sbytes;
            const char* rmsg;
            size_t      rbytes;
            // Server task
            task_t*     server;
            // Read Helper
            uint32_t    read_off;
            // Write Helper
            uint32_t    write_off;
        }msg;

        struct
        {
            int32_t     scoid;
            int32_t     type;
            int32_t     data;
            void*       notification;
        }notify;

        struct
        {
            void*       value_ptr;
        }join;
    }data;

    struct
    {
        uint16_t    set;        // signals if time out is set
        uint16_t    type;       // auto clean, reload
        uint32_t    waitTime;   // max wait time
        glistNode_t node;
        void (*handler)(void*, task_t*);
        void*       arg;
        uint32_t    pendTime;   // pending time till wake up
    }timeout;

    struct
    {
        int32_t     irq;
        int32_t     id;
    }interrupt;

    glist_t     joined;         // joined tasks list
    glist_t     owned_mutexs;   // task owned mutexs

    struct
    {
        void*       registers;  // Pointer to structure that stores the register file
        tls_t*      tls;        // thread local storage
        vaddr_t		entry;      // thread entry point
        vaddr_t		exit;       // thread entry point
        vStack_t*   stack;      // stack address space handler
        size_t		spSize;     // stack size
        size_t		spMaxSize;  // Maximum stack size allowed for the task
        vaddr_t		sp;         // stack top
    }memory;

}task_t;


typedef struct PROCESS
{
	pid_t			pid;
	// Privilege level of the process
	uint32_t		privilege;
	// Parent pointer, if NULL it was created by the kernel or detached
	struct PROCESS	*parent;
	// Siblings process
	glistNode_t		siblings;
	// List of child processes
	glist_t			childprocs;
	// Owned tasks, task at position 0 is the main task
	allocator_t		tasksPool;
	glist_t			tasks;
	int32_t			tasksRunning;
	// Generic node to allow the handler to be added to other lists
	glistNode_t		node;
	// Execution arguments
	const char		*argv;

	// Executable memory and layout
	exec_t			exec;

	struct
	{
		// Process page table
		pgt_t		pgt;
		// Memory Statistics
		uint32_t	memUsed;
		// Process Local Storage
		//_pls_t	*pls;
		// Stacks base address
		vaddr_t		stacksBase;
		// Memory map base address (Heap space comes from here)
		vaddr_t		mmapBase;
		// Memory Map Virtual Space Manager
		vManager_t	mmapManager;
		// Stacks Virtual Space Manager
		sManager_t	stacksManager;
		// List of memory allocated with mmap of for the heap
		glist_t		privList;
		glist_t		sharedList;
		glist_t		devicesList;

		// TODO: PROCESS -> Process local storage
	}Memory;

	glist_t     mutexs;
	glist_t     semaphores;

	// IPC channels
	vector_t	channels;
	// IPC connections
	vector_t 	connections;

	// TODO: Parent tasks waiting --> In future remove from here
	glist_t     pendingTasks;

	// TODO: PROCESS -> Signals

	// TODO: PROCESS -> Interruptions

}process_t;

typedef struct
{
	uint16_t priority;
	uint16_t detached;
	size_t	 stackSize;
}taskAttr_t;

typedef struct
{
	vaddr_t		(*entry)(void*);
	vaddr_t 	(*exit)(void*);
	void 		*arg;
}taskParam_t;

typedef struct
{
	uint16_t	priority;
	uint16_t	privilege;
	uint8_t		detached;
	uint8_t		heritage;
	uint16_t	maxTasks;
	size_t		virtualSpaceSize;
	size_t		stacksSize;
	size_t		heapSize;
}procAttr_t;

/* Exported constants ------------------------------------- */

enum Privileges
{
	Limited,
	Basic,
	IO_Access,
	Full_Access
};

/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */


#ifdef __cplusplus
    }
#endif

#endif /* _PROCTYPES_H_ */
