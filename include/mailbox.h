#ifndef __MAILBOX_H__
#define __MAILBOX_H__

#include "link.h"

extern Link_t *create_mailbox_system();
extern int register_thread_mailbox(Link_t *pmail, char *name, void *(*pfun)(void *));
extern void destroy_mailbox(Link_t *pmail);
extern int recv_mail_msg(struct msgdata *mydata, char *send_name);
extern int send_mail_msg(char *send_name, struct msgdata mydata, char *recv_name);

#endif