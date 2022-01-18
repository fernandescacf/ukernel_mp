/**
 * @file        task.h
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        18 August, 2018
 * @brief       Tasks Manager Definition Header File
*/

#ifndef _TASK_H_
#define _TASK_H_


/* Includes ----------------------------------------------- */
#include <types.h>
#include <glist.h>
#include <vstack.h>

#include <proctypes.h>


/* Exported types ----------------------------------------- */



/* Exported constants ------------------------------------- */
#define TASK_CANCEL_TYPE_MASK   (1 << 0)
#define TASK_CANCEL_ENABLE      (0 << 0)
#define TASK_CANCEL_DISABLE     (1 << 0)
#define TASK_CANCEL_STATE_MASK	(1 << 1)
#define TASK_CANCEL_DEFERRED	(0 << 1)
#define TASK_CANCEL_ASYNC       (1 << 1)
#define TASK_CANCEL_PENDING     (1 << 4)

#define TASK_CANCELED_RET		((void*)(-1))

#define TASK_DETACHED				(1 << 0)
#define TASK_PRIV_NONE				(PRIV_NONE << 1)
#define TASK_PRIV_IO				(PRIV_IO << 1)

enum
{
	PRIV_NONE,
	PRIV_IO
};


/* Exported macros ---------------------------------------- */



/* Private functions -------------------------------------- */



/* Exported functions ------------------------------------- */

/* @brief	Routine to initialize a new task.
 *
 * @param	parent - Task parent
 * 			attr   - Task attributes
 * 			param  - Task Parameters
 *
 * @retval	Returns Success
 */
int32_t TaskInit(task_t *task, taskAttr_t *attr, taskParam_t *param);

/* @brief	Routine to create the idle task
 *
 * @param	No parameters
 *
 * @retval	Returns the idle task handler struture
 */
task_t *TaskCreateIdle();

/* @brief	Routine to set the task id
 *
 * @param	task - Task
 * 			id   - Task id
 *
 * @retval	No return
 */
void TaskSetId(task_t *task, uint32_t id);

/* @brief	Routine to initialize the task stack
 *
 * @param	stackManager - Parent process stack manager
 * 			size  - Initial stack size
 * 			task  - Task
 * 			argv  - Command used to start the process (only applicable for the main task)
 *
 * @retval	Returns success
 */
int32_t TaskSetStack(sManager_t *stackManager, size_t size, task_t *task, const char *argv);

/* @brief	Routine to clean/free all memory allocated by the task
 * 			Note: If called in the task being cleaned it has to be called using _CriticalCall
 *
 * @param	task  - Task
 *
 * @retval	No return
 */
void TaskClean(task_t *task);

/* @brief	Routine to terminate the task.
 * 			Will free all joined tasks and remove the task from any pending list
 *
 * @param	task  - Task
 *
 * @retval	No return
 */
void TaskTerminate(task_t *task, void *ret, bool_t killed);

int32_t TaskExpandStack(task_t *task, uint32_t size);

#endif /* _TASK_H_ */
