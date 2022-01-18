/**
 * @file        procmgr.h
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        26 August, 2018
 * @brief       Process Manager Definition Header File
*/

#ifndef _PROCMGR_H_
#define _PROCMGR_H_


/* Includes ----------------------------------------------- */
#include <proctypes.h>


/* Exported types ----------------------------------------- */
typedef struct
{
	uint16_t	priority;
	uint16_t	privilege;
	uint8_t		detached;
	uint8_t		heritage;
}spawn_attr_t;


/* Exported constants ------------------------------------- */
#define PARAM_UNDEFINED		(-1)


/* Exported macros ---------------------------------------- */



/* Private functions -------------------------------------- */



/* Exported functions ------------------------------------- */

int32_t ProcManagerInit(void);

uint32_t ProcProcessesRunning();

pid_t ProcSpawn(void *elf, char *cmd, spawn_attr_t* sattr, uint32_t fd_count, int32_t* fd_map);

process_t *ProcGetProcess(pid_t pid);

void ProcProcessTerminate(process_t *process);

int32_t ProcTaskCancel(uint32_t tid);

#endif /* _PROCMGR_H_ */
