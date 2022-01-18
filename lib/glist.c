/**
 * @file        glist.c
 * @author      Carlos Fernandes
 * @version     2.0
 * @date        19 August, 2018
 * @brief       Kernel Generic List implementation
*/

/* Includes ----------------------------------------------- */
#include "glist.h"


/* Private types ------------------------------------------ */



/* Private constants -------------------------------------- */



/* Private macros ----------------------------------------- */



/* Private variables -------------------------------------- */


/* Private function prototypes ---------------------------- */

static int32_t GlistInsert(glist_t *list, glistNode_t *node)
{
    glistNode_t *iterator = list->first;

    if(NULL == list->ListSort)
    {
        return E_FAULT;
    }

    while(NULL != iterator && 0 < list->ListSort(iterator, node))
    {
        iterator = iterator->next;
    }

    if(NULL == iterator)
    {
        //Insert new object at the end of the list
        node->next = NULL;
        node->prev = list->last;

        if(list->last)
        {
            list->last->next = node;
        }
        else
        {
            list->first = node;
        }

        list->last = node;
    }
    else
    {
        node->next = iterator;
        node->prev = iterator->prev;
        iterator->prev = node;

        if(node->prev)
        {
            node->prev->next = node;
        }
        else{
            list->first = node;
        }
    }

    list->count += 1;

    return E_OK;
}

static int32_t GcircularListInsert(glist_t *list, glistNode_t *node)
{
    glistNode_t *iterator = list->first;
    uint32_t count = list->count;

    if(NULL == list->ListSort)
    {
        return E_FAULT;
    }

    if(0 == count)
    {
        list->first = node;
        list->last = node;
        list->count += 1;

        node->next = node;
        node->prev = node;

        return E_OK;
    }

    while(0 < count && 0 < list->ListSort(iterator, node))
    {
        iterator = iterator->next;
        count -= 1;
    }

    node->next = iterator;
    node->prev = iterator->prev;
    iterator->prev->next = node;
    iterator->prev = node;

    if(list->first == iterator && 0 < count)
    {
        list->first = node;
    }
    else if(0 == count)
    {
        list->last = node;
    }

    list->count += 1;

    return E_OK;
}

static int32_t GqueueInsert(glist_t *list, glistNode_t *node)
{
    node->prev = list->last;
    node->next = NULL;

    if(NULL != list->last)
    {
    	list->last->next = node;
    }

    list->last = node;

    if(NULL == list->first)
	{
		list->first = node;
	}

    list->count += 1;

    return E_OK;
}

static glistNode_t *GlistSearch(glist_t *list, void *value)
{
    glistNode_t *iterator = list->first;
    int count = list->count;

    if(NULL == list->ListCmp)
    {
        return NULL;
    }

    while(0 < count && 0 != list->ListCmp(iterator, value))
    {
        iterator = iterator->next;
        count -= 1;
    }

    if(0 == count)
    {
        return NULL;
    }
    else
    {
        return iterator;
    }
}

static void GlistRemoveNode(glist_t *list, glistNode_t *node)
{
    if(NULL != node->prev)
    {
        node->prev->next = node->next;
    }
    else
    {
        list->first = node->next;
    }

    if(NULL != node->next)
    {
        node->next->prev = node->prev;
    }
    else
    {
        list->last = node->prev;
    }

    list->count -= 1;
}

static void GcircularListRemoveNode(glist_t *list, glistNode_t *node)
{
    if(node == node->next)
    {
        list->first = NULL;
        list->last = NULL;
    }
    else
    {
        node->next->prev = node->prev;
        node->prev->next = node->next;

        if(list->first == node)
        {
           list->first = node->next;
        }

        if(list->last == node)
        {
            list->last = node->prev;
        }
    }

    list->count -= 1;
}

static void GqueueRemoveNode(glist_t *list, glistNode_t *node)
{
    GlistRemoveNode(list, node);
}


/* Dispatch Tables ---------------------------------------- */

static int32_t (*InsertHandler[4])(glist_t *, glistNode_t *) =
    {
        GlistInsert,
        GcircularListInsert,
        GqueueInsert,
		GqueueInsert
    };

static void (*RemoveHandler[4])(glist_t *, glistNode_t *) =
    {
        GlistRemoveNode,
        GcircularListRemoveNode,
		GqueueRemoveNode,
		GqueueRemoveNode
    };

/* Private functions -------------------------------------- */

/**
 * GlistInitialize Implementation (See header file for description)
*/
int32_t GlistInitialize(glist_t *list, glistType_t type)
{
    if(NULL == list)
    {
        return E_INVAL;
    }

    list->first = 0;
    list->last = 0;
    list->count = 0;
    list->ListCmp = 0;
    list->ListSort = 0;
    list->type = type;

    RWlockInit(&list->lock);

    return E_OK;
}

/**
 * GlistSetSort Implementation (See header file for description)
*/
int32_t GlistSetSort(glist_t *list, int32_t (*ListSort)(glistNode_t *, glistNode_t *))
{
    if(NULL == list)
    {
        return E_INVAL;
    }

    if(GFifo != list->type && GLifo != list->type)
    {
        list->ListSort = ListSort;
    }

    return E_OK;
}

/**
 * GlistSetCmp Implementation (See header file for description)
*/
int32_t GlistSetCmp(glist_t *list, int32_t (*ListCmp) (glistNode_t *, void *))
{
    if(NULL == list)
    {
        return E_INVAL;
    }

    if(GFifo != list->type && GLifo != list->type)
    {
        list->ListCmp = ListCmp;
    }

    return E_OK;
}

/**
 * GlistInsertObject Implementation (See header file for description)
*/
int32_t GlistInsertObject(glist_t *list, glistNode_t *node)
{
    if(NULL == list || NULL == node)
    {
        return E_INVAL;
    }

    node->owner = list;
    node->next = NULL;
    node->prev = NULL;

    uint32_t status;
    WriteLock(&list->lock, &status);

    int32_t ret = InsertHandler[list->type](list,node);

    WriteUnlock(&list->lock, &status);

    return ret;
}

/**
 * GlistGetFirst Implementation (See header file for description)
*/
glistNode_t *GlistGetFirst(glist_t *list)
{
    if(NULL == list)
    {
        return NULL;
    }

    ReadLock(&list->lock);

    glistNode_t* node = list->first;

    ReadUnlock(&list->lock);

    return node;
}

/**
 * GlistGetLast Implementation (See header file for description)
*/
glistNode_t *GlistGetLast(glist_t *list)
{
    if(NULL == list)
    {
        return NULL;
    }

    ReadLock(&list->lock);

    glistNode_t* node = list->last;;

    ReadUnlock(&list->lock);

    return node;
}

/**
 * GlistGetObject Implementation (See header file for description)
*/
glistNode_t *GlistGetObject(glist_t *list, void *value)
{
    if(NULL == list)
    {
        return NULL;
    }

    glistNode_t* node = NULL;

    ReadLock(&list->lock);

    if(GFifo == list->type)
    {
    	node = GlistGetFirst(list);
    }
    else if (GLifo == list->type)
    {
    	node = GlistGetLast(list);
    }
    else
    {
    	node = GlistSearch(list, value);
    }

    ReadUnlock(&list->lock);

    return node;
}

/**
 * GlistRemoveSpecific Implementation (See header file for description)
*/
int32_t GlistRemoveSpecific(glistNode_t *node)
{
    if(NULL == node || NULL == node->owner)
    {
        return E_INVAL;
    }

    glist_t *list = (glist_t*)node->owner;

    uint32_t status;
    WriteLock(&list->lock, &status);

    uint32_t count = list->count;

    RemoveHandler[list->type](list, node);

    WriteUnlock(&list->lock, &status);

    node->owner = NULL;
    node->next = NULL;
    node->prev = NULL;

    if(count > list->count)
    {
        return E_OK;
    }
    else
    {
        return E_SRCH;
    }
}

/**
 * GlistRemoveObject Implementation (See header file for description)
*/
glistNode_t *GlistRemoveObject(glist_t *list, void *value)
{
    if(NULL == list)
    {
        return NULL;
    }

    glistNode_t *node = NULL;

    uint32_t status;
    WriteLock(&list->lock, &status);

    if(GFifo == list->type)
	{
    	node = GlistGetFirst(list);
	}
	else if (GLifo == list->type)
	{
		node = GlistGetLast(list);
	}
	else
    {
        node = GlistSearch(list, value);
    }

    if(NULL == node)
	{
		return NULL;
	}

	RemoveHandler[list->type](list, node);

	WriteUnlock(&list->lock, &status);

	node->owner = NULL;
	node->next = NULL;
	node->prev = NULL;

	return node;
}

/**
 * GlistRemoveFirst Implementation (See header file for description)
*/
glistNode_t *GlistRemoveFirst(glist_t *list)
{
	if((NULL == list) || (list->count == 0))
	{
		return NULL;
	}

    uint32_t status;
    WriteLock(&list->lock, &status);

	glistNode_t *node = GlistGetFirst(list);

	GlistRemoveNode(list, node);

	WriteUnlock(&list->lock, &status);

	node->owner = NULL;
	node->next = NULL;
	node->prev = NULL;

	return node;
}

int32_t GlisFifotSort(glistNode_t *c, glistNode_t *n)
{
	(void)c; (void)n;

	return 0;
}

int32_t GlisLifotSort(glistNode_t *c, glistNode_t *n)
{
	(void)c; (void)n;

	return 1;
}
