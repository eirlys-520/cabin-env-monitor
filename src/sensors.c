/*
 * sensors.c —— 密闭舱监测项目：传感器统一采集
 * 读取逻辑与 E:\PT2026\Driver\sensor_code 下各独立调试程序完全一致，
 * 只是汇总到本模块供 get_data_th 调用，替换原来的 rand() 随机值。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include "sensors.h"

/* ================== 可按实际接线调整的配置 ================== */
#define DHT11_DEV        "/dev/misc_dht11"                                /* DHT11 设备节点      */
#define BH1750_DEV       "/dev/misc_bh1750"                               /* BH1750 设备节点     */
#define MQ2_IIO_DIR      "/sys/bus/iio/devices/iio:device0"               /* MQ-2 AO 接 ADC1     */
#define MQ2_ADC_CH       1                                                /* MQ-2 通道号(VX=IN1) */
#define FLAME_ADC_CH     0                                                /* 火焰 AO 接的通道号  */
#define FLAME_GPIO_NUM   2                                                /* 火焰 DO 接 GPIO1_IO02 */
#define DO_FLAME_LEVEL   0                                                /* 有火时 DO 电平      */
#define FLAME_DO_FIRE    3500.0f                                          /* DO 判有火时填充的 ADC 值 */
#define FLAME_DO_SAFE    300.0f                                           /* DO 判无火时填充的 ADC 值 */
#define FAIL_LIMIT       3                                                /* 连续失败N次才算离线 */
/* ============================================================ */

/* mq-2 浓度换算公式参数（来自已验证的 adc-mq2 调试程序） */
#define MQ2_R0           6.64f
#define MQ2_RL           0.5f

/* 设备句柄（sensors_init 中打开） */
static int fd_dht11  = -1;
static int fd_bh1750 = -1;

/* 各路传感器状态：最近一次成功值 + 连续失败计数（失败保旧值，防止UI数值跳变） */
static struct {
    float temp, hum;      int dht_fail;
    float lux;            int lux_fail;
    float smoke;          int smoke_fail;
    float flame;          int flame_fail;
} cache = {
    .temp = 0, .hum = 0, .lux = 0, .smoke = 0, .flame = 0,
};

/* ==================== 通用工具 ==================== */

/* 读 sysfs 整数值（IIO 框架用，同 mq2/flame 调试程序） */
static int sysfs_read_int(const char *path, int *val)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        printf("[sensors] can't open %s: %s\r\n", path, strerror(errno));
        return -1;
    }
    if (fscanf(fp, "%d", val) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

/* 读 sysfs 浮点值（in_voltage_scale） */
static int sysfs_read_float(const char *path, float *val)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return -1;
    if (fscanf(fp, "%f", val) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

/* ==================== DHT11 温湿度 ==================== */
/* 驱动返回4字节：[0]湿度整数 [1]湿度小数 [2]温度整数 [3]温度小数 */
static int dht11_read(float *temp, float *hum)
{
    unsigned char data[4] = {0};

    if (fd_dht11 < 0)
        return -1;
    if (read(fd_dht11, data, 4) != 4)
        return -1;

    *hum  = data[0] + data[1] / 100.0f;
    *temp = data[2] + data[3] / 100.0f;
    return 0;
}

/* ==================== BH1750 光照 ==================== */
/* 驱动返回 short 原始值，实际照度 = 原始值 / 1.2 */
static int bh1750_read(float *lux)
{
    short data = 0;

    if (fd_bh1750 < 0)
        return -1;
    if (read(fd_bh1750, &data, sizeof(data)) != sizeof(data))
        return -1;

    *lux = data / 1.2f;
    return 0;
}

/* ==================== MQ-2 烟雾 ==================== */
/* IIO 读原始值 -> 电压 -> 气体浓度公式（与 adc-mq2 调试程序一致） */
static int mq2_read(float *ppm)
{
    int raw = 0;
    float scale = 0.0f, act = 0.0f, rs = 0.0f;
    char path[128];

    snprintf(path, sizeof(path), "%s/in_voltage_scale", MQ2_IIO_DIR);
    if (sysfs_read_float(path, &scale) < 0)
        return -1;

    snprintf(path, sizeof(path), "%s/in_voltage%d_raw", MQ2_IIO_DIR, MQ2_ADC_CH);
    if (sysfs_read_int(path, &raw) < 0)
        return -1;

    act = (scale * raw) / 1000.0f;              /* 实际电压 V */
    rs  = (5.0f - act) / act * MQ2_RL;          /* 传感器电阻（公式来自调试程序） */
    *ppm = powf(11.5428f * MQ2_R0 / rs, 0.6549f) * 100.0f;
    return 0;
}

/* ==================== LM393 火焰 ==================== */
/* AO(ADC模拟量) 优先：直接取原始值 0~4095 与 UI 阈值体系一致；
   AO 不可用时用 DO(GPIO2) 兜底：有火填充 FLAME_DO_FIRE，无火填充 FLAME_DO_SAFE */
static int flame_read(float *flame)
{
    int raw = 0;
    char path[128];
    FILE *fp;

    /* 1) 先尝试 AO 模拟量 */
    snprintf(path, sizeof(path),
             "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw", FLAME_ADC_CH);
    if (sysfs_read_int(path, &raw) == 0) {
        *flame = (float)raw;
        return 0;
    }

    /* 2) AO 失败，用 DO 数字量兜底（gpio sysfs） */
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", FLAME_GPIO_NUM);
    fp = fopen(path, "r");
    if (fp == NULL) {
        /* 首次可能未导出，尝试导出一次 */
        fp = fopen("/sys/class/gpio/export", "w");
        if (fp != NULL) {
            fprintf(fp, "%d", FLAME_GPIO_NUM);
            fclose(fp);
            usleep(50000);
            snprintf(path, sizeof(path),
                     "/sys/class/gpio/gpio%d/value", FLAME_GPIO_NUM);
            fp = fopen(path, "r");
        }
        if (fp == NULL)
            return -1;
    }
    if (fscanf(fp, "%d", &raw) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    *flame = (raw == DO_FLAME_LEVEL) ? FLAME_DO_FIRE : FLAME_DO_SAFE;
    return 0;
}

/* ==================== 对外接口 ==================== */

int sensors_init(void)
{
    int mask = 0;

    fd_dht11 = open(DHT11_DEV, O_RDWR);
    if (fd_dht11 < 0)
        printf("[sensors] %s open failed: %s\r\n", DHT11_DEV, strerror(errno));
    else
        mask |= 0x01;

    fd_bh1750 = open(BH1750_DEV, O_RDWR);
    if (fd_bh1750 < 0)
        printf("[sensors] %s open failed: %s\r\n", BH1750_DEV, strerror(errno));
    else
        mask |= 0x02;

    printf("[sensors] init done, ready mask = 0x%x\r\n", mask);
    return mask;
}

int sensors_read_all(struct msgdata *data)
{
    float t = 0, h = 0, l = 0, s = 0, f = 0;

    /* ---- DHT11 温湿度 ---- */
    if (dht11_read(&t, &h) == 0) {
        cache.temp = t;  cache.hum = h;  cache.dht_fail = 0;
    } else if (++cache.dht_fail >= FAIL_LIMIT) {
        printf("[sensors] DHT11 连续 %d 次读取失败\r\n", cache.dht_fail);
    }
    data->temp = cache.temp;
    data->hum  = cache.hum;

    /* ---- BH1750 光照 ---- */
    if (bh1750_read(&l) == 0) {
        cache.lux = l;  cache.lux_fail = 0;
    } else if (++cache.lux_fail >= FAIL_LIMIT) {
        printf("[sensors] BH1750 连续 %d 次读取失败\r\n", cache.lux_fail);
    }
    data->lux = cache.lux;

    /* ---- MQ-2 烟雾 ---- */
    if (mq2_read(&s) == 0) {
        cache.smoke = s;  cache.smoke_fail = 0;
    } else if (++cache.smoke_fail >= FAIL_LIMIT) {
        printf("[sensors] MQ-2 连续 %d 次读取失败\r\n", cache.smoke_fail);
    }
    data->smoke = cache.smoke;

    /* ---- LM393 火焰 ---- */
    if (flame_read(&f) == 0) {
        cache.flame = f;  cache.flame_fail = 0;
    } else if (++cache.flame_fail >= FAIL_LIMIT) {
        printf("[sensors] 火焰传感器连续 %d 次读取失败\r\n", cache.flame_fail);
    }
    data->flame = cache.flame;

    return 0;
}
