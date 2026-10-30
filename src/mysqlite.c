#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sqlite3.h>
#include "mysqlite.h"


int msqlite3_init(void)
{
    //sqlite3_open
    //sqlite3_exec();
    //sqlite3_close();
    sqlite3* db;
    int ret = sqlite3_open("./studentinfo.db",&db);
    if(SQLITE_OK != ret)
    {
        fprintf(stderr,"open error:%s\n",sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
       
    char sql_cmd1[512]="create table info(id INT, name TEXT, age INT);";//table name  info

    ret = sqlite3_exec(db,sql_cmd1,NULL,NULL,NULL);

    

    sqlite3_close(db);
    printf("Hello World!\n");
    return 0;
}