/**
 * @file        rwlock.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        8 December, 2020
 * @brief       Kernel Read/Write Lock implementation
*/

/* Includes ----------------------------------------------- */
#include <rwlock.h>
#include <arch.h>
#include <atomic.h>
#include <spinlock.h>


/* Private types ------------------------------------------ */



/* Private constants -------------------------------------- */


/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */



/* Private functions -------------------------------------- */

/**
 * RWlockInit Implementation (See header file for description)
*/
void RWlockInit(rwlock_t* lock)
{
    lock->readers = 0;
    lock->writer = NO_WRITER;
}

/**
 * ReadLock Implementation (See header file for description)
*/
void ReadLock(rwlock_t* lock)
{
    atomic_inc(&lock->readers);

    while(atomic_cmp_set(&lock->writer, NO_WRITER, NO_WRITER) != E_OK)
    {
        _cpu_hold();
    }
}

/**
 * ReadUnlock Implementation (See header file for description)
*/
void ReadUnlock(rwlock_t* lock)
{
    atomic_dec(&lock->readers);

    _cpus_signal();
}

/**
 * WriteLock Implementation (See header file for description)
*/
void WriteLock(rwlock_t* lock, uint32_t* status)
{
    critical_lock(status);

    while(lock->readers > 0 && atomic_cmp_set(&lock->writer, NO_WRITER, _cpuId()) != E_OK)
    {
        _cpu_hold();
    }
}

/**
 * WriteUnlock Implementation (See header file for description)
*/
void WriteUnlock(rwlock_t* lock, uint32_t* status)
{
    if(atomic_cmp_set(&lock->writer, _cpuId(), NO_WRITER) != E_OK)
    {
        // We are not the owners :O
    	_cpus_signal();
    }
    else
    {
    	_cpus_signal();

    	critical_unlock(status);
    }
}
