/**
 * @file        process.h
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        16 August, 2018
 * @brief       Process Manager Definition Header File
*/

#ifndef _PROCESS_H_
#define _PROCESS_H_


/* Includes ----------------------------------------------- */
#include <proctypes.h>
#include <system.h>


/* Exported types ----------------------------------------- */



/* Exported constants ------------------------------------- */



/* Exported macros ---------------------------------------- */


/* Private functions -------------------------------------- */



/* Exported functions ------------------------------------- */

/*
 * @brief   Initializes the process handler structure
 * 			It will initialize all memory handlers and the process main task
 *
 * @param   proc - process handler structure
 * 			attr - process creation attributes
 * 			argv - command line used to start the process
 *
 * @retval  Success
 */
int32_t ProcessInit(process_t *proc, procAttr_t *attr, const char *argv);

/*
 * @brief   Add new process to a parent process (process creating the new process)
 *
 * @param   parent - parent process handler structure
 * 			child - new process handler structure
 *
 * @retval  Success
 */
int32_t ProcessAddChild(process_t *parent, process_t *child);

/*
 * @brief   Routine to add the process main task to the scheduler ready list
 *
 * @param   proc - process handler structure
 *
 * @retval  Success
 */
int32_t ProcessStart(process_t *proc);

/*
 * @brief   Routine to terminate the execution of all process tasks and
 * 			to close all opens channels and connections
 *
 * @param   proc - process handler structure
 *
 * @retval  No return
 */
void ProcessTerminate(process_t *process);

/*
 * @brief   Routine to clean and free all memory allocated by a process
 *
 * @param   proc - process handler structure
 *
 * @retval  No return
 */
void ProcessMemoryClean(process_t *process);

/*
 * @brief   Routine to check if a task is the process main task
 *
 * @param   tid - task ID to be checked
 *
 * @retval  True or false
 */
bool_t ProcessIsMainTask(uint32_t tid);

/*
 * @brief   Routine to create a new task for the specified process
 *
 * @param   process - task parent process
 * 			taskAttr - task creation attributes
 * 			arg - argument to pass to the task routine
 * 			entry - task routine entry point
 * 			mainTask - if True we are creating a main task, false normal task
 *
 * @retval  Returns the task structure handler
 */
//task_t  *ProcessTaskCreate(process_t *process, taskAttr_t *taskAttr, void *arg, void *entry, bool_t mainTask);
task_t* ProcessTaskCreate(process_t* process, taskAttr_t* taskAttr, void* arg, void* entry, void* exit, bool_t mainTask);

/*
 * @brief   Routine to set the process virtual space as the current one
 *
 * @param   proc - process handler structure
 *
 * @retval  No return
 */
void ProcessEnableVirtualSpace(process_t *process);

/*
 * @brief   Routine to get a task from a given process
 *
 * @param   process - task parent process
 * 			tid - task id
 *
 * @retval  Returns the task structure handler
 */
task_t *ProcessGetTask(process_t *process, uint32_t tid);

vaddr_t ProcessRegisterPrivMemory(process_t *process, mbv_t *memory, int32_t parts, size_t size, memCfg_t *memcfg);

sref_t* ProcessRegisterShareMemory(process_t* process, uint32_t coid, sobj_t* sobj, memCfg_t* memcfg);

dev_obj_t *ProcessRegisterDevice(process_t *process, dev_t *device, memCfg_t *memcfg);

int32_t ProcessCleanPrivateObject(pobj_t* obj);

int32_t ProcessCleanSharedRef(process_t* process, sref_t* obj);

int32_t ProcessCleanDevice(process_t* process, dev_obj_t* device);

int32_t ProcessCopyConnectionsRange(process_t* parent, process_t* child, uint32_t fd_count, int32_t* fd_map);

int32_t ProcessCopyConnections(process_t* parent, process_t* child);

#endif /* _PROCESS_H_ */
