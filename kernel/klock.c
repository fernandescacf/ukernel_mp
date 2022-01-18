/**
 * @file        klock.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        30 October, 2020
 * @brief       In Kernel Lock implementation
*/

/* Includes ----------------------------------------------- */
#include <klock.h>
#include <arch.h>
#include <spinlock.h>
#include <atomic.h>


/* Private types ------------------------------------------ */



/* Private constants -------------------------------------- */
#define KLOCK_FREE				(uint32_t)(-1)


/* Private macros ----------------------------------------- */
#define RUNNING_CPU				_cpuId()


/* Private variables -------------------------------------- */



/* Private function prototypes ---------------------------- */



/* Private functions -------------------------------------- */

/**
 * KlockInit Implementation (See header file for description)
*/
void KlockInit(klock_t* lock)
{
    lock->count = 0;
    lock->owner = KLOCK_FREE;
}

/**
 * Klock Implementation (See header file for description)
*/
void Klock(klock_t* lock, uint32_t* status)
{
    if(status)
    {
        critical_lock(status);
    }
    else
    {
        critical_lock((uint32_t*)&status);
    }

    uint32_t cpu = RUNNING_CPU;

    if(lock->owner != cpu)
    {
        while(atomic_cmp_set(&lock->owner, KLOCK_FREE, cpu) != E_OK)
        {
            _cpu_hold();
        }
    }

    lock->count++;
}

/**
 * Kunlock Implementation (See header file for description)
*/
void Kunlock(klock_t* lock, uint32_t* status)
{
    uint32_t cpu = RUNNING_CPU;

    if(lock->owner != cpu)
    {
        return;
    }

    if(--lock->count == 0)
    {
        while(atomic_cmp_set(&lock->owner, cpu, KLOCK_FREE) != E_OK);
        // Signal cpus in case they are waiting on this lock
        _cpus_signal();
    }

    if(status)
    {
        critical_unlock(status);
    }
}

/**
 * KlockEnsure Implementation (See header file for description)
*/
void KlockEnsure(klock_t* lock, uint32_t* status)
{
    if(status)
    {
        critical_lock(status);
    }
    else
    {
        critical_lock((uint32_t*)&status);
    }

    uint32_t cpu = RUNNING_CPU;

    if(lock->owner == cpu)
    {
        return;
    }

    while(atomic_cmp_set(&lock->owner, KLOCK_FREE, cpu) != E_OK)
    {
        _cpu_hold();
    }

    lock->count++;
}

