#include "mylv.h"
#include "lvgl.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"

#include "UI/ui.h"

extern int system_run_flag;

// LVGL渲染主线程，只做初始化 + lv_timer_handler，不碰业务数据
void *lv_start_th(void *arg)
{
    lv_init();

    /* --- 显示：framebuffer 驱动（原 demo main.c 里搬来的） --- */
    fbdev_init();

    static lv_color_t buf[128 * 1024];
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, 128 * 1024);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res  = 800;
    disp_drv.ver_res  = 480;
    lv_disp_drv_register(&disp_drv);

    /* --- 输入：evdev 触摸（不加屏亮了也点不动） --- */
    evdev_init();
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type     = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb  = evdev_read;
    lv_indev_drv_register(&indev_drv);

    ui_init();

    while (system_run_flag)
    {
        lv_timer_handler();
        usleep(5000);
    }
    ui_destroy();
    return NULL;
}


// 邮箱接收线程：接收msgdata，转换成ui_env_t，推送给UI接口ui_env_update
void *lv_data_th(void *arg)
{
    Link_t *pmail = (Link_t *)arg;
    struct msgdata mydata;
    char send_name[32] = {0};
    int ret = 0;

    ui_env_t env_in;

    while (system_run_flag)
    {
        ret = recv_mail_msg(&mydata, send_name);
        if (0 != ret)
        {
            continue;
        }

        // ========= 把struct msgdata → 转换成ui_env_t =========
        memset(&env_in, 0, sizeof(ui_env_t));

        // 顺序：UI_S_TEMP,UI_S_HUM,UI_S_LIGHT,UI_S_VIB,UI_S_TILT,UI_S_SMOKE,UI_S_FLAME
        env_in.v[UI_S_TEMP]   = mydata.temp;
        env_in.v[UI_S_HUM]    = mydata.hum;
        env_in.v[UI_S_LIGHT]  = mydata.lux;
        env_in.v[UI_S_VIB]    = 0.0f;   // 你当前模拟数据没有振动，填0
        env_in.v[UI_S_TILT]   = 0.0f;   // 倾角模拟填0
        env_in.v[UI_S_SMOKE]  = mydata.smoke;
        env_in.v[UI_S_FLAME]  = mydata.flame;

        env_in.risk    = -1;  // 填‑1，交给UI内部按阈值自动计算风险等级
        env_in.online  = 0x3F;// 模拟：全部6个传感器在线 bit0~bit5全部置1
        env_in.mqtt    = 1;   // 模拟mqtt已连接，后面你可以改成真实状态

        // ======== 调用UI对外接口，提交数据（内部自带互斥锁，线程安全） ========
        ui_env_update(&env_in);
    }
    return NULL;
}
