/**
 * @file        glist.h
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        19 August, 2018
 * @brief       Kernel Generic List Definition Header File
*/

#ifndef GLIST_H
#define GLIST_H

#ifdef __cplusplus
    extern "C" {
#endif

/* Includes ----------------------------------------------- */
#include <types.h>
#include <misc.h>
#include <rwlock.h>


/* Exported types ----------------------------------------- */

/* Generic list types */
typedef enum
{
    GList = 0,
    GCircularList = 1,
    GFifo = 2,
	GLifo = 3
}glistType_t;

struct gListNode;
typedef struct gListNode glistNode_t;

/* Generic list node */
typedef struct gListNode
{
    glistNode_t         *next, *prev;
    void                *owner;
}glistNode_t;

/* Generic list */
typedef struct
{
    glistType_t     type;
    uint32_t        count;
    int32_t (*ListSort) (glistNode_t *, glistNode_t *);
    int32_t (*ListCmp)  (glistNode_t *, void *);
    glistNode_t     *first, *last;
    rwlock_t		lock;
}glist_t;


/* Exported constants ------------------------------------- */


/* Exported macros ---------------------------------------- */

// Macro to convert from node to a specific type
#define GLISTNODE2TYPE(node,type,member)({\
    const typeof( ((type *)0)->member ) *__mptr = (node);    \
    ((__mptr) ? (type *)( (char *)__mptr - offsetof(type,member) ) : NULL);})

// Macros to iterate manually through a list
#define GLIST_EMPTY(list)				((list)->count == 0)
#define GLIST_LAST(list,type,member)	GLISTNODE2TYPE((list)->last,type,member)
#define GLIST_FIRST(list,type,member)	GLISTNODE2TYPE((list)->first,type,member)
#define GLIST_NEXT(node,type,member)	GLISTNODE2TYPE((node)->next,type,member)
#define GLIST_PREV(node,type,member)	GLISTNODE2TYPE((node)->prev,type,member)

// Macro to get owner of the list
#define GLIST_OWNER(list,type,member)	GLISTNODE2TYPE(((glist_t*)(list)->owner),type,member)

/* Exported functions ------------------------------------- */

/*
 * @brief   Initialize the list
 * @param   list - list
 * 			type:	GList - Normal double linked list
 * 					GCircularList - Circular doubled linked list
 * 					GQueue - Queue
 * @retval  Return Success
 */
int32_t GlistInitialize(glist_t *list, glistType_t type);

/*
 * @brief   Set the sorting algorithm for the list.
 * 			Doesn't have effect on a queue type list
 * @param   list - list
 * 			ListSort - Pointer to the sorting function
 * @retval  Return Success
 */
int32_t GlistSetSort(glist_t *list, int32_t (*ListSort)(glistNode_t *, glistNode_t *));

/*
 * @brief   Set the list elements matching algorithm
 * 			Doesn't have effect on a queue type list
 * @param   list - list
 * 			ListCmp - Pointer to the comparing function
 * @retval  Return Success
 */
int32_t GlistSetCmp(glist_t *list, int32_t (*ListCmp) (glistNode_t *, void *));

/*
 * @brief   Function to insert a new element in a list.
 * 			The sorting algorithm used depends on the type of list.
 * @param   list - list
 * 			node - node of the element being inserted
 * @retval  Return Success
 */
int32_t GlistInsertObject(glist_t *list, glistNode_t *node);

/*
 * @brief   Function to get the first element in the list.
 * 			Note this function will not remove the element from the list
 * @param   list - list
 * @retval  Pointer to the first element node
 */
glistNode_t *GlistGetFirst(glist_t *list);

/*
 * @brief   Function to get the last element in the list.
 * 			Note this function will not remove the element from the list
 * @param   list - list
 * @retval  Pointer to the last element node
 */
glistNode_t *GlistGetLast(glist_t *list);

/*
 * @brief   Function to get the element in the list that matches the specified value.
 * 			Note this function will not remove the element from the list and cannot
 * 			be used in a queue type list.
 * 			Value doesn't have effect on a queue type list it will get the first
 * 			or the last node according to the queue type
 * @param   list - list
 * 			value - value to be search for
 * @retval  Pointer to the matching element node
 */
glistNode_t *GlistGetObject(glist_t *list, void *value);

/*
 * @brief   Function to remove the specified element. The list is identified by the node
 * @param   node - node to be removed
 * @retval  Return Success
 */
int32_t GlistRemoveSpecific(glistNode_t *node);

/*
 * @brief   Function to remove the element that matches the specified value
 * 			Value doesn't have effect on a queue type list it will remove the first
 * 			or the last node according to the queue type
 * @param   list - list
 * 			value - value to be search for
 * @retval  Pointer to the removed element node
 */
glistNode_t *GlistRemoveObject(glist_t *list, void *value);

/*
 * @brief   Function to remove the first element in the list.
 * 			This function will always remove the first element independently of the list type
 * @param   list - list
 *
 * @retval  Pointer to the removed node
 */
glistNode_t *GlistRemoveFirst(glist_t *list);

/*
 * @brief   Helper function to be used as a sort function by a glist.
 * 			Always insert in the list head;
 * @param   -
 *
 * @retval  -
 */
int32_t GlisFifotSort(glistNode_t *c, glistNode_t *n);

/*
 * @brief   Helper function to be used as a sort function by a glist.
 * 			Always insert in the list tail;
 * @param   -
 *
 * @retval  -
 */
int32_t GlisLifotSort(glistNode_t *c, glistNode_t *n);


#ifdef __cplusplus
    }
#endif

#endif // GLIST_H
