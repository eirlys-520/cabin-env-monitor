#include "linkqueue.h"
#include <stdio.h>
#include <stdlib.h>

LQue_t * create_link_queue()
{
    LQue_t *pque = malloc(sizeof(LQue_t)); 
    if (NULL == pque)
    {
        printf("malloc error\n");
        return NULL;
    }  
    pque->phead = NULL;
    pque->ptail = NULL;
    pque->clen = 0;

    return pque;
}

int is_empty_link_queue(LQue_t *pque)
{
    return NULL == pque->phead;
}
int push_link_queue(LQue_t *pque, Data_t data)
{
    QNode_t *pnode = malloc(sizeof(QNode_t));
    if (NULL == pnode)
    {
        printf("malloc error\n");
        return -1;
    }
    pnode->data = data;
    pnode->pnext = NULL;

    if (is_empty_link_queue(pque))
    {
        pque->phead = pnode;
        pque->ptail = pnode;
    }
    else
    {
        pque->ptail->pnext = pnode;
        pque->ptail = pnode;
    }
    pque->clen++;

    return 0;
}

void show_link_queue(LQue_t *pque)
{
    QNode_t *ptmp = pque->phead;
    while (ptmp != NULL)
    {
   //     printf("%d ", ptmp->data);
        ptmp = ptmp->pnext;
    }
    printf("\n");
}


int pop_link_queue(LQue_t *pque, Data_t *pdata)
{
    if (is_empty_link_queue(pque))
    {
        return -1;
    }

    QNode_t *pfree = pque->phead;
    pque->phead = pfree->pnext;
    if (pdata != NULL)
    {
        *pdata = pfree->data;
    }
    free(pfree);
    if (NULL == pque->phead)
    {
        pque->ptail = NULL;
    }
    pque->clen--;

    return 0;
}

int get_link_queue_head(LQue_t *pque, Data_t *pdata)
{
    if (is_empty_link_queue(pque))
    {
        return -1;
    }
    if (pdata != NULL)
    {
        *pdata = pque->phead->data;
        return 0;
    }

    return -1;
}

void destroy_link_queue(LQue_t *pque)
{
    while (!is_empty_link_queue(pque))
    {
        pop_link_queue(pque, NULL);
    }
    free(pque);
}