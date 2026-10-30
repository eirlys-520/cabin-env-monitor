#ifndef __LINK_H__
#define __LINK_H__

#include <pthread.h>
#include <semaphore.h>
#include "linkqueue.h"

typedef struct thread_task
{
    char name[32];        //线程任务名称
    pthread_t tid;               //线程id号
    void *(*pfun)(void *);  //线程要执行任务
    LQue_t *pque;            //用来通信的队列
    pthread_mutex_t mutex;   //队列互斥锁
    sem_t sem;               //同步队列读写的信号量
}LData_t;

//链表结点类型
typedef struct node
{
    LData_t data;               //数据域：保存的数据
    struct node *pnext;     //指针域：下一个结点的地址
}Node_t;

//链表对象类型
typedef struct link
{
    Node_t *phead;   //链表头节点指针
    int clen;        //链表当前结点的个数
}Link_t;


extern Link_t *create_link();
extern int insert_link_head(Link_t *plink, LData_t data);
extern void show_link(Link_t *plink);
extern int insert_link_tail(Link_t *plink, LData_t data);
extern int delete_link_head(Link_t *plink);
extern int delete_link_tail(Link_t *plink);
extern void destroy_link(Link_t *plink);

#endif