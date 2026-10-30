#ifndef __MYLV_H__
#define __MYLV_H__

#include <pthread.h>
#include "mailbox.h"
#include "link.h"
#include "linkqueue.h"

// LVGL渲染tick线程
void *lv_start_th(void *arg);
// 邮箱接收数据，转换并推送给UI
void *lv_data_th(void *arg);

#endif
