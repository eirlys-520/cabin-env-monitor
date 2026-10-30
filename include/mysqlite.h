#ifndef __MYSQLITE_H__
#define __MYSQLITE_H__
#include "linkqueue.h"

//打开数据库，db_name数据库文件名
int sqlite_env_init(const char *db_name);
//插入一条msgdata数据到环境日志表
int sqlite_env_insert(struct msgdata *p_data);
//关闭数据库
void sqlite_env_close(void);

#endif
