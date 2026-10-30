#ifndef __SENSORS_H__
#define __SENSORS_H__

#include "linkqueue.h"

/*
 * 传感器统一采集模块
 *
 * 对接的外设（内核驱动 .ko 已在板端 insmod，/dev 节点全局可见）：
 *   DHT11      /dev/misc_dht11                    温度 + 湿度
 *   BH1750     /dev/misc_bh1750                   光照强度 lx
 *   MQ-2       IIO: iio:device0 通道1              烟雾 ppm（含浓度换算公式）
 *   火焰LM393  IIO通道0(AO) 优先 / GPIO2(DO) 兜底   火焰 ADC 原始值 0~4095
 *
 * 使用方式：
 *   sensors_init()      采集线程启动时调用一次（打开设备）
 *   sensors_read_all()  每个采集周期调用，自动填充 mydata.temp/hum/lux/smoke/flame
 *                       内部带"失败保旧值 + 连续失败计数"，单路故障不影响整机
 *
 * 注意：ADXL345 暂未接入（struct msgdata 当前无 vib/tilt 字段，send_data 上报
 *       vib:0/tilt:0 为占位），后续加字段后在此模块补一路 /dev/misc_adxl345 即可。
 */

extern int sensors_init(void);
extern int sensors_read_all(struct msgdata *data);

#endif
