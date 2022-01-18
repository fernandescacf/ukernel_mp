/**
 * @file        system.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        14 May, 2020
 * @brief       Kernel System (kernel NameSpace Owner) Definition Header File
*/

#ifndef _SYSTEM_H_
#define _SYSTEM_H_


/* Includes ----------------------------------------------- */
#include <types.h>
#include <io_types.h>
#include <glist.h>
#include <mmap.h>
#include <proctypes.h>

/* Exported types ----------------------------------------- */

struct NameSpace;
typedef struct NameSpace namespace_t;

struct Server;
typedef struct Server server_t;

struct NameSpace
{
	namespace_t *owner;
	namespace_t *namespaces;
	namespace_t *siblings;
	server_t    *servers;
	uint32_t    nentries;
	uint32_t    sentries;
	uint16_t    flags;
	uint16_t    len;
	char name[1];
};

struct Server
{
	namespace_t *owner;
	server_t    *siblings;

	uint32_t pid;
	int32_t  chid;

	uint16_t flags;
	uint16_t len;
	char     name[1];
};

/* Exported constants ------------------------------------- */
#define NOFD			(-1)


/* Exported macros ---------------------------------------- */


/* Private functions -------------------------------------- */


/* Exported functions ------------------------------------- */

void SystemInit();

int32_t ServerTerminate(int32_t chid);

int32_t ker_ServerTerminate(process_t *process, int32_t chid);

int32_t SystemReceive(const io_hdr_t *hdr, const char *obuff, const char *ibuff, uint32_t *offset);

#endif /* _IPC_H_ */
