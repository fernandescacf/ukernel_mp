/**
 * @file        ipc.h
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        05 September, 2020
 * @brief       Interprocess Communication Definition Header File
*/

#ifndef _IPC_H_
#define _IPC_H_


/* Includes ----------------------------------------------- */
#include <types.h>
#include <glist.h>
#include <system.h>
#include <mmap.h>
#include <klock.h>


/* Exported types ----------------------------------------- */

typedef struct
{
	pid_t      pid;
	int32_t    chid;
	server_t*  server;
	uint32_t   flags;
	uint32_t   priority;
	vector_t   connections;
	vector_t   messages;
	klock_t    lock;
	glist_t    receive;
	glist_t    send;
	glist_t    response;
	glist_t    notify;
}channel_t;

typedef struct
{
	int32_t    scoid;       // server connection id
	channel_t* channel;
	uint32_t   flags;
    sobj_t*    shared;      // shared memory from channel owner, used for mmap
	glist_t    clinks;      // list of connection links attached to this connection
}connection_t;

typedef struct
{
	pid_t         pid;
	int32_t       coid;    // connection id
	connection_t* connection;
	uint16_t      flags;
	int16_t       refs;
    sref_t*       privMap;
	glistNode_t   node;
}clink_t;

typedef struct
{
	glistNode_t node;
    // Notification data
    int32_t     type;
    int32_t     data;
    int32_t     scoid;
    uint16_t    priority;
}notify_t;

typedef struct
{
    pid_t       pid;
    uint32_t    tid;
    int32_t     chid;
    int32_t     coid;
    int32_t     scoid;
}msg_info_t;

/* Exported constants ------------------------------------- */

#define INVALID_CHID                  (-1)
#define CHANNEL_FLAGS_VALID(flags)    (!(flags & ~(0x1F)))
#define CHANNEL_SCOID_DETACH_NOTIFY   (1 << 0)
#define CHANNEL_SCOID_ATTACH_NOTIFY   (1 << 1)
#define CHANNEL_FIXED_PRIORITY        (1 << 2)
#define CHANNEL_TASK_UNBLOCK          (1 << 3)
#define CHANNEL_COID_DEATH_NOTIFY     (1 << 4)

#define CHANNEL_IS_DIRECTORY          (1 << 5)    // TODO: should this flags only be applicable for services?
#define CHANNEL_IS_DEVICE             (1 << 6)    // TODO: should this flags only be applicable for services?
#define CHANNEL_OBJ_UNREF_PURGE       (1 << 7)    // TODO: is it needed;
#define CHANNEL_OBJ_UNREF_NOTIFY      (1 << 8)    // TODO: is it needed;

#define INVALID_COID                  (-1)
#define CONNECTION_FLAGS_VALID(flags) (!(flags & ~(0x07)))
#define CONNECTION_NOT_SHARED		  (1 << 0)
#define CONNECTION_SERVER_BONDED	  (1 << 1)
#define CONNECTION_PRIVATE	          (1 << 2)

#define INVALID_RCVID                 (-1)
#define NOTIFY_RCVID                  (0)

#define IPC_ERROR					  (-1)
#define IPC_CHANNEL_DEAD		 	  (-2)
#define IPC_CHONNECTION_DEAD		  (-3)
#define IPC_TASK_DEAD				  (-4)

// Pulses types
#define _NOTIFY_SCOID_ATTACH_         (0x1)
#define _NOTIFY_SCOID_DETACH_         (0x2)
#define _NOTIFY_TASK_UNBLOCK_         (0x3)
#define _NOTIFY_COID_DEAD_            (0x4)

/* Exported macros ---------------------------------------- */
#define CONNECTION_SCOID(scoid)			(scoid & 0xFFFF)
#define CONNECTION_CHID(scoid)			(scoid >> 16)


/* Exported functions ------------------------------------- */

void IpcSendCancel(task_t* task);

void IpcReceiveCancel(task_t* task);

void IpcReplyCancel(task_t* task);

/*
 * @brief   Routine to create a new IPC channel for the running process
 *
 * @param   flags - channel configuration flags
 *
 * @retval  Return chid or in case of error -1
 */
int32_t ChannelCreate(uint32_t flags);

/*
 * @brief   Routine to create a connection to the channel chid owned by the process pid
 *
 * @param   pid - channel owner process id
 *          chid - channel id
 *          index - base value for returned coid
 *          flags - connection configuration flags
 *
 * @retval  Return coid or in case of error -1
 */
int32_t ConnectAttach(pid_t pid, int32_t chid, uint32_t index, uint32_t flags);

int32_t ker_ChannelDestroy(process_t* process, channel_t* channel);

/*
 * @brief   Routine to destroy the IPC channel
 *
 * @param   chid - id of the channel being destroyed
 *
 * @retval  Return success
 */
int32_t ChannelDestroy(int32_t chid);

int32_t ker_ConnectDetach(process_t* process, clink_t* link, bool_t force);

/*
 * @brief   Routine to detach a connection from its channel
 *
 * @param   coid - connection id
 *
 * @retval  Return success
 */
int32_t ConnectDetach(int32_t coid);

/*
 * @brief   System call to send a message through an IPC channel
 *
 * @param   coid - connection id
 * 			hdr - message header
 *          smsg - message being sent
 *          rmsg - reply buffer
 *          offset - used to return the reply side
 *
 * @retval  Return success
 */
int32_t MsgSend(int32_t coid, const io_hdr_t* hdr, const char* smsg, const char* rmsg, uint32_t* offset);

/*
 * @brief   System call to received a message/notify through an IPC channel
 *
 * @param   chid - channel id
 * 			hdr - received message header
 *          msg - buffer to receive messages
 *          size - size of the receive buffer
 *          info - structure to be filled with sender connection information
 *
 * @retval  Return rcvid for a message, 0 if we received a pulse or -1 in case of error
 */
int32_t MsgReceive(int32_t chid, io_hdr_t* hdr, const char* msg, size_t size, uint32_t* offset, msg_info_t* info);

/*
 * @brief   System call to respond/reply to a received message
 *
 * @param   rcvid - received message id (who we are replying to)
 *          status - return for MsgSend
 *          msg - reply message buffer
 *          size - reply message size
 *
 * @retval  Return success
 */
int32_t MsgRespond(int32_t rcvid, int32_t status, const char *msg, size_t size);

/*
 * @brief   System call to write to a received message reply buffer
 *
 * @param   rcvid - received message id
 *          msg - data to be written to the reply buffer
 *          size - reply message size
 *          offset - offset to write the message in the reply buffer
 *
 * @retval  Return success
 */
int32_t MsgWrite(int32_t rcvid, const void *msg, size_t size, int32_t offset);

/*
 * @brief   System call to read to a received message send buffer
 *
 * @param   rcvid - received message id
 *          msg - memory buffer
 *          size - size of the memory buffer
 *          offset - offset to read from the message send buffer
 *
 * @retval  Return success
 */
int32_t MsgRead(int32_t rcvid, const void *msg, size_t size, int32_t offset);

int32_t ker_MsgNotify(connection_t* connection, int32_t priority, int32_t type, int32_t value);

/*
 * @brief   System call to send a notification through an IPC channel
 *
 * @param   coid - connection id
 *          priority - notification priority
 *          code - notification type
 *          value - 4 bytes of data being sent
 *
 * @retval  Return success
 */
int32_t MsgNotify(int32_t coid, int32_t priority, int32_t type, int32_t value);


void ChannelPriorityResolve(channel_t* channel, task_t* task, uint16_t prio);

void ChannelPriorityAdjust(task_t* task, uint16_t prio);

#endif /* _IPC_H_ */
