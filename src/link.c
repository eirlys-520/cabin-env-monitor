#include "link.h"
#include <stdio.h>
#include <stdlib.h>


Link_t *create_link()
{
    Link_t *plink = malloc(sizeof(Link_t));
    if (NULL == plink)
    {
        printf("malloc error\n");
        return NULL;
    }
    plink->phead = NULL;
    plink->clen = 0;

    return plink;
}


int insert_link_head(Link_t *plink, LData_t data)
{
    Node_t *pinsert = malloc(sizeof(Node_t));
    if (NULL == pinsert)
    {
        printf("malloc error\n");
        return -1;
    }

    pinsert->data = data;
    pinsert->pnext = NULL;

    pinsert->pnext = plink->phead;
    plink->phead = pinsert;

    plink->clen++;

    return 0;
}

void show_link(Link_t *plink)
{
    Node_t *ptmp = plink->phead;

    while (ptmp != NULL)
    {
 //       printf("%d ", ptmp->data);
        ptmp = ptmp->pnext;
    }
    printf("\n");
}

int is_empty_link(Link_t *plink)
{
    if (NULL == plink->phead)
    {
        return 1;
    }
    return 0;
}
int insert_link_tail(Link_t *plink, LData_t data)
{
    Node_t *pinsert = malloc(sizeof(Node_t));
    if (NULL == pinsert)
    {
        printf("mallocc error\n");
        return -1;
    }
    pinsert->data = data;
    pinsert->pnext = NULL;

    if (is_empty_link(plink))
    {
        plink->phead = pinsert;
    }
    else
    {
        Node_t *ptmp = plink->phead;
        while (ptmp->pnext != NULL)
        {
            ptmp = ptmp->pnext;
        }
        ptmp->pnext = pinsert;
    }
    plink->clen++;

    return 0;
}


int delete_link_head(Link_t *plink)
{
    if (is_empty_link(plink))
    {
        return -1;
    }

    Node_t *pfree = plink->phead;
    plink->phead = pfree->pnext;
    free(pfree);
    plink->clen--;

    return 0;
}

int delete_link_tail(Link_t *plink)
{
    if (is_empty_link(plink))
    {
        return -1;
    }
    else if (NULL == plink->phead->pnext)
    {
        free(plink->phead);
        plink->phead = NULL;
    }
    else
    {
        Node_t *ptmp = plink->phead;
        while (ptmp->pnext->pnext != NULL)
        {
            ptmp = ptmp->pnext;
        }
        free(ptmp->pnext);
        ptmp->pnext = NULL;
    }
    plink->clen--;

    return 0;
}

void destroy_link(Link_t *plink)
{
    while (!is_empty_link(plink))
    {
        delete_link_head(plink);
    }
    free(plink);
}





