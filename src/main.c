#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include "link.h"
#include "mailbox.h"
#include "linkqueue.h"
#include "mqtt.h"
#include "mylv.h"
#include "sensors.h"
#include <sys/time.h>

int system_run_flag = 1;
Link_t *g_pmail = NULL; //全局邮箱指针，信号函数里用

void sig_handler(int sig)
{
    (void)sig;
    system_run_flag = 0;
    sleep(1);
    if(g_pmail)
    {
        destroy_mailbox(g_pmail);
    }
}

void *get_data_th(void *arg)
{
    struct msgdata mydata;
    int cnt = 0;
    time_t sec;
    struct tm *ptm = NULL;

    sensors_init();  /* 打开 DHT11/BH1750 设备（MQ-2/火焰走 sysfs，无需打开） */

    while (system_run_flag)
    {
        memset(&mydata, 0, sizeof(mydata));
        mydata.id = 1;
        mydata.cnt = cnt++;

        /* 真实传感器采集（sensors.c）：DHT11温湿度 / BH1750光照 / MQ-2烟雾 / LM393火焰
           内部已做失败保旧值处理，单路传感器故障不影响其余数据刷新 */
        sensors_read_all(&mydata);

        mydata.alarm_sta = 0;
        time(&sec);
        ptm = localtime(&sec);
        mydata.tim = *ptm;

        send_mail_msg("get_data", mydata, "show_data");
        send_mail_msg("get_data", mydata, "save_data");
        send_mail_msg("get_data", mydata, "send_data");
        send_mail_msg("get_data", mydata, "lv_data"); //新增：发给LVGL数据线程
        sleep(3);
    }
    return NULL;
}

void *show_data_th(void *arg)
{
    struct msgdata mydata;
    char send_name[32] = {0};
    int ret = 0;
    while (system_run_flag)
    {
        ret = recv_mail_msg(&mydata, send_name);
        if (0 != ret)
        {
            continue;
        }
        printf("%s->show: id = %d, cnt = %d, temp = %.2f, hum = %.2f, lux=%.2f, smoke=%.2f, flame=%.2f\n",
               send_name, mydata.id, mydata.cnt, mydata.temp, mydata.hum, mydata.lux, mydata.smoke, mydata.flame);
    }
    return NULL;
}

void *save_data_th(void *arg)
{
    struct msgdata mydata;
    char send_name[32] = {0};
    int ret = 0;
    while (system_run_flag)
    {
        ret = recv_mail_msg(&mydata, send_name);
        if (0 != ret)
        {
            continue;
        }
        printf("%s->save: id = %d, cnt = %d, temp = %.2f, hum = %.2f\n",send_name, mydata.id, mydata.cnt, mydata.temp, mydata.hum);
        //这里后面写sqlite存储
    }
    return NULL;
}

#include "UI/ui.h"     /* 顶部补上，取 risk / alarm_cnt 用 */

void *send_data_th(void *arg)
{
    struct msgdata mydata;
    char send_name[32] = {0};
    char params[512];
    int ret = 0;
    mqtt_init();
    while (system_run_flag)
    {
        ret = recv_mail_msg(&mydata, send_name);
        if (0 != ret)
        {
            continue;
        }
        const ui_env_t * penv = ui_env_get();   /* UI 研判结果（线程安全接口） */

        snprintf(params, sizeof(params),
            "\"temp\":{\"value\":%.2f},\"hum\":{\"value\":%.2f}"
            ",\"light\":{\"value\":%.2f},\"smoke\":{\"value\":%.0f}"
            ",\"flame\":{\"value\":%.0f},\"vib\":{\"value\":0}"
            ",\"tilt\":{\"value\":0},\"risk\":{\"value\":%d}"
            ",\"online\":{\"value\":%d},\"alarm_cnt\":{\"value\":%d}",
            mydata.temp, mydata.hum, mydata.lux, mydata.smoke, mydata.flame,
            penv ? penv->risk : 0, 0x3F, ui_alarm_n);

        mqtt_send_multi(params);     /* 一条报文带全部属性，替代原来的 6 次循环 */
    }
    mqtt_deinit();
    return 0;
}


int main(void)
{
    signal(SIGINT, sig_handler); //注册Ctrl+C退出信号
    pthread_t lv_tid;

    g_pmail = create_mailbox_system();
    if (NULL == g_pmail)
    {
        return -1;
    }
    //注册原有业务线程
    register_thread_mailbox(g_pmail, "get_data", get_data_th);
    register_thread_mailbox(g_pmail, "show_data", show_data_th);
    register_thread_mailbox(g_pmail, "save_data", save_data_th);
    register_thread_mailbox(g_pmail, "send_data", send_data_th);
    //注册LVGL数据接收线程【lv_data】
    register_thread_mailbox(g_pmail, "lv_data", lv_data_th);

    //单独启动LVGL渲染tick线程
    pthread_create(&lv_tid, NULL, lv_start_th, NULL);
    //lv_data_th 是邮箱回调，已经被register_thread_mailbox管理，不用pthread_create！！
    //❗重点：lv_data_th 是邮箱框架自动创建，不要再pthread_create，只有lv_start_th是独立线程！

    //等待所有线程运行，main阻塞
    while(system_run_flag)
    {
        sleep(1);
    }
    return 0;
}

uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if (start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    return (uint32_t)(now_ms - start_ms);
}
