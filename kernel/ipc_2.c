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
	task_t* currentMsg = GLISTNODE2TYPE(current, task_t, node);
	task_t* msg = GLISTNODE2TYPE(newMsg, task_t, node);

	// Ensure that the older message with same priority is handled first
	if(currentMsg->active_prio == msg->active_prio)
	{
		return 1;
	}

    return (int32_t)currentMsg->active_prio - (int32_t)msg->active_prio;
}

int32_t NotifyListSort(glistNode_t* current, glistNode_t* newMsg)
{
	notify_t* currentNotify = GLISTNODE2TYPE(current, notify_t, node);
	notify_t* notify = GLISTNODE2TYPE(newMsg, notify_t, node);

	// Ensure that the older message with same priority is handled first
	if(currentNotify->priority == notify->priority)
	{
		return 1;
	}

    return (int32_t)currentNotify->priority - (int32_t)notify->priority;
}

int32_t MsgListMatchScoid(glistNode_t* current, void* cmp)
{
	task_t* msg = GLISTNODE2TYPE(current, task_t, node);

	msgCmp_t* msgCmp = (msgCmp_t*)cmp;

	return ((msg->data.msg.scoid != msgCmp->scoid) | ((msg->tid >> 16) != msgCmp->pid));
}

int32_t MsgGet(channel_t* channel, task_t* rcv)
{
	task_t* send = GLIST_FIRST(&channel->send, task_t, node);
	notify_t* notify = GLIST_FIRST(&channel->notify, notify_t, node);

	if(send != NULL && ((notify == NULL) || (send->active_prio > notify->priority)))
	{
		rcv->client = send;
		GlistRemoveSpecific(&send->node);
		return send->data.msg.rcvid;
	}

	if(notify != NULL && ((send == NULL) || (send->active_prio <= notify->priority)))
	{
		rcv->data.notify.notification = notify;
		GlistRemoveSpecific(&notify->node);
		return NOTIFY_RCVID;
	}

	return INVALID_RCVID;
}

void MsgsFlushByScoid(glist_t* list, int32_t scoid, process_t* process)
{
	msgCmp_t cmp = { scoid, process->pid };

	while(TRUE)
	{
		task_t* msg = GLISTNODE2TYPE(GlistRemoveObject(list, (void*)&cmp), task_t, node);

		if(msg == NULL)
		{
			break;
		}

//		if(msg->hdr.rcvid == 0)
//		{
//			kfree(&msg->notify);
//		}
//		else
//		{
			msg->ret = IPC_CHONNECTION_DEAD;
			SchedAddTask(msg);
//		}
	}
}

void MsgsFlush(glist_t* list)
{
	while(!GLIST_EMPTY(list))
	{
		task_t* msg = GLISTNODE2TYPE(GlistRemoveFirst(list), task_t, node);

//		if(msg->hdr.rcvid == 0)
//		{
//			kfree(&msg->notify);
//		}
//		else
//		{
			msg->ret = IPC_CHANNEL_DEAD;
			SchedAddTask(msg);
//		}
	}
}

void MsgsReceiverFlush(glist_t *list)
{
	while(!GLIST_EMPTY(list))
	{
		task_t* task = GLISTNODE2TYPE(GlistRemoveFirst(list), task_t, node);

		task->ret = IPC_CHANNEL_DEAD;

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

	if(task->subState == IPC_SEND)
	{
		// Reinsert task
		GlistRemoveSpecific(&task->node);
		GlistInsertObject(&channel->send, &task->node);

		// Update all channel tasks priority
		channel->priority = prio;
		task_t* ch_task = GLIST_FIRST(&process->tasks, task_t, siblings);
		for(; (ch_task != NULL) && (ch_task->chid == channel->chid); ch_task = GLIST_NEXT(&ch_task->siblings, task_t, siblings))
		{
			ch_task->active_prio = channel->priority;
			PriorityResolve(ch_task, prio);
		}
	}
	else // Reply blocked
	{
		// Just change receiver task priority
		PriorityResolve(task->data.msg.server, prio);
	}
}

void ChannelPriorityAdjust(task_t* task, uint16_t prio)
{
	channel_t* channel = (channel_t*)task->block_on;

	if((channel->flags & CHANNEL_FIXED_PRIORITY) || (prio <= channel->priority))
	{
		return;
	}

	uint32_t status;
	Klock(&channel->lock, &status);

	process_t* process = ProcGetProcess(channel->pid);

	if(task->subState == IPC_SEND)
	{
		// Reinsert task
		GlistRemoveSpecific(&task->node);
		GlistInsertObject(&channel->send, &task->node);

		// Update all channel tasks priority
		channel->priority = prio;
		task_t* ch_task = GLIST_FIRST(&process->tasks, task_t, siblings);
		for(; (ch_task != NULL) && (ch_task->chid == channel->chid); ch_task = GLIST_NEXT(&ch_task->siblings, task_t, siblings))
		{
			ch_task->active_prio = channel->priority;
			PriorityResolve(ch_task, prio);
		}
	}
	else // Reply blocked
	{
		// Just change receiver task priority
		PriorityResolve(task->data.msg.server, prio);
	}
}

/*

void ChannelRestorePriority(channel_t* channel, uint16_t prio)
{
	if((channel->flags & CHANNEL_FIXED_PRIORITY) || (prio < channel->priority))
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
		task->priority = ((channel->priority == 0) ? (task->blocked.priority) : ((uint16_t)channel->priority));
		// Reinsert task in ready queue with new priority
		if(task->state == READY)
		{
			GlistRemoveSpecific(&task->blocked.node);
			SchedAddTask(task);
		}
	}

	// Done resume preemption
	//SchedulerEnablePreemption();

	SchedYield();
}

*/

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

uint32_t MsgCopyFromSender(task_t* sender, char* buffer, size_t size, uint32_t readOff)
{
	if((sender->data.msg.sbytes == 0) || (size == 0))
	{
		return 0;
	}

	if(SchedGetRunningProcess() == sender->parent)
	{
		sender->data.msg.read_off = MsgDirectCopy(sender->data.msg.smsg,  sender->data.msg.sbytes, readOff, buffer, size, 0);
	}
	else
	{
		sender->data.msg.read_off = MsgMapSenderCopy(sender->data.msg.smsg, readOff, buffer, 0, sender->parent->Memory.pgt, MsgCopySize(sender->data.msg.sbytes, 0, size, 0));
	}

	return sender->data.msg.read_off;
}

uint32_t MsgCopyToSender(task_t* sender, char* buffer, size_t size, uint32_t writeOff)
{
	if((sender->data.msg.rbytes == 0) || (size == 0))
	{
		return 0;
	}

	if(SchedGetRunningProcess() == sender->parent)
	{
		sender->data.msg.write_off = MsgDirectCopy(buffer, size, 0, sender->data.msg.rmsg, sender->data.msg.rbytes, writeOff);
	}
	else
	{
		sender->data.msg.write_off = MsgMapReceiverCopy(buffer, 0, sender->data.msg.rmsg, writeOff, sender->parent->Memory.pgt, MsgCopySize(size, 0, sender->data.msg.rbytes, writeOff));
	}

	return sender->data.msg.write_off;
}

void MsgSetResponseHeader(io_hdr_t* hdr, int32_t type, int32_t code, size_t rbytes, size_t sbytes)
{
	hdr->type   = type;
	hdr->code   = code;
	hdr->rbytes = rbytes;
	hdr->sbytes = sbytes;
}

subState_t MsgState(task_t* sender)
{
	return sender->subState;
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

	channel_t* channel = (channel_t*)kmalloc(sizeof(channel_t));

	if(channel == NULL)
	{
		return INVALID_CHID;
	}

	if(VectorInit(&channel->connections, VECTOR_CONNECTIONS_SIZE) != E_OK)
	{
		kfree(channel, sizeof(channel_t));
		return INVALID_CHID;
	}

	if(VectorInit(&channel->messages, VECTOR_MESSAGES_SIZE) != E_OK)
	{
		VectorFree(&channel->connections);
		kfree(channel, sizeof(channel_t));
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
	GlistSetSort(&channel->notify, NotifyListSort);
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

	connection_t* connection = (connection_t*)kmalloc(sizeof(connection_t));
	if(connection == NULL)
	{
		return INVALID_COID;
	}

	clink_t* link = (clink_t*)kmalloc(sizeof(clink_t));
	if(link == NULL)
	{
		kfree(connection, sizeof(connection_t));
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
		ker_MsgNotify(connection, SchedGetRunningTask()->active_prio, _NOTIFY_SCOID_ATTACH_, link->pid);
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
			kfree(connection->shared, sizeof(sobj_t));
		}

		kfree(connection, sizeof(connection_t));

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

	kfree(channel, sizeof(channel_t));

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
		ker_MsgNotify(connection, SchedGetRunningTask()->active_prio, _NOTIFY_COID_DEAD_, coid);
	}

	// Remove link from connection
	GlistRemoveSpecific(&link->node);
	// If connection has no more links terminate it
	if(GLIST_EMPTY(&connection->clinks))
	{
		if(connection->shared != NULL)
		{
			// Decrement references to the shared memory object
			connection->shared->obj->refs -= 1;
			// Release shared reference
			kfree(connection->shared, sizeof(*connection->shared));
		}

		// Do we have to notify the channel?
		if((channel->flags & CHANNEL_SCOID_DETACH_NOTIFY) && (channel->flags & CHANNEL_ALIVE))
		{
			ker_MsgNotify(connection, SchedGetRunningTask()->active_prio, _NOTIFY_SCOID_DETACH_, link->pid);
		}

		// Free scoid
		VectorRemove(&channel->connections, (uint32_t)connection->scoid);

		kfree(connection, sizeof(*connection));
	}

	// At this point it is safe to unlink the connection from the process
	VectorRemove(&process->connections, (uint32_t)coid);

	// Free allocated memory
	kfree(link, sizeof(*link));

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
	int32_t rcvid = VectorInsert(&channel->messages, task);

	// Initialize message send structure
	task->data.msg.rcvid = RCVID(channel->chid, rcvid);
	task->data.msg.coid = coid;
	task->data.msg.scoid = link->connection->scoid;
	task->data.msg.type = hdr->type;
	task->data.msg.code = hdr->code;
	task->data.msg.smsg = smsg;
	task->data.msg.sbytes = hdr->sbytes;
	task->data.msg.rmsg = rmsg;
	task->data.msg.rbytes = hdr->rbytes;
    // Read Helper
	task->data.msg.read_off = 0;
    // Write Helper
	task->data.msg.write_off = 0;
	// Return
	task->ret = IPC_ERROR;

	int32_t ret;

    uint32_t status;
    Klock(&channel->lock, &status);

    task_t* receiver = GLISTNODE2TYPE(GlistRemoveFirst(&channel->receive), task_t, node);

    if(receiver != NULL)
    {
        // Go strait to reply blocked
        GlistInsertObject(&channel->response, &task->node);
        Kunlock(&channel->lock, &status);
        // Set up receiver task to attend sent message
        receiver->active_prio = task->active_prio;
        receiver->client = task;
        receiver->ret = task->data.msg.rcvid;
        task->data.msg.server = receiver;
        // Unblock receiver and suspend/block sender
        SchedLock(NULL);
        // TODO: Priority
        receiver->active_prio = task->active_prio;
        SchedAddTask(receiver);
        ret = SchedStopRunningTask(BLOCKED, IPC_REPLY);
    }
    else
    {
        // Add task to sender blocked list
        GlistInsertObject(&channel->send, &task->node);
        // Resolve priority inversion
        //TODO: ChannelResolvePriority(channel, task->active_prio);
        // Before release the channel lock get the scheduler lock to safely suspend running task
        SchedLock(NULL);
        Kunlock(&channel->lock, NULL);
        ret = SchedStopRunningTask(BLOCKED, IPC_SEND);
    }
    // Both use cases will leave interrupts disabled
    critical_unlock(&status);

	// Get offset/response size if sender requests it
	if(offset != NULL)
	{
		*offset = task->data.msg.write_off;
	}

	// If channel is still alive remove message from the channel vector
	if(ret != IPC_CHANNEL_DEAD)
	{
		VectorRemove(&channel->messages, MSGID(task->data.msg.rcvid));
	}

	// TODO: Resolve task priority

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

	task->chid = chid;

    uint32_t status;
    Klock(&channel->lock, &status);

    int32_t rcvid = MsgGet(channel, task);

    if(rcvid == INVALID_RCVID)
    {
    	task->ret = INVALID_RCVID;
        GlistInsertObject(&channel->receive, &task->node);
        // Suspend receiver
        SchedLock(NULL);
        Kunlock(&channel->lock, NULL);
        rcvid = SchedStopRunningTask(BLOCKED, IPC_RECEIVE);
        // Interrupts are still disabled
        critical_unlock(&status);
        // Check if there was no errors
        if(rcvid == INVALID_RCVID)
        {
            return INVALID_RCVID;
        }
    }
    else
    {
    	if(rcvid != NOTIFY_RCVID)
    	{
    		// Add sender task to reply blocked list
    		task->client->subState = IPC_REPLY;
    		GlistInsertObject(&channel->response, &task->client->node);
    		// TODO: Priority
    		task->active_prio = task->client->active_prio;
    	}
    	else
    	{
    		// TODO: Priority
    		task->active_prio = ((notify_t*)task->data.notify.notification)->priority;
    	}
    	Kunlock(&channel->lock, &status);
    }

    if(rcvid == NOTIFY_RCVID)
    {
    	if(task->data.notify.notification == NULL)
    	{
    		MsgSetResponseHeader(hdr, task->data.notify.type, task->data.notify.data, 0, (size_t)CONNECTION_USCOID(channel->chid, task->data.notify.scoid));
    	}
    	else
    	{
    		MsgSetResponseHeader(hdr, ((notify_t*)task->data.notify.notification)->type, ((notify_t*)task->data.notify.notification)->data, 0, (size_t)CONNECTION_USCOID(channel->chid, ((notify_t*)task->data.notify.notification)->scoid));
    		// Release memory used to pass the notification
    		kfree(task->data.notify.notification, sizeof(notify_t));
    		task->data.notify.notification = NULL;
    	}

    	return NOTIFY_RCVID;
    }

	// We received a message
    task_t* sender = task->client;
	// Copy message from sender virtual space
    sender->data.msg.read_off = MsgCopyFromSender(sender, (char*)msg, size, 0);
	// Copy header information
	MsgSetResponseHeader(hdr, sender->data.msg.type, sender->data.msg.code, sender->data.msg.rbytes, sender->data.msg.sbytes);
	// Get message info if requested
	if(info != NULL)
	{
		// Fill info
		info->pid = (sender->tid >> 16);
		info->tid = sender->tid;
		info->chid = channel->chid;
		info->coid = sender->data.msg.coid;
		// NOTE: User space scoid is a combination of chid and scoid
		info->scoid = CONNECTION_USCOID(channel->chid, sender->data.msg.scoid);
	}

	// Get offset/send size if receiver requests it
	if(offset != NULL)
	{
		*offset = sender->data.msg.read_off;
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

	// Get running task
	task_t* task = SchedGetRunningTask();

	// Get Channel
	channel_t* channel = (channel_t*)VectorPeek(&process->channels, MSGCHID(rcvid));

	// Check if channel is still alive
	if((channel == NULL) || !(channel->flags & CHANNEL_ALIVE))
	{
		// Channel is dead restore receiver priority
		task->client = NULL;
		task->chid = INVALID_CHID;
		//TODO: PriorityRestore(task, task->real_prio);
		task->active_prio = task->real_prio;

		// Trigger scheduler
		SchedYield();

		return E_ERROR;
	}

	// Check if sender is still waiting for the reply
	if(task->client == NULL)
	{
		VectorRemove(&channel->messages, MSGID(rcvid));
		task->chid = INVALID_CHID;
		//TODO: PriorityRestore(task, task->real_prio);
		task->active_prio = task->real_prio;
		SchedYield();

		return E_ERROR;
	}

	// Get message
	task_t* sender = task->client;

	// Copy response message from receiver to sender virtual space
    sender->data.msg.write_off = MsgCopyToSender(sender, (char*)msg, size, 0);

    // Remove sender from reply blocked list
    uint32_t stat;
    Klock(&channel->lock, &stat);
    // Remove sender from reply blocked list
    GlistRemoveSpecific(&sender->node);
    // Release channel
    Kunlock(&channel->lock, &stat);

    // Detach server task from client task
    task->client = NULL;
    task->chid = INVALID_CHID;

	// Resume sender
	sender->ret = status;
	SchedAddTask(sender);

	// TODO: ChannelRestorePriority(channel, send->hdr.priority);
	task->active_prio = task->real_prio;
	SchedYield();

	return E_OK;
}

/**
 * MsgWrite Implementation (See header file for description)
*/
int32_t MsgWrite(int32_t rcvid, const void *msg, size_t size, int32_t offset)
{
	// Get running process
	process_t* process = SchedGetRunningProcess();

	// Get running task
	task_t* task = SchedGetRunningTask();

	// Get Channel
	channel_t* channel = (channel_t*)VectorPeek(&process->channels, MSGCHID(rcvid));

	// Check if channel is still alive
	if((channel == NULL) || !(channel->flags & CHANNEL_ALIVE))
	{
		// Channel is dead restore receiver priority
		task->client = NULL;
		task->chid = INVALID_CHID;
		//TODO: PriorityRestore(task, task->real_prio);
		task->active_prio = task->real_prio;

		// Trigger scheduler
		SchedYield();

		return E_ERROR;
	}

	// Check if sender is still waiting for the reply
	if(task->client == NULL)
	{
		VectorRemove(&channel->messages, MSGID(rcvid));
		task->chid = INVALID_CHID;
		//TODO: PriorityRestore(task, task->real_prio);
		task->active_prio = task->real_prio;
		SchedYield();

		return E_ERROR;
	}

	// Get message
	task_t* sender = task->client;

	// Copy response message from receiver to sender virtual space
    sender->data.msg.write_off = MsgCopyToSender(sender, (char*)msg, size, offset);

	return sender->data.msg.write_off;
}

/**
 * MsgRead Implementation (See header file for description)
*/
int32_t MsgRead(int32_t rcvid, const void *msg, size_t size, int32_t offset)
{
	// Get running process
	process_t* process = SchedGetRunningProcess();

	// Get running task
	task_t* task = SchedGetRunningTask();

	// Get Channel
	channel_t* channel = (channel_t*)VectorPeek(&process->channels, MSGCHID(rcvid));

	// Check if channel is still alive
	if((channel == NULL) || !(channel->flags & CHANNEL_ALIVE))
	{
		// Channel is dead restore receiver priority
		task->client = NULL;
		task->chid = INVALID_CHID;
		//TODO: PriorityRestore(task, task->real_prio);
		task->active_prio = task->real_prio;

		// Trigger scheduler
		SchedYield();

		return E_ERROR;
	}

	// Check if sender is still waiting for the reply
	if(task->client == NULL)
	{
		VectorRemove(&channel->messages, MSGID(rcvid));
		task->chid = INVALID_CHID;
		//TODO: PriorityRestore(task, task->real_prio);
		task->active_prio = task->real_prio;
		SchedYield();

		return E_ERROR;
	}

	// Read message from sender at specified offset
	return MsgCopyFromSender(task->client, (char*)msg, size, offset);
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

	notify_t* notify = (notify_t*)kmalloc(sizeof(notify_t));
	if(notify == NULL)
	{
		return E_ERROR;
	}
	notify->priority = (uint16_t)((uint32_t)priority & 0xFFFF);
	notify->scoid = connection->scoid;
	notify->type = type;
	notify->data = value;

    uint32_t status;
    Klock(&channel->lock, &status);

    task_t* receiver = GLISTNODE2TYPE(GlistRemoveFirst(&channel->receive), task_t, node);

    if(receiver != NULL)
    {
        Kunlock(&channel->lock, &status);
        // Set up receiver task to attend sent message
        receiver->active_prio = priority;
        receiver->data.notify.data = value;
        receiver->data.notify.scoid = connection->scoid;;
        receiver->data.notify.type = type;
        receiver->data.notify.notification = NULL;
        receiver->ret = NOTIFY_RCVID;
        // For now
        kfree(notify, sizeof(notify_t));
        // Unblock receiver
        SchedAddTask(receiver);
    }
    else
    {
        // Add notification to notification pending list
    	GlistInsertObject(&channel->notify, &notify->node);
        // TODO: Resolve priority inversion
        // ChannelResolvePriority(channel, priority);
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
void IpcSendCancel(task_t* task)
{
	// Get connection link and channel
	clink_t* link = VectorPeek(&task->parent->connections, task->data.msg.coid);;
	channel_t* channel = link->connection->channel;

	uint32_t status;
	Klock(&channel->lock, &status);

	GlistRemoveSpecific(&task->node);

	Kunlock(&channel->lock, &status);
}

/**
 * IpcReceiveCancel Implementation (See header file for description)
*/
void IpcReceiveCancel(task_t* task)
{
	// Get Channel
	channel_t* channel = (channel_t *)VectorPeek(&task->parent->channels, task->chid);
	// Lock channel to remove the task from the receiver list
	uint32_t status;
	Klock(&channel->lock, &status);
	// Remove from sender list
	GlistRemoveSpecific(&task->node);
	// Release channel
	Kunlock(&channel->lock, &status);
}

/**
 * IpcReplyCancel Implementation (See header file for description)
*/
void IpcReplyCancel(task_t* task)
{
	// Get connection link and channel
	clink_t* link = VectorPeek(&task->parent->connections, task->data.msg.coid);;
	channel_t* channel = link->connection->channel;

	uint32_t status;
	Klock(&channel->lock, &status);

	if(task->data.msg.server)task->data.msg.server->client = NULL;
	GlistRemoveSpecific(&task->node);

	Kunlock(&channel->lock, &status);
}
