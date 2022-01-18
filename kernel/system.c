/**
 * @file        system.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        14 May, 2020
 * @brief       Kernel System (kernel NameSpace Owner) implementation
*/

/* Includes ----------------------------------------------- */
#include <system.h>
#include <ipc_2.h>
#include <procmgr.h>
#include <process.h>
#include <scheduler.h>
#include <memmgr.h>
#include <string.h>
#include <misc.h>
#include <kheap.h>
#include <rwlock.h>


/* Private types ------------------------------------------ */

// Name Space entry structure
typedef struct
{
	int32_t  type;
	uint32_t nentries;
	uint32_t sentries;
	uint32_t len;
	char name[1];
}nentry_t;

// Server entry structure
typedef struct
{
	int32_t  type;
	pid_t    pid;
	int32_t  chid;
	uint32_t len;
	char name[1];
}sentry_t;

/* Private constants -------------------------------------- */


/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */

static struct
{
    namespace_t root;
    rwlock_t	lock;
}System;

/* Private function prototypes ---------------------------- */

namespace_t *PathResolve(const char *path, char **remaining)
{
    namespace_t *parent = &System.root;
    namespace_t *current;

    char *ptr = (char*)path;

    while(*ptr)
    {
        // Skip '/'
        if(ptr[0] == '/')
        {
            ptr++;
        }
        else
        {
            *remaining = ptr;
            break;
        }

        uint32_t len = 0;
        for( ; ptr[len] && ('/' != ptr[len]); len++) {}

        for(current = parent->namespaces; NULL != current; current = current->siblings)
        {
            if(current->len == len && !memcmp(ptr, current->name, len))
            {
                break;
            }
        }

        if(current == NULL)
        {
            *remaining = ((ptr[0] == 0) ? (NULL) : (ptr));
            return parent;
        }

        ptr += len;
        parent = current;
    }

    if(ptr[0] == 0)
    {
        *remaining = NULL;
    }

    return parent;
}

void NamespaceAdd(namespace_t *parent, namespace_t *child)
{
    child->owner = parent;
    child->siblings = parent->namespaces;
    parent->namespaces = child;
    parent->nentries++;
}

namespace_t *NamespaceRegister(namespace_t *parent, char *path, char **remaining, uint16_t flags)
{
    uint32_t len = 0;
    for( ; path[len] && ('/' != path[len]); len++) {}

    if(path[len] == 0)
    {
        return NULL;
    }

    namespace_t* current = (namespace_t*)kmalloc(sizeof(namespace_t) + len);

    current->flags = flags;
    current->len = len;
    current->namespaces = NULL;
    current->servers = NULL;
    current->nentries = 0;
    current->sentries = 0;

    memcpy(current->name, path, len);

    NamespaceAdd(parent, current);

    *remaining = (path + len + 1);

    return current;
}

server_t *ServerSearch(namespace_t *parent, const char *name, uint32_t len)
{
	server_t *server = parent->servers;
	for( ; NULL != server; server = server->siblings)
	{
		if(server->len == len && !memcmp(name, server->name, len))
		{
			return server;
		}
	}
	return NULL;
}

server_t *AddServer(namespace_t *parent, char *name, int32_t chid, pid_t pid, uint32_t flags)
{
    uint32_t len = strlen(name);
    server_t *server = (server_t*)kmalloc(sizeof(server_t) + len);

    server->chid = chid;
    server->pid = pid;
    server->owner = parent;
    server->len = len;
    memcpy(server->name, name, len);
    server->siblings = parent->servers;
    parent->servers = server;
    parent->sentries++;

    return server;
}

void NameSpaceClean(namespace_t *ns)
{
	if(ns->namespaces != NULL || ns->servers != NULL)
	{
		return;
	}

	ns->owner->nentries--;

	if(ns->owner->namespaces == ns)
	{
		ns->owner->namespaces = ns->siblings;
		// Check if parent namespace become empty
		NameSpaceClean(ns->owner);
	}
	else
	{
		namespace_t *it = ns->owner->namespaces;
		for(; it->siblings != ns; it = it->siblings){}
		it->siblings = ns->siblings;
	}

	kfree(ns, sizeof(*ns) + ns->len);
}

void ServerRemove(server_t *server)
{
	// Remove server from name space
	namespace_t *namespace = server->owner;
	// Decrease server entries
	namespace->sentries--;

	if(namespace->servers == server)
	{
		namespace->servers = server->siblings;

		// Check if namespace is still valid or if we need to clean it
		NameSpaceClean(namespace);
	}
	else
	{
		server_t *it;
		for(it = namespace->servers; it->siblings != server; it = it->siblings){}
		it->siblings = server->siblings;
	}
}

/* Private functions -------------------------------------- */

void SystemInit()
{
    memset(&System.root, 0x0, sizeof(namespace_t));
    RWlockInit(&System.lock);
}

// TODO: Pass flags to set server as a device or a directory?
int32_t ServerInstall(int32_t chid, const char *path)
{
	// Get running process
	process_t *process = SchedGetRunningProcess();

	// Get Channel
	channel_t *channel = (channel_t*)VectorPeek(&process->channels, chid);

	if((channel == NULL) || (channel->server != NULL))
	{
		return E_BUSY;
	}

	// Get path length
	uint32_t length = strlen(path);

	// Last character has to be different from '/' but first character has to be '/'
	if((path[0] != '/') || (path[length - 1] == '/'))
	{
		return E_INVAL;
	}

	// We will be modifying the system path so lock it
	uint32_t status;
	WriteLock(&System.lock, &status);

    // Get the last name space in the specified path
    char *remaining;
    namespace_t *parent = PathResolve(path, &remaining);

    // If we consumed the whole path there is nothing more to be registered...
    // The path is invalid
    if(*remaining == 0)
    {
    	WriteUnlock(&System.lock, &status);
        return E_INVAL;
    }

    // Register all needed name spaces
    while(TRUE)
    {
        namespace_t *current = NamespaceRegister(parent, remaining, &remaining, 0x0);

        // All name spaces where resolved
        if(current == NULL)
        {
            break;
        }

        parent = current;
    }

    // At this point we only have the name of the server left
    channel->server = AddServer(parent,remaining, chid, process->pid, 0);

    WriteUnlock(&System.lock, &status);

	return channel->chid;
}

// TODO: Channel could have a flag to check if we close server the channel also dies
// set when we create the server
int32_t ker_ServerTerminate(process_t *process, int32_t chid)
{
	// Get Channel
	channel_t *channel = (channel_t*)VectorPeek(&process->channels, chid);

	// Did we get a valid channel with a active server
	if(channel == NULL || channel->server == NULL)
	{
		return E_INVAL;
	}

	// Get server
	server_t *server = channel->server;

	// Disconnect server from Channel
	channel->server = NULL;

	// TODO: set connections and links server bonded to invalid

#ifdef NO_DEFINED_BEHAVIOUR
	// Close all shared objects
	while(!GLIST_EMPTY(&server->sharedList))
	{
		// Get shared object
		shared_obj_t *shared = GLISTNODE2TYPE(GlistRemoveFirst(&server->sharedList), shared_obj_t, node);

		// If there is a client using this shared object we need to unmap it from the client space
		connection_t *connection = (connection_t*)VectorPeek(&channel->connections, shared->scoid);

		if(connection->sref != NULL)
		{
			// Remove reference to this shared object
			ProcessCleanSharedRef(ProcGetProcess(connection->pid), connection->sref);
		}
		// TODO: Add a flag to signal the connection of the object closing?
		// (Terminating the shared object will probably crash the client)

		kfree(shared);
	}
#endif

	// We will be modifying the system path so lock it
	uint32_t status;
	WriteLock(&System.lock, &status);

	// Remove server from system namespace
	ServerRemove(server);

	WriteUnlock(&System.lock, &status);

	kfree(server, sizeof(*server) + server->len);

	return E_OK;
}

int32_t ServerTerminate(int32_t chid)
{
	// Just a entry point for the system call

	return ker_ServerTerminate(SchedGetRunningProcess(), chid);
}

int32_t ServerConnect(const char *path)
{
    // Get Process
    // Do we need? It seems not

    // Get path length
    uint32_t length = strlen(path);

    // Last character has to be different from '/' but first character has to be '/'
    if((path[0] != '/') || (path[length - 1] == '/'))
    {
        return -1;
    }

	// We will Only read so lock it for reading
	ReadLock(&System.lock);

    // First we need to find the name space where the server is registered
    char *remaining;
    namespace_t *parent = PathResolve(path, &remaining);

    if(remaining == NULL)
    {
        // Nothing left to search for a server
    	ReadUnlock(&System.lock);
        return -1;
    }

    for(length = 0; remaining[length] != '\0'; length++)
    {
        if(remaining[length] == '/')
        {
            // We couldn't resolve the whole path
        	ReadUnlock(&System.lock);
            return -1;
        }
    }

    // Search for the server in the name space
    server_t *server;
    for(server = parent->servers; NULL != server; server = server->siblings)
    {
        if(server->len == length && !memcmp(remaining, server->name, length))
        {
            break;
        }
    }

    ReadUnlock(&System.lock);

    // If we didn't found the server
    if(server == NULL)
    {
        return -1;
    }

    int32_t coid = ConnectAttach(server->pid, server->chid, 0x0, CONNECTION_NOT_SHARED | CONNECTION_SERVER_BONDED);

    // Do we want to add a reference counter to the server?
    // We are already connected to the channel and it is the same thing if we connect to the channel
    // we are connected to the server and vice versa
    // A server is just a way to register the channel in a namespace and share memory objects

    return coid;
}

int32_t ServerDisconnect(int32_t coid)
{
	// Get running process
	process_t *process = SchedGetRunningProcess();

	// Get connection
	clink_t* connection = (clink_t*)VectorPeek(&process->connections, (uint32_t)coid);

	if(connection == NULL/* || !(connection->flags & CONNECTION_SERVER_BONDED)*/)
	{
		// Nothing to be done
		return E_INVAL;
	}
#ifdef NOT_DEF
	if(connection->sref != NULL)
	{
		if(connection->refs == 1)
		{
			// Close open shared reference
			// This call will also call ConnectDetach and since the mapped object is what is keeping the
			// connection alive this call will terminate the connection
			return ProcessCleanSharedRef(process, connection->sref);
		}
		// In this case this call will only decrease the reference count
		ProcessCleanSharedRef(process, connection->sref);
	}
#endif
	// The connection is bound to the sever we have to terminate it
	return ker_ConnectDetach(process, connection, FALSE);
}

uint32_t NameSpaceCopy(nentry_t *entry, namespace_t *namespace)
{
	entry->type = INFO_NAMESPACE;
	entry->sentries = namespace->sentries;
	entry->nentries = namespace->nentries;
	entry->len = namespace->len;
	memcpy(entry->name, namespace->name, namespace->len);

	return (offsetof(nentry_t, name) + namespace->len);
}

uint32_t ServerEntryCopy(sentry_t *entry, server_t *server)
{
	entry->type = INFO_SERVER;
	entry->pid = server->pid;
	entry->chid = server->chid;
	entry->len = server->len;
	memcpy(entry->name, server->name, server->len);

	return (offsetof(sentry_t, name) + server->len);
}

uint32_t ServerEntryAppendPath(sentry_t *entry, const char *path, uint32_t len)
{
	if(len == 0)
	{
		return 0;
	}

	uint32_t offset = 0;

	if(path[0] != '/')
	{
		entry->name[entry->len] = '/';
		offset++;
	}

	memcpy(&entry->name[entry->len + offset], path, len);

	return len + offset;
}

uint32_t NameSpaceCopyEntry(void *entry, uint32_t index, namespace_t *namespace)
{
	if(index <= namespace->nentries)
	{
		namespace_t *it = namespace->namespaces;
		uint32_t i;
		for(i = 1; i < index; i++)
		{
			it = it->siblings;
		}

		return NameSpaceCopy((nentry_t*)entry, it);
	}

	index -= namespace->nentries;

	if(index <= namespace->sentries)
	{
		server_t *it = namespace->servers;
		uint32_t i;
		for(i = 1; i < index; i++)
		{
			it = it->siblings;
		}

		return ServerEntryCopy((sentry_t*)entry, it);
	}

	return 0;
}

typedef struct
{
	uint32_t ramtotal;
	uint32_t ramavailable;
	uint32_t ramusage;
	uint32_t runningprocs;
}sysinfo_t;

int32_t SystemReadStats(char *buffer, size_t size, uint32_t *offset)
{
	if(size < sizeof(sysinfo_t))
	{
		if(offset != NULL)
		{
			*offset = 0;
		}

		return E_ERROR;
	}

	sysinfo_t *sysinfo = (sysinfo_t *)buffer;
	sysinfo->ramtotal = RamGetTotal();
	sysinfo->ramavailable = RamGetAvailable();
	sysinfo->ramusage = RamGetUsage();
	sysinfo->runningprocs = ProcProcessesRunning();

	if(offset != NULL)
	{
		*offset = sizeof(sysinfo_t);
	}

	return E_OK;
}

int32_t SystemReceive(const io_hdr_t *hdr, const char *obuff, const char *ibuff, uint32_t *offset)
{
	// This the system request handler: We only support the _IO_INFO requests

	if(hdr->type == _IO_READ)
	{
		return SystemReadStats((char*)ibuff, hdr->rbytes, offset);
	}

	if((hdr->type != _IO_INFO) || (obuff == NULL) || (obuff[0] != '/'))
	{
		return E_INVAL;
	}

	ReadLock(&System.lock);

	char *remaining = NULL;
	namespace_t *parent = PathResolve(obuff, &remaining);

	if(remaining)
	{
		if(!((hdr->code == INFO_NAMESPACE_LS) || (hdr->code == INFO_BEST_MATCH)))
		{
			// Not allowed operation. We can only request entries information to namespaces
			return E_INVAL;
		}

		uint32_t len = 0;
		uint32_t entrylen = 0;
		bool_t slashs = TRUE;
		for( ; remaining[len]; len++)
		{
			if((remaining[len] == '/') && (entrylen == 0))
			{
				entrylen = len;
			}
		}

		// Check if there were no '/'
		if(entrylen == 0)
		{
			entrylen = len;
			slashs = FALSE;
		}
		else if((hdr->code == INFO_NAMESPACE_LS))
		{
			// Namespace list is not supported if there is more than one entry in the path
			ReadUnlock(&System.lock);
			return E_INVAL;
		}

		// First remaining entry has to match a server
		server_t *server = ServerSearch(parent, remaining, entrylen);

		if(server == NULL)
		{
			// We didn't found a server so there is no way to solve the path
			ReadUnlock(&System.lock);
			return E_INVAL;
		}

		sentry_t *sentry = (sentry_t *)ibuff;
		uint32_t entrybytes = ServerEntryCopy(sentry, server);

		ReadUnlock(&System.lock);

		if(slashs == TRUE)
		{
			// There is path remaining so we have to append it to the server name
			entrybytes += ServerEntryAppendPath(sentry, &remaining[entrylen], len - entrylen);
		}

		if(offset != NULL)
		{
			*offset = entrybytes;
		}

		return E_OK;
	}

	// No path remaining and we want to get the namespace info
	if((hdr->code == INFO_NAMESPACE_LS) || (hdr->code == INFO_BEST_MATCH))
	{
		// Get a namespace entry
		nentry_t *nentry = (nentry_t *)ibuff;
		uint32_t entrybytes = NameSpaceCopy(nentry, parent);

		ReadUnlock(&System.lock);

		if(offset != NULL)
		{
			*offset = entrybytes;
		}

		return E_OK;
	}

	uint32_t entrybytes = NameSpaceCopyEntry((void*)ibuff, hdr->code, parent);

	ReadUnlock(&System.lock);

	// If no bytes where copied the entry was not valid
	if(entrybytes == 0)
	{
		return E_INVAL;
	}

	if(offset != NULL)
	{
		*offset = entrybytes;
	}

	return E_OK;
}
