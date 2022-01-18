/**
 * @file        vector.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        12 January, 2020
 * @brief       Kernel Vector Definition Header File
*/

#ifndef VECTOR_H
#define VECTOR_H

#ifdef __cplusplus
    extern "C" {
#endif

/* Includes ----------------------------------------------- */
#include <types.h>



/* Exported types ----------------------------------------- */

struct ENTRY;
typedef struct ENTRY entry_t;

struct ENTRY
{
	entry_t *next;
};

typedef struct
{
	uint16_t		 size;
	uint16_t		 nfree;
	entry_t			*free;
	void			**data;
}vector_t;


/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */


/* Exported functions ------------------------------------- */

/*
 * @brief   Initialize the vector with the specified size
 * @param   vector	- vector handler
 * 			size	- Initial size of the vector
 * @retval  Return Success
 */
int32_t VectorInit(vector_t *vector, uint32_t size);

/*
 * @brief   Insert a new object in the vector. If vector is full
 * 			it will be expanded till it reaches the maximum allowed size
 * @param   vector	- vector handler
 * 			obj		- pointer to the object to be inserted
 * @retval  Index where the object was inserted
 */
int32_t VectorInsert(vector_t *vector, void *obj);

/*
 * @brief   Insert a new object in the vector at the specified index. It is only
 * 			guarantee that the object will be inserted at >= index.
 * @param   vector	- vector handler
 * 			obj		- pointer to the object to be inserted
 * 			index	- index to insert the object
 * @retval  Index where the object was inserted
 */
int32_t VectorInsertAt(vector_t *vector, void *obj, uint32_t index);

/*
 * @brief   Removes the object at the specified index
 * @param   vector	- vector handler
 * 			index	- index to remove object
 * @retval  Removed object or NULL if there was no object at the specified index
 */
void *VectorRemove(vector_t *vector, uint32_t index);

/*
 * @brief   Get object at the specified index without removing it
 * @param   vector	- vector handler
 * 			index	- index to get object
 * @retval  Pointer to the object or NULL if there was no object
 */
void *VectorPeek(vector_t *vector, uint32_t index);

/*
 * @brief   Routine to get the current vector usage
 * @param   vector	- vector handler
 *
 * @retval  Return number of slots used
 */
uint32_t VectorUsage(vector_t *vector);

/*
 * @brief   Routine to get the current vector size
 * @param   vector	- vector handler
 *
 * @retval  Return the vector size
 */
uint32_t VectorSize(vector_t *vector);

/*
 * @brief   Frees all allocated resources by the vector
 * @param   vector	- vector handler
 *
 * @retval  No return
 */
void VectorFree(vector_t *vector);


#ifdef __cplusplus
    }
#endif

#endif // VECTOR_H
