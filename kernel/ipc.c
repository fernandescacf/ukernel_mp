/**
 * @file        ipc.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        05 September, 2020
 * @brief       Interprocess Communication implementation
*/


/* Includes ----------------------------------------------- */
#include <ipc_2.h>
#include <scheduler.h>
#include <procmgr.h>
#include <process.h>
#include <kvspace.h>
#include <kheap.h>
#include <string.h>
#include <vector.h>
#include <spinlock.h>


/* Private types ------------------------------------------ */

typedef struct
{
	int32_t     scoid;
	pid_t       pid;
}msgCmp_t;


/* Private constants -------------------------------------- */
#define VECTOR_CONNECTIONS_SIZE      (4)
#define VECTOR_MESSAGES_SIZE         (8)

// Internal Channel control flags
// Flag to lock access to the a channel (we may need to change to a sync method)
#define CHANNEL_LOCK				(1 << 16)
// Flag that signals that the channel is alive
#define CHANNEL_ALIVE				(1 << 17)

// Flag used to mark a connection as invalid when the channel is destroyed
#define CONNECTION_INVALID			(1 << 16)
#define CLINK_DEAD					(1 << 0)

/* Private macros ----------------------------------------- */
#define RCVID(chid, id)				((chid << 16) | (id + 1))
#define MSGID(rcvid)				((rcvid & 0xFFFF) - 1)
#define MSGCHID(rcvid)				(rcvid >> 16)
#define MSGPID(msg)					(((msg)->tid) >> 16)

#define CONNECTION_USCOID(chid, scoid)	((chid << 16) | (scoid))

/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */

int32_t MsgListSort(glistNode_t* current, glistNode_t* newMsg)
{
	msg_t* currentMsg = GLISTNODE2TYPE(current, msg_t, hdr.node);
	msg_t* msg = GLISTNODE2TYPE(newMsg, msg_t, hdr.node);

	// Ensure that the older message with same priority is handled first
	if(currentMsg->hdr.priority == msg->hdr.priority)
	{
		return 1;
	}

    return (int32_t)currentMsg->hdr.priority - (int32_t)msg->hdr.priority;
}

int32_t MsgListMatchScoid(glistNode_t* current, void* cmp)
{
	msg_t* msg = GLISTNODE2TYPE(current, msg_t, hdr.node);

	msgCmp_t* msgCmp = (msgCmp_t*)cmp;

	return ((msg->hdr.scoid != msgCmp->scoid) | (msg->hdr.pid != msgCmp->pid));
}

msg_t* MsgGet(channel_t* channel)
{
	msg_t* msg = NULL;

	while(TRUE)
	{
		msg = GLIST_FIRST(&channel->send, msg_t, hdr.node);

		// Is sender task still alive? If not clean up message
		if(msg != NULL && msg->msg.ret == IPC_TASK_DEAD)
		{
			// Remove message from queue
			(void)GlistRemoveFirst(&channel->send);
			// Free message ID
			VectorRemove(&channel->messages, MSGID(msg->hdr.rcvid));
			// Clean Memory
			kfree(msg);
			// Try fetch a new message
			continue;
		}
		break;
	}

	if(msg == NULL)
	{
		return GLISTNODE2TYPE(GlistRemoveFirst(&channel->notify), msg_t, hdr.node);
	}

	msg_t* notify = GLIST_FIRST(&channel->notify, msg_t, hdr.node);

	if(notify == NULL)
	{
		return GLISTNODE2TYPE(GlistRemoveFirst(&channel->send), msg_t, hdr.node);
	}

	glist_t* list = ((msg->hdr.priority > notify->hdr.priority) ? (&channel->send) : (&channel->notify));

	return GLISTNODE2TYPE(GlistRemoveFirst(list), msg_t, hdr.node);
}

void MsgsFlushByScoid(glist_t* list, int32_t scoid, process_t* process)
{
	msgCmp_t cmp = { scoid, process->pid };

	while(TRUE)
	{
		msg_t* msg = GLISTNODE2TYPE(GlistRemoveObject(list, (void*)&cmp), msg_t, hdr.node);

		if(msg == NULL)
		{
			break;
		}

		if(msg->hdr.rcvid == 0)
		{
			kfree(&msg->notify);
		}
		else
		{
			msg->msg.ret = IPC_CHONNECTION_DEAD;
			SchedAddTask(ProcessGetTask(process, msg->msg.tid));
		}
	}
}

void MsgsFlush(glist_t* list)
{
	while(!GLIST_EMPTY(list))
	{
		msg_t* msg = GLISTNODE2TYPE(GlistRemoveFirst(list), msg_t, hdr.node);

		if(msg->hdr.rcvid == 0)
		{
			kfree(&msg->notify);
		}
		else
		{
			msg->msg.ret = IPC_CHANNEL_DEAD;
			SchedAddTask(ProcessGetTask(ProcGetProcess(MSGPID(&msg->msg)), msg->msg.tid));
		}
	}
}

void MsgsReceiverFlush(glist_t *list)
{
	while(!GLIST_EMPTY(list))
	{
		task_t* task = GLISTNODE2TYPE(GlistRemoveFirst(list), task_t, blocked.node);

		task->blocked.ret = IPC_CHANNEL_DEAD;

		SchedAddTask(task);
	}
}

void ChannelPriorityResolve(channel_t* channel, task_t* task, uint16_t prio)
{
	if((channel->flags & CHANNEL_FIXED_PRIORITY) || (prio <= channel->priority))
	{
		return;
	}

	uint32_t status;
	Klock(&channel->lock, &status);

	process_t* process = ProcGetProcess(channel->pid);

	if(task->info.subState == IPC_SEND)
	{
		send_t* msg = (send_t*)task->blocked.data;
		// Reinsert task
		GlistRemoveSpecific(&msg->hdr.node);
		GlistInsertObject(&channel->send, &msg->hdr.node);

		// Update all channel tasks priority
		channel->priority = prio;
		task_t* ch_task = GLIST_FIRST(&process->tasks, task_t, siblings);
		for(; (ch_task != NULL) && (ch_task->info.chid == channel->chid); ch_task = GLIST_NEXT(&ch_task->siblings, task_t, siblings))
		{
			task->info.active_prio = channel->priority;
			PriorityResolve(ch_task, prio);
		}
	}
	else // Reply blocked
	{
		// Just change receiver task priority
		//task_t* rcv_task;

		//PriorityResolve(rcv_task, prio);
	}
}

void ChannelResolvePriority(channel_t* channel, uint32_t priority)
{
	if((channel->flags & CHANNEL_FIXED_PRIORITY) || (priority <= channel->priority))
	{
		return;
	}

	// Propagate highest priority to all tasks in the channel process
//	SchedulerDisablePreemption();

	channel->priority = priority;

	process_t* process = ProcGetProcess(channel->pid);

	task_t* task;
	for(task = GLIST_FIRST(&process->tasks, task_t, siblings); task != NULL && task->info.chid == channel->chid; task = GLIST_NEXT(&task->siblings, task_t, siblings))
	{
		task->blocked.priority = task->info.priority = channel->priority;
		// TODO: Depending on the task state we might need to reinsert from other blocked lists (e.g. mutex)
		// Reinsert task in ready queue with new priority
		if(task->info.state == READY)
		{
			GlistRemoveSpecific(&task->blocked.node);
			SchedAddTask(task);
		}
	}

	// Done resume preemption
//	SchedulerEnablePreemption();
}

void ChannelRestorePriority(channel_t* channel, uint32_t priority)
{
	if((channel->flags & CHANNEL_FIXED_PRIORITY) || (priority < channel->priority))
	{
		return;
	}

	// Propagate highest priority to all tasks in the channel process
	//SchedulerDisablePreemption();

	channel->priority = 0;
	msg_t* m1 = GLIST_FIRST(&channel->send, msg_t, hdr.node);
	channel->priority = ((m1 != NULL) ? (m1->hdr.priority) : (channel->priority));

	msg_t* m2 = GLIST_FIRST(&channel->response, msg_t, hdr.node);
	channel->priority = ((m2 != NULL && channel->priority < m2->hdr.priority) ? (m2->hdr.priority) : (channel->priority));

	process_t* process = ProcGetProcess(channel->pid);

	task_t* task;
	for(task = GLIST_FIRST(&process->tasks, task_t, siblings); task != NULL; task = GLIST_NEXT(&task->siblings, task_t, siblings))
	{
		task->info.priority = ((channel->priority == 0) ? (task->blocked.priority) : ((uint16_t)channel->priority));
		// Reinsert task in ready queue with new priority
		if(task->info.state == READY)
		{
			GlistRemoveSpecific(&task->blocked.node);
			SchedAddTask(task);
		}
	}

	// Done resume preemption
	//SchedulerEnablePreemption();

	SchedYield();
}

void MsgCopy(const char* dst, const char* src, size_t size)
{
	(void)memcpy((char*)dst, src, size);
}

size_t MsgCopySize(size_t sbytes, uint32_t soff, size_t rbytes, uint32_t roff)
{
	return (((sbytes - soff) < (rbytes - roff)) ? (sbytes - soff) : (rbytes - roff));
}

uint32_t MsgDirectCopy(const char* send, size_t sbytes, uint32_t soff, const char* recv, size_t rbytes, uint32_t roff)
{
    size_t copySize = MsgCopySize(sbytes, soff, rbytes, roff);

    MsgCopy(&recv[roff], &send[soff], copySize);

    return copySize;
}

uint32_t MsgMapReceiverCopy(const char* send, uint32_t sendOff, const char* recv, uint32_t rcvOff, pgt_t rcvPgt, size_t size)
{
	// Apply offset to the original receiver address
	vaddr_t vcopyAddr = (vaddr_t)((uint32_t)recv + rcvOff);

	// Get physical address to where we want to copy data
	paddr_t p_rcv = MemoryVirtual2physical(rcvPgt, (vaddr_t)vcopyAddr);

	// Get the physical address aligned to a PAGE boundary
	paddr_t bp_rcv =  (paddr_t)(ALIGN_DOWN((uint32_t)p_rcv, PAGE_SIZE));

	// Get offset to beginning of the memory page
	uint32_t addrOff = ((uint32_t)p_rcv) - ((uint32_t)bp_rcv);

	size_t mapped = 0;
	size_t mapSize = (((PAGE_SIZE - addrOff) > size) ? (size) : (PAGE_SIZE - addrOff));

	while(mapped < size)
	{
		// Since there is no guaranty that be receiver buffer is a block of continuous physical memory we have to copy in Page size limits
		char* k_rcv = (char*)VirtualSpaceMmap(bp_rcv, PAGE_SIZE);

		MsgCopy(&k_rcv[addrOff], &send[mapped + sendOff], mapSize);

		// Unmap
		VirtualSpaceUnmmap((vaddr_t)k_rcv);

		mapped += mapSize;
		addrOff = 0;
		vcopyAddr = (vaddr_t)((uint32_t)vcopyAddr + mapSize);
		mapSize = (((mapped + PAGE_SIZE) > size) ? (size - mapped) : (PAGE_SIZE));
		bp_rcv = MemoryVirtual2physical(rcvPgt, (vaddr_t)vcopyAddr);
	}

	return size;
}

uint32_t MsgMapSenderCopy(const char* send, uint32_t sendOff, const char* recv, uint32_t rcvOff, pgt_t sendPgt, size_t size)
{
	// Apply offset to the original receiver address
	vaddr_t vcopyAddr = (vaddr_t)((uint32_t)send + sendOff);

	// Get physical address to where we want to copy data
	paddr_t p_send = MemoryVirtual2physical(sendPgt, (vaddr_t)vcopyAddr);

	// Get the physical address aligned to a PAGE boundary
	paddr_t bp_send =  (paddr_t)(ALIGN_DOWN((uint32_t)p_send, PAGE_SIZE));

	// Get offset to beginning of the memory page
	uint32_t addrOff = ((uint32_t)p_send) - ((uint32_t)bp_send);

	size_t mapped = 0;
	size_t mapSize = (((PAGE_SIZE - addrOff) > size) ? (size) : (PAGE_SIZE - addrOff));

	while(mapped < size)
	{
		// Since there is no guaranty that be receiver buffer is a block of continuous physical memory we have to copy in Page size limits
		char* k_send = (char*)VirtualSpaceMmap(bp_send, PAGE_SIZE);

		MsgCopy(&recv[mapped + rcvOff], &k_send[addrOff], mapSize);

		// Unmap
		VirtualSpaceUnmmap((vaddr_t)k_send);

		mapped += mapSize;
		addrOff = 0;
		vcopyAddr = (vaddr_t)((uint32_t)vcopyAddr + mapSize);
		mapSize = (((mapped + PAGE_SIZE) > size) ? (size - mapped) : (PAGE_SIZE));
		bp_send = MemoryVirtual2physical(sendPgt, (vaddr_t)vcopyAddr);
	}

	return size;
}

uint32_t MsgCopyFromSender(send_t* msg, char* buffer, size_t size, uint32_t readOff)
{
	if((msg->sbytes == 0) || (size == 0))
	{
		return 0;
	}

	if(SchedGetRunningProcess()->pid == MSGPID(msg))
	{
		msg->readOff = MsgDirectCopy(msg->smsg,  msg->sbytes, readOff, buffer, size, 0);
	}
	else
	{
		msg->readOff = MsgMapSenderCopy(msg->smsg, readOff, buffer, 0, ProcGetProcess(MSGPID(msg))->Memory.pgt, MsgCopySize(msg->sbytes, 0, size, 0));
	}

	return msg->readOff;
}

uint32_t MsgCopyToSender(send_t* msg, char* buffer, size_t size, uint32_t writeOff)
{
	if((msg->rbytes == 0) || (size == 0))
	{
		return 0;
	}

	if(SchedGetRunningProcess()->pid == MSGPID(msg))
	{
		msg->writeOff = MsgDirectCopy(buffer, size, 0, msg->rmsg, msg->rbytes, writeOff);
	}
	else
	{
		msg->writeOff = MsgMapReceiverCopy(buffer, 0, msg->rmsg, writeOff, ProcGetProcess(MSGPID(msg))->Memory.pgt, MsgCopySize(size, 0, msg->rbytes, writeOff));
	}

	return msg->writeOff;
}

void MsgSetResponseHeader(io_hdr_t* hdr, int32_t type, int32_t code, size_t rbytes, size_t sbytes)
{
	hdr->type   = type;
	hdr->code   = code;
	hdr->rbytes = rbytes;
	hdr->sbytes = sbytes;
}

taskSubState_t MsgState(send_t* msg)
{
	return ProcessGetTask(ProcGetProcess(MSGPID(msg)), msg->tid)->info.subState;
}

task_t* MsgOwner(send_t* send)
{
	return ProcessGetTask(ProcGetProcess(MSGPID(send)), send->tid);
}


/* Private functions -------------------------------------- */

/**
 * ChannelCreate Implementation (See header file for description)
*/
int32_t ChannelCreate(uint32_t flags)
{
	// Check if used flags are valid
	if(!CHANNEL_FLAGS_VALID(flags))
	{
		return INVALID_CHID;
	}

	process_t* process = SchedGetRunningProcess();

	channel_t* channel = (channel_t*)kalloc(sizeof(channel_t));

	if(channel == NULL)
	{
		return INVALID_CHID;
	}

	if(VectorInit(&channel->connections, VECTOR_CONNECTIONS_SIZE) != E_OK)
	{
		kfree(channel);
		return INVALID_CHID;
	}

	if(VectorInit(&channel->messages, VECTOR_MESSAGES_SIZE) != E_OK)
	{
		VectorFree(&channel->connections);
		kfree(channel);
		return INVALID_CHID;
	}

	// Receive list does not require any sorting
	GlistInitialize(&channel->receive, GFifo);

	// Send list is sorted by priority and is searchable using scoid and pid
	GlistInitialize(&channel->send, GList);
	GlistSetSort(&channel->send, MsgListSort);
	GlistSetCmp(&channel->send, MsgListMatchScoid);
	// Notifications list is sorted by priority and is searchable using scoid and pid
	GlistInitialize(&channel->notify, GList);
	GlistSetSort(&channel->notify, MsgListSort);
	GlistSetCmp(&channel->notify, MsgListMatchScoid);

	// Response list does not require any sorting and is searchable using scoid and pid
	GlistInitialize(&channel->response, GList);
	GlistSetSort(&channel->response, MsgListSort);
	GlistSetCmp(&channel->response, MsgListMatchScoid);

	// Set channel flags
	channel->flags = flags;

	// Register channel in process and get a chid
	channel->chid = VectorInsert(&process->channels, channel);
	channel->pid = process->pid;

	// By default a channel does not use a server
	channel->server = NULL;

	KlockInit(&channel->lock);

	// Make channel available
	channel->flags |= CHANNEL_ALIVE;

	// By default no priority is assigned to the channel
	channel->priority = 0;

	return channel->chid;
}

/**
 * ConnectAttach Implementation (See header file for description)
*/
int32_t ConnectAttach(pid_t pid, int32_t chid, uint32_t index, uint32_t flags)
{
	// Check if used flags are valid
	if(!CHANNEL_FLAGS_VALID(flags))
	{
		return INVALID_COID;
	}

	process_t* process = SchedGetRunningProcess();

	// Get channel process owner
	process_t* channelProc = ProcGetProcess(pid);
	// Get channel
	channel_t* channel = VectorPeek(&channelProc->channels, (uint32_t)chid);
	// Check if the channel is still alive
	if((channel ==  NULL) || !(channel->flags & CHANNEL_ALIVE))
	{
		return INVALID_COID;
	}

	// Check if we can share an existing connection (flag CONNECTION_NOT_SHARE)
	if(!(flags & CONNECTION_NOT_SHARED))
	{
		// All shared connections must have the reference count updated
		uint32_t count = VectorUsage(&process->connections) - 1;
		const uint32_t size = VectorSize(&process->connections);
		uint32_t index = 1;
		for(; (count > 0) && (index < size); index++, count--)
		{
			clink_t* link = (clink_t*)VectorPeek(&process->connections, index);
			if(link != NULL && link->connection->channel == channel)
			{
				link->refs++;
				return link->coid;
			}
		}
		// No previous connection available create a new one
	}

	// Connect Attach always create a new connection (connection + link)

	connection_t* connection = (connection_t*)kalloc(sizeof(connection_t));
	if(connection == NULL)
	{
		return INVALID_COID;
	}

	clink_t* link = (clink_t*)kalloc(sizeof(clink_t));
	if(link == NULL)
	{
		kfree(connection);
		return INVALID_COID;
	}

	// Initialize connection
	connection->channel = channel;
	connection->flags = flags;
	connection->shared = NULL;
	GlistInitialize(&connection->clinks, GFifo);

	// Create link to the new created connection
	link->pid = process->pid;
	link->connection = connection;
	link->flags = 0;
	link->refs = 1;
	link->privMap = NULL;
	GlistInsertObject(&connection->clinks, &link->node);

	// Set coid to be used by all links to this connection
	link->coid = VectorInsertAt(&process->connections, link, index);
	// Register connection in the channel
	connection->scoid = VectorInsert(&channel->connections, connection);

	// If channel as flag CHANNEL_SCOID_ATTACH_NOTIFY set we need to send a pulse to the channel
	if(channel->flags & CHANNEL_SCOID_ATTACH_NOTIFY)
	{
		ker_MsgNotify(connection, SchedGetRunningTask()->info.priority, _NOTIFY_SCOID_ATTACH_, link->pid);
	}

	return link->coid;
}

/**
 * ker_ChannelDestroy Implementation (See header file for description)
*/
int32_t ker_ChannelDestroy(process_t* process, channel_t* channel)
{
	// Signal that this channel is no longer alive
	channel->flags &= ~CHANNEL_ALIVE;

	// At this point all communications that where running will fail

	// If this channel is registered in the system path remove it
	if(channel->server != NULL)
	{
		ker_ServerTerminate(process, channel->chid);
	}

	// Close all connections owned by this channel
	uint32_t count = VectorUsage(&channel->connections);
	const uint32_t size = VectorSize(&channel->connections);
	uint32_t index = 0;
	for(; (count > 0) && (index < size); index++)
	{
		connection_t* connection = (connection_t*)VectorRemove(&channel->connections, index);
		if(connection == NULL)
		{
			continue;
		}

		// Mark connection as invalid
		connection->flags |= CONNECTION_INVALID;

		// Set all links to this connection as invalid
		while(!GLIST_EMPTY(&connection->clinks))
		{

			clink_t* link = GLISTNODE2TYPE(GlistRemoveFirst(&connection->clinks), clink_t, node);
			//link->flags |= CONNECTION_INVALID
			link->connection = NULL;
			link->privMap->shared = NULL;
			// Remove mapping from client, no longer valid
			//TODO: ProcessCleanSharedRef(ProcGetProcess(link->pid), link->privMap);
		}

		if(connection->shared != NULL)
		{
			// Decrement references to the shared memory object
			connection->shared->obj->refs -= 1;
			// Release shared reference
			kfree(connection->shared);
		}

		kfree(connection);

		count--;
	}

	// Remove all messages and pulses
	MsgsFlush(&channel->send);
	MsgsFlush(&channel->notify);
	MsgsFlush(&channel->response);
	MsgsReceiverFlush(&channel->receive);
	VectorFree(&channel->messages);

	// Remove channel from process
	VectorRemove(&process->channels, (uint32_t)channel->chid);

	kfree(channel);

	return E_OK;
}

/**
 * ChannelDestroy Implementation (See header file for description)
*/
int32_t ChannelDestroy(int32_t chid)
{
	// Just a entry point for the system call

	// Get running process
	process_t* process = SchedGetRunningProcess();

	// Get channel
	channel_t* channel = VectorPeek(&process->channels, (uint32_t)chid);

	return ker_ChannelDestroy(process, channel);
}

/**
 * ker_ConnectDetach Implementation (See header file for description)
*/
int32_t ker_ConnectDetach(process_t* process, clink_t* link, bool_t force)
{
	// Only terminate the connection/link if there is no more references to it
	// or if caller forces the closing
	if((--link->refs > 0) && (force != TRUE))
	{
		return E_BUSY;
	}

	// Mark link as invalid
	link->flags |= CLINK_DEAD;

	// Get connection
	connection_t* connection = link->connection;

	// Get channel
	channel_t* channel = connection->channel;

	// Remove link from connection
	GlistRemoveSpecific(&link->node);

	// Handle the shared reference
	if(link->privMap != NULL)
	{
		// Decrement references to the shared memory object
		link->privMap->shared->refs -= 1;
		// Process API will handle all deallocations
		ProcessCleanSharedRef(process, link->privMap);
		// Just in case clean reference
		link->privMap = NULL;
	}

	// Remove messages and pulses related to this link from the channel
	MsgsFlushByScoid(&channel->send, connection->scoid, process);
	MsgsFlushByScoid(&channel->notify, connection->scoid, process);
	MsgsFlushByScoid(&channel->response, connection->scoid, process);

	// Save coid to be later used
	int32_t coid = link->coid;

	// If requested notify coid death to channel
	if((channel->flags & CHANNEL_COID_DEATH_NOTIFY) && (channel->flags & CHANNEL_ALIVE))
	{
		ker_MsgNotify(connection, SchedGetRunningTask()->info.priority, _NOTIFY_COID_DEAD_, coid);
	}

	// If connection has no more links terminate it
	if(GLIST_EMPTY(&connection->clinks))
	{
		if(connection->shared != NULL)
		{
			// Decrement references to the shared memory object
			connection->shared->obj->refs -= 1;
			// Release shared reference
			kfree(connection->shared);
		}

		// Free scoid
		VectorRemove(&channel->connections, (uint32_t)connection->scoid);

		// Do we have to notify the channel?
		if((channel->flags & CHANNEL_SCOID_DETACH_NOTIFY) && (channel->flags & CHANNEL_ALIVE))
		{
			ker_MsgNotify(connection, SchedGetRunningTask()->info.priority, _NOTIFY_SCOID_DETACH_, link->pid);
		}

		kfree(connection);
	}

	// At this point it is safe to unlink the connection from the process
	VectorRemove(&process->connections, (uint32_t)coid);

	// Free allocated memory
	kfree(link);

	return E_OK;
}

/**
 * ConnectDetach Implementation (See header file for description)
*/
int32_t ConnectDetach(int32_t coid)
{
	// Get running process
	process_t* process = SchedGetRunningProcess();

	// Get connection/link
	clink_t* link = (clink_t*)VectorPeek(&process->connections, (uint32_t)coid);

	// Check if we got a valid link
	if(link == NULL)
	{
		return E_INVAL;
	}

	return ker_ConnectDetach(process, link, FALSE);
}

/**
 * MsgSend Implementation (See header file for description)
*/
int32_t MsgSend(int32_t coid, const io_hdr_t* hdr, const char* smsg, const char* rmsg, uint32_t* offset)
{
	// Check if we are sending a message to System
	if(coid == 0)
	{
		return SystemReceive(hdr, smsg, rmsg, offset);
	}

	// Get running process
	process_t* process = SchedGetRunningProcess();

	// Get running task
	task_t* task = SchedGetRunningTask();

	// Get connection link
	clink_t* link = VectorPeek(&process->connections, coid);

	// Check if connection link is valid
	if((link == NULL) || (link->flags & CLINK_DEAD) || link->connection == NULL)
	{
		return E_INVAL;
	}

	// Get channel
	channel_t* channel = link->connection->channel;

	if((channel == NULL) || !(channel->flags & CHANNEL_ALIVE))
	{
		return IPC_CHANNEL_DEAD;
	}

	// Get a send_t structure
	send_t* msg = (send_t*)kalloc(sizeof(send_t));
	int32_t rcvid = VectorInsert(&channel->messages, msg);

	// Initialize message send structure
	msg->hdr.rcvid = RCVID(channel->chid, rcvid);
	msg->hdr.priority = task->info.priority;
	msg->hdr.pid = process->pid;
	msg->hdr.coid = coid;
	msg->hdr.scoid = link->connection->scoid;
	msg->tid = task->info.tid;
	msg->type = hdr->type;
	msg->code = hdr->code;
	msg->smsg = smsg;
	msg->sbytes = hdr->sbytes;
	msg->rmsg = rmsg;
	msg->rbytes = hdr->rbytes;
    // Read Helper
	msg->readOff = 0;
    // Return
	msg->writeOff = 0;
	msg->ret = IPC_ERROR;

	task->blocked.data = (void*)msg;

    uint32_t status;
    Klock(&channel->lock, &status);

    task_t* receiver = GLISTNODE2TYPE(GlistRemoveFirst(&channel->receive), task_t, blocked.node);

    if(receiver != NULL)
    {
        // Go strait to reply blocked
        GlistInsertObject(&channel->response, &msg->hdr.node);
        Kunlock(&channel->lock, &status);
        // Set up receiver task to attend sent message
        receiver->info.priority = task->info.priority;
        receiver->blocked.data = (void*)msg;
        receiver->blocked.ret = E_OK;
        // Unblock receiver and suspend/block sender
        SchedLock(NULL);
        SchedAddTask(receiver);
        SchedStopRunningTask(BLOCKED, IPC_REPLY);
    }
    else
    {
        // Add task to sender blocked list
        GlistInsertObject(&channel->send, &msg->hdr.node);
        // Resolve priority inversion
        ChannelResolvePriority(channel, task->info.priority);
        // Before release the channel lock get the scheduler lock to safely suspend running task
        SchedLock(NULL);
        Kunlock(&channel->lock, NULL);
        SchedStopRunningTask(BLOCKED, IPC_SEND);
    }
    // Both use cases will leave interrupts disabled
    critical_unlock(&status);

    int32_t ret = msg->ret;

	// Get offset/response size if sender requests it
	if(offset != NULL)
	{
		*offset = msg->writeOff;
	}

	// If channel is still alive remove message from the channel vector
	if(ret != IPC_CHANNEL_DEAD)
	{
		VectorRemove(&channel->messages, MSGID(msg->hdr.rcvid));
	}

	task->blocked.data = NULL;

	// Free the send message structure
	kfree(msg);

	return ret;
}

/**
 * MsgReceive Implementation (See header file for description)
*/
int32_t MsgReceive(int32_t chid, io_hdr_t* hdr, const char* msg, size_t size, uint32_t* offset, msg_info_t* info)
{
	// hdr cannot be null
	if(hdr == NULL)
	{
		return INVALID_RCVID;
	}

	// Get running process
	process_t* process = SchedGetRunningProcess();

	// Get running task
	task_t* task = SchedGetRunningTask();

	// Get Channel
	channel_t* channel = (channel_t *)VectorPeek(&process->channels, chid);

	// Check if channel is still alive
	if((channel == NULL) || !(channel->flags & CHANNEL_ALIVE))
	{
		return IPC_CHANNEL_DEAD;
	}

	task->info.chid = chid;

    uint32_t status;
    Klock(&channel->lock, &status);

    msg_t* send = MsgGet(channel);

    if(send != NULL)
    {
        if(send->hdr.rcvid != NOTIFY_RCVID)
        {
            // Add task to reply blocked list
            MsgOwner(&send->msg)->info.subState = IPC_REPLY;
            GlistInsertObject(&channel->response, &send->hdr.node);
        }
        Kunlock(&channel->lock, &status);
    }
    else
    {
        task->blocked.data = NULL;
        task->blocked.ret = E_ERROR;
        GlistInsertObject(&channel->receive, &task->blocked.node);
        // Suspend receiver
        SchedLock(NULL);
        Kunlock(&channel->lock, NULL);
        int32_t ret = SchedStopRunningTask(BLOCKED, IPC_RECEIVE);
        // Interrupts are still disabled
        critical_unlock(&status);
        // Check if there was no errors
        if(ret != E_OK)
        {
            return INVALID_RCVID;
        }
        // Get message
        send = (msg_t*)task->blocked.data;
    }

	int32_t rcvid = send->hdr.rcvid;

	// Did we receive a notification
	if(rcvid == NOTIFY_RCVID)
	{
		notify_t* notify = &send->notify;

		MsgSetResponseHeader(hdr, notify->type, notify->data, 0, (size_t)CONNECTION_USCOID(channel->chid, notify->hdr.scoid));

		return rcvid;
	}

	// We received a message
	// Copy message from sender virtual space
	send->msg.readOff = MsgCopyFromSender(&send->msg, (char*)msg, size, 0);
	// Copy header information
	MsgSetResponseHeader(hdr, send->msg.type, send->msg.code, send->msg.rbytes, send->msg.sbytes);
	// Get message info if requested
	if(info != NULL)
	{
		// Fill info
		info->pid = MSGPID((&send->msg));
		info->tid = send->msg.tid;
		info->chid = channel->chid;
		info->coid = send->hdr.coid;
		// NOTE: User space scoid is a combination of chid and scoid
		info->scoid = CONNECTION_USCOID(channel->chid, send->hdr.scoid);
	}

	// Get offset/send size if receiver requests it
	if(offset != NULL)
	{
		*offset = send->msg.readOff;
	}

	// Return message id
	return rcvid;
}

/**
 * MsgRespond Implementation (See header file for description)
*/
int32_t MsgRespond(int32_t rcvid, int32_t status, const char *msg, size_t size)
{
	// Get running process
	process_t* process = SchedGetRunningProcess();

	// Get Channel
	channel_t* channel = (channel_t*)VectorPeek(&process->channels, MSGCHID(rcvid));

	// Check if channel is still alive
	if((channel == NULL) || !(channel->flags & CHANNEL_ALIVE))
	{
		// Channel is dead restore receiver priority
		SchedGetRunningTask()->info.priority = SchedGetRunningTask()->blocked.priority;

		// Trigger scheduler
		SchedYield();

		return E_ERROR;
	}

	// Get message
	send_t* send = (send_t*)VectorPeek(&channel->messages, MSGID(rcvid));

	// Verify if message is still valid
	if(send == NULL || send->hdr.rcvid != rcvid)
	{
		return E_ERROR;
	}

    uint32_t stat;
    Klock(&channel->lock, &stat);
    // Remove sender from reply blocked list
    int32_t ret = GlistRemoveSpecific(&send->hdr.node);
    // Release channel
    Kunlock(&channel->lock, &stat);
    // Did we get the message
    if(ret != E_OK)
    {
    	return E_ERROR;
    }

	// Check if sender task is still alive
	if(send->ret == IPC_TASK_DEAD)
	{
		// Remove from messages vector and free allocated memory
		VectorRemove(&channel->messages, MSGID(rcvid));

		uint32_t priority = send->hdr.priority;

		kfree(send);

		ChannelRestorePriority(channel, priority);

		return E_FAULT;
	}

	// Copy response message from receiver to sender virtual space
	send->writeOff = MsgCopyToSender(send, (char*)msg, size, 0);

	// Resume sender
	send->ret = status;
	SchedAddTask(MsgOwner(send));

	ChannelRestorePriority(channel, send->hdr.priority);

	return E_OK;
}

/**
 * MsgWrite Implementation (See header file for description)
*/
int32_t MsgWrite(int32_t rcvid, const void *msg, size_t size, int32_t offset)
{
	// Get running process
	process_t* process = SchedGetRunningProcess();

	// Get Channel
	channel_t* channel = (channel_t*)VectorPeek(&process->channels, MSGCHID(rcvid));

	// Check if channel is still alive
	if((channel == NULL) || !(channel->flags & CHANNEL_ALIVE))
	{
		return IPC_CHANNEL_DEAD;
	}

	// Get message
	send_t* send = (send_t*)VectorPeek(&channel->messages, MSGID(rcvid));

	// Verify if message is still valid
	if(send == NULL || send->hdr.rcvid != rcvid)
	{
		return IPC_ERROR;
	}

	// Check if sender task is still alive
	if(send->ret == IPC_TASK_DEAD)
	{
		// Remove from messages vector and free allocated memory
		VectorRemove(&channel->messages, MSGID(rcvid));
		GlistRemoveSpecific(&send->hdr.node);

		uint32_t priority = send->hdr.priority;

		kfree(send);

		ChannelRestorePriority(channel, priority);

		return IPC_TASK_DEAD;
	}

	// Ensure task is in reply blocked state
	if((MsgState(send) != IPC_REPLY))
	{
		return IPC_ERROR;
	}

	// Copy message from receiver to sender virtual space at specified offset
	send->writeOff = MsgCopyToSender(send, (char*)msg, size, offset);

	return send->writeOff;
}

/**
 * MsgRead Implementation (See header file for description)
*/
int32_t MsgRead(int32_t rcvid, const void *msg, size_t size, int32_t offset)
{
	// Get running process
	process_t* process = SchedGetRunningProcess();

	// Get Channel
	channel_t* channel = (channel_t*)VectorPeek(&process->channels, MSGCHID(rcvid));

	// Check if channel is still alive
	if((channel == NULL) || !(channel->flags & CHANNEL_ALIVE))
	{
		return IPC_CHANNEL_DEAD;
	}

	// Get message
	send_t* send = (send_t*)VectorPeek(&channel->messages, MSGID(rcvid));

	// Verify if message is still valid
	if(send == NULL || send->hdr.rcvid != rcvid)
	{
		return IPC_ERROR;
	}

	// Check if sender task is still alive
	if(send->ret == IPC_TASK_DEAD)
	{
		// Remove from messages vector and free allocated memory
		VectorRemove(&channel->messages, MSGID(rcvid));
		GlistRemoveSpecific(&send->hdr.node);

		uint32_t priority = send->hdr.priority;

		kfree(send);

		ChannelRestorePriority(channel, priority);

		return IPC_TASK_DEAD;
	}

	// Ensure task is in reply blocked state
	if((MsgState(send) != IPC_REPLY))
	{
		return IPC_ERROR;
	}

	// Read message from sender at specified offset
	return MsgCopyFromSender(send, (char*)msg, size, offset);
}

/**
 * ker_MsgNotify Implementation (See header file for description)
*/
int32_t ker_MsgNotify(connection_t* connection, int32_t priority, int32_t type, int32_t value)
{
	// Check if condition to send notification are meet
	if((connection == NULL) || ((connection->flags & CONNECTION_INVALID) & !(type == _NOTIFY_SCOID_DETACH_)))
	{
		return E_INVAL;
	}

	// Get channel
	channel_t* channel = connection->channel;

	notify_t* notify = (notify_t*)kalloc(sizeof(notify_t));
	if(notify == NULL)
	{
		return E_ERROR;
	}
	notify->hdr.rcvid = 0;
	notify->hdr.priority = (uint32_t)priority;
	notify->hdr.scoid = connection->scoid;
	notify->hdr.pid = 0;
	notify->hdr.coid = 0;
	notify->type = type;
	notify->data = value;

    uint32_t status;
    Klock(&channel->lock, &status);

    task_t* receiver = GLISTNODE2TYPE(GlistRemoveFirst(&channel->receive), task_t, blocked.node);

    if(receiver != NULL)
    {
        Kunlock(&channel->lock, &status);
        // Set up receiver task to attend sent message
        receiver->info.priority = priority;
        receiver->blocked.data = (void*)notify;
        receiver->blocked.ret = E_OK;
        // Unblock receiver
        SchedAddTask(receiver);
    }
    else
    {
        // Add notification to notification pending list
    	GlistInsertObject(&channel->notify, &notify->hdr.node);
        // Resolve priority inversion
        ChannelResolvePriority(channel, priority);
        // Before release the channel lock get the scheduler lock to safely suspend running task
        Kunlock(&channel->lock, &status);
    }

	return E_OK;
}

/**
 * MsgNotify Implementation (See header file for description)
*/
int32_t MsgNotify(int32_t coid, int32_t priority, int32_t type, int32_t value)
{
	// Get running process
	process_t* process = SchedGetRunningProcess();

	// Get connection link
	clink_t* link = VectorPeek(&process->connections, coid);

	// Check if connection link is still valid
	if((link == NULL) || (link->flags & CLINK_DEAD) || link->connection == NULL)
	{
		return E_INVAL;
	}

	return ker_MsgNotify(link->connection, priority, type, value);
}

/**
 * IpcSendCancel Implementation (See header file for description)
*/
void IpcSendCancel(send_t* msg)
{
	// Signal death of task. Clean up will be performed by a receiver task
	msg->ret = IPC_TASK_DEAD;
}

/**
 * IpcReceiveCancel Implementation (See header file for description)
*/
void IpcReceiveCancel(task_t* task)
{
	// Get Channel
	channel_t* channel = (channel_t *)VectorPeek(&task->info.parent->channels, task->info.chid);
	// Lock channel to remove the task from the receiver list
	uint32_t status;
	Klock(&channel->lock, &status);
	// Remove from sender list
	GlistRemoveSpecific(&task->blocked.node);
	// Release channel
	Kunlock(&channel->lock, &status);
}

/**
 * IpcReplyCancel Implementation (See header file for description)
*/
void IpcReplyCancel(send_t* msg)
{
	// Signal death of task. Clean up will be performed by a receiver task
	msg->ret = IPC_TASK_DEAD;
}
