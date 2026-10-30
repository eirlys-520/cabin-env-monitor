#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>     /* int32_t */

/*
 * LM393 红外火焰传感器调试程序（参考 adc-mq2 框架）
 * 平台：IMX6ULL（IIO ADC 框架 + GPIO sysfs）
 *
 * 接线说明（模块共 4 引脚）：
 *   VCC -> 3.3V
 *   GND -> GND
 *   AO  -> ADC 输入引脚（模拟量，反映火焰强度，请按实际接线修改 FLAME_ADC_CH）
 *   DO  -> 普通 GPIO（数字量，模块电位器比较阈值输出，请按实际接线修改 FLAME_GPIO_NUM）
 *
 * LM393 逻辑说明：
 *   检测到火焰时 DO 通常输出低电平（若你的模块相反，把下面 DO_FLAME_LEVEL 改成 0/1 即可）
 */

/* ================== 需要根据实际接线修改的配置 ================== */
#define EN_AO               1       /* 0=纯DO模式(AO悬空)，1=DO+AO双路模式 */
#define FLAME_ADC_CH        0       /* AO 接的 ADC 通道号：接 IN1 改为 1（VX 脚），以此类推 */

/*
 * 火焰强度转换公式（线性映射）：
 *   强度% = (V_base - V) / (V_base - V_fire) * 100
 * 原理：LM393 红外接收管受火焰红外照射后导通增强，AO 电压随火焰增强而降低
 *   V_base：无火时的基准电压(V)      —— 先运行程序记录无火时的电压后填入
 *   V_fire：近距离火焰时的电压(V)    —— 同理记录近距离火焰时的电压后填入
 * 以下默认值为典型值，请按实测校准
 */
#define FLAME_V_BASE        1.8f    /* 无火基准电压 */
#define FLAME_V_FIRE        0.5f    /* 近火电压 */
#define FLAME_AVG_N         10      /* 滑动平均采样次数，让数值平稳 */
#define FLAME_GPIO_NUM      2       /* DO 接 GPIO1_IO02，编号 = (组号-1)*32 + 引脚号 = (1-1)*32+2 = 2 */
#define DO_FLAME_LEVEL      0       /* 检测到火焰时 DO 的电平，大多数 LM393 火焰模块为低电平 0 */
#define FLAME_ADC_VOLT_TH   1.5f    /* 模拟量告警阈值(V)：AO 电压低于该值视为检测到火焰
                                       先跑程序观察无火/有火时的电压，再取中间值填这里 */
#define SAMPLE_INTERVAL_MS  200     /* 采样周期 */
/* ================================================================ */

/* ADC 数据设备结构体（与 mq2 一致） */
struct adc_dev {
    int   raw;      /* ADC 原始采样值 */
    float scale;    /* ADC 电压刻度 */
    float act;      /* 实际电压值 V */
};

struct flame_dev {
    struct adc_dev adc;     /* 模拟量（AO） */
    int gpio_val;           /* 数字量（DO）：0/1 */
};

struct flame_dev flame;

/*
 * @description : 读取指定文件内容
 * @param – filename : 要读取的文件路径
 * @param - str : 读取到的文件字符串
 * @return : 0 成功;其他 失败
 */
static int file_data_read(char *filename, char *str)
{
    int ret = 0;
    FILE *data_stream;

    data_stream = fopen(filename, "r"); /* 只读打开 */
    if (data_stream == NULL) {
        printf("can't open file %s\r\n", filename);
        return -1;
    }

    ret = fscanf(data_stream, "%s", str);
    if (!ret) {
        printf("file read error!\r\n");
    } else if (ret == EOF) {
        /* 读到文件末尾的话将文件指针重新调整到文件头 */
        fseek(data_stream, 0, SEEK_SET);
    }
    fclose(data_stream); /* 关闭文件 */
    return 0;
}

/*
 * @description : 获取 ADC 数据（与 mq2 的 adc_read 一致，通道由 FLAME_ADC_CH 决定）
 * @param - dev : 设备结构体
 * @return : 0 成功;其他 失败
 */
static int adc_read(struct adc_dev *dev)
{
    int ret = 0;
    char str[50];
    char path[128];

    /* 读取电压刻度 */
    snprintf(path, sizeof(path),
        "/sys/bus/iio/devices/iio:device0/in_voltage_scale");
    ret = file_data_read(path, str);
    if (ret < 0) return ret;
    dev->scale = atof(str);

    /* 读取原始采样值 */
    snprintf(path, sizeof(path),
        "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw", FLAME_ADC_CH);
    ret = file_data_read(path, str);
    if (ret < 0) return ret;
    dev->raw = atoi(str);

    /* 转换得到实际电压值 V */
    dev->act = (dev->scale * dev->raw) / 1000.f;
    return ret;
}

/*
 * @description : 多次采样求平均后读 ADC（滤波，让转换数值平稳）
 * @param - dev : 设备结构体
 * @return : 0 成功;其他 失败
 */
static int adc_read_avg(struct adc_dev *dev)
{
    int i, ret;
    unsigned long sum = 0;

    for (i = 0; i < FLAME_AVG_N; i++) {
        ret = adc_read(dev);
        if (ret < 0)
            return ret;
        sum += dev->raw;
        usleep(2000);   /* 2ms 间隔连续采样 */
    }
    dev->raw  = sum / FLAME_AVG_N;
    dev->act  = (dev->scale * dev->raw) / 1000.f;
    return 0;
}

/*
 * @description : 传感器公式：AO 电压 -> 火焰强度（int32 型，0~100，对应物联网平台 int32 属性）
 * @param - volt : 当前 AO 电压(V)
 * @return : 火焰强度 int32，0~100（>100 按饱和处理，<0 视为 0）
 */
static int32_t volt_to_flame(float volt)
{
    float pct;

    pct = (FLAME_V_BASE - volt) / (FLAME_V_BASE - FLAME_V_FIRE) * 100.0f;
    if (pct < 0.0f)    pct = 0.0f;
    if (pct > 100.0f)  pct = 100.0f;
    return (int32_t)(pct + 0.5f);   /* 四舍五入到整数 */
}

/*
 * @description : 初始化 DO 引脚对应的 GPIO（导出并设为输入）
 * @return : 0 成功;其他 失败
 */
static int gpio_init(void)
{
    char path[64];
    FILE *fp;

    /* 判断是否已经导出，避免重复 export 报 "Device or resource busy" */
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", FLAME_GPIO_NUM);
    if (access(path, F_OK) != 0) {
        fp = fopen("/sys/class/gpio/export", "w");
        if (fp == NULL) {
            printf("can't open /sys/class/gpio/export\r\n");
            return -1;
        }
        fprintf(fp, "%d", FLAME_GPIO_NUM);
        fclose(fp);
    }

    /* 设置为输入方向 */
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", FLAME_GPIO_NUM);
    fp = fopen(path, "w");
    if (fp == NULL) {
        printf("can't open %s\r\n", path);
        return -1;
    }
    fprintf(fp, "in");
    fclose(fp);
    return 0;
}

/*
 * @description : 读取 DO 引脚电平
 * @param - val : 读到的电平值 0/1
 * @return : 0 成功;其他 失败
 */
static int gpio_read(int *val)
{
    char path[64];
    char str[8];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", FLAME_GPIO_NUM);
    if (file_data_read(path, str) < 0)
        return -1;
    *val = atoi(str);
    return 0;
}

/*
 * @description : main 主程序
 * @param – argc : argv 数组元素个数
 * @param - argv : 具体参数
 * @return : 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
    int ret = 0;
    int flame_gpio = 0;     /* DO 判断结果 */
    int flame_adc  = 0;     /* AO 判断结果 */

    if (argc != 1) {
        printf("Error Usage!\r\n");
        printf("Usage: ./flame_app  (无参数，循环采样打印)\r\n");
        return -1;
    }

    /* 初始化 DO 的 GPIO */
    ret = gpio_init();
    if (ret < 0) {
        printf("gpio_init failed! 请检查 FLAME_GPIO_NUM(%d) 是否正确\r\n", FLAME_GPIO_NUM);
        /* GPIO 失败不退出，AO 通道仍可调试 */
    }

    printf("===== LM393 火焰传感器调试程序启动 =====\r\n");
    printf("ADC通道:%d  GPIO:%d  模拟量阈值:%.2fV\r\n\r\n",
            FLAME_ADC_CH, FLAME_GPIO_NUM, FLAME_ADC_VOLT_TH);

    while (1)
    {
#if EN_AO
        ret = adc_read_avg(&flame.adc);   /* 多次采样求平均后再换算，数值更稳 */
#else
        ret = 0;    /* 纯DO模式：不读ADC，AO悬空时读数随机漂移，读进来只会造成误判 */
#endif
        if (ret == 0) {
            /* 读 DO 数字量 */
            if (gpio_read(&flame.gpio_val) == 0)
                flame_gpio = (flame.gpio_val == DO_FLAME_LEVEL);    /* 1=有火 */
            else
                flame_gpio = -1;    /* 读取失败 */

#if EN_AO
            /* AO 模拟量判断：电压低于阈值视为有火（可根据实测修改方向） */
            flame_adc = (flame.adc.act < FLAME_ADC_VOLT_TH);
#endif

            /* 调试信息：ADC 原始值 + 电压 + 传感器公式转换后的 int32 型火焰强度 */
#if EN_AO
            printf("raw:%4d  AO电压:%.3fV  火焰强度:%d | DO:%d | ",
                    flame.adc.raw, flame.adc.act,
                    volt_to_flame(flame.adc.act), flame.gpio_val);
#else
            printf("DO:%d | ", flame.gpio_val);
#endif
            if (flame_gpio == 1
#if EN_AO
                || flame_adc == 1
#endif
               )
#if EN_AO
                printf(">>> 检测到火焰! (DO:%s  AO:%s) <<<\r\n",
                        flame_gpio == 1 ? "有火" : "无火",
                        flame_adc  == 1 ? "有火" : "无火");
#else
                printf(">>> 检测到火焰! <<<\r\n");
#endif
            else
                printf("安全\r\n");
        } else {
            printf("ADC 读取失败，请检查 FLAME_ADC_CH(%d) 与设备树配置\r\n", FLAME_ADC_CH);
        }
        usleep(SAMPLE_INTERVAL_MS * 1000);
    }
    return 0;
}
