# 密闭舱环境监测与告警系统

基于 IMX6ULL + 嵌入式 Linux 的密闭舱环境监测系统：多路传感器实时采集、LVGL 触摸屏界面显示与告警研判、MQTT 上报云端。从内核驱动、应用层多线程框架到上位机界面与云端对接，完整自主实现。

## 系统架构

```
传感器层（内核驱动 .ko）          应用层（多线程 + 邮箱消息框架）            输出
┌─────────────────────┐      ┌──────────────────────────────┐      ┌─────────────┐
│ DHT11   温湿度       │      │  get_data_th  采集线程        │      │ LVGL 触摸屏  │
│ BH1750  光照 (I2C)   │ ───► │        │                     │ ───► │  四页界面    │
│ MQ-2    烟雾 (ADC)   │      │        ▼  邮箱队列            │      ├─────────────┤
│ LM393   火焰 (GPIO)  │      │  show/save/send/lv_data 线程  │ ───► │ MQTT 云平台  │
│ ADXL345 振动/倾角(SPI)│      │        │                     │      ├─────────────┤
└─────────────────────┘      │        ▼                     │      │ SQLite 本地  │
                             └──────────────────────────────┘      └─────────────┘
```

**线程模型**：`get_data` 采集线程将 `struct msgdata` 通过邮箱队列分发给 4 个消费者线程——

- `show_data`：终端打印实时数据
- `save_data`：本地存储（SQLite）
- `send_data`：MQTT 上报 OneNET 云平台（单条报文携带全部属性）
- `lv_data`：刷新 LVGL 界面；`lv_start_th` 独立线程驱动 LVGL tick 渲染

## 功能特性

| 功能 | 说明 |
| --- | --- |
| 多传感器采集 | DHT11 温湿度、BH1750 光照、MQ-2 烟雾（ppm 换算）、LM393 火焰、ADXL345 振动倾角 |
| 风险研判 | 多参数阈值融合，输出 4 级风险等级（正常 / 提示 / 隐患 / 高危） |
| 触摸屏界面 | LVGL v8，800×480 Framebuffer，四个页面：实时数据 / 历史曲线 / 阈值设置 / 异常状态 |
| 页面切换 | 触摸导航、自动轮播（10s/页）、异常事件自动跳转（停留 15s）三种触发方式 |
| 云端上报 | MQTT（Paho）对接 OneNET，属性包含温度、湿度、光照、烟雾、火焰、风险等级、在线状态、告警计数 |
| 本地存储 | SQLite 记录历史数据 |
| 故障容错 | 单路传感器读取失败保旧值，连续失败判定离线，不阻塞其余数据刷新 |

## 硬件平台

- **主控**：IMX6ULL（ARM Cortex-A7），嵌入式 Linux
- **显示**：800×480 触摸屏（Framebuffer + evdev 触摸）
- **传感器**
  - DHT11 温湿度（单总线，GPIO）
  - BH1750 光照强度（I2C）
  - MQ-2 烟雾（ADC / IIO 子系统）
  - LM393 红外火焰（GPIO 数字量 + ADC 模拟量）
  - ADXL345 三轴加速度（SPI，用于振动与倾角）

## 目录结构

```
.
├── src/                  应用层源码
│   ├── main.c            主程序：线程注册、采集线程、MQTT 上报线程
│   ├── sensors.c         传感器统一采集模块（五路汇总）
│   ├── mailbox.c         邮箱消息框架
│   ├── linkqueue.c       消息队列
│   ├── link.c            线程链表管理
│   ├── mqtt.c            MQTT（Paho / OneNET）封装
│   ├── mylv.c            LVGL 数据线程与渲染
│   ├── mysqlite.c        SQLite 本地存储
│   └── Makefile          交叉编译脚本
├── include/              头文件（消息结构体、各模块接口）
├── drivers/              传感器内核驱动与调试程序
│   ├── dht11/            温湿度：驱动 + 应用
│   ├── bh1750/           光照：驱动 + 应用
│   ├── mq2/              烟雾：IIO ADC 应用
│   ├── flame/            火焰：AO/DO 双路应用
│   └── adxl345/          加速度：驱动 + 应用
└── docs/
    └── 项目报告.md        系统设计说明
```

> 注：`lib/` 目录（LVGL、sqlite3、openssl、paho 等第三方依赖）体积较大未纳入仓库，需自行获取：
> LVGL v8 + lv_port_linux_frame_buffer、sqlite3、openssl、paho.mqtt.c。

## 编译与部署

**工具链**：`arm-linux-gnueabihf-gcc`（交叉编译）

```bash
cd src
make            # 生成可执行文件 app
```

**部署**：将 `app` 与依赖库（`libpaho-mqtt3c`、`libsqlite3`、`libssl`、`libcrypto`）拷贝到板端根文件系统，加载传感器内核模块后运行：

```bash
insmod dht11_drv.ko
insmod bh1750_drv.ko
insmod adxl345_drv.ko
./app
```

## 传感器数据来源

| 数据 | 设备节点 / 接口 | 换算方式 |
| --- | --- | --- |
| 温度 / 湿度 | `/dev/misc_dht11` | 4 字节原始数据，整数 + 小数/100 |
| 光照 | `/dev/misc_bh1750` | 原始值 ÷ 1.2（lx） |
| 烟雾 | IIO `in_voltage1_raw` | 电压 → Rs → 浓度公式 ppm |
| 火焰 | IIO `in_voltage0_raw` / GPIO2 | AO 采 ADC 原始值 0~4095，DO 兜底判有无火 |

## 关键指标

- 采集周期：3s，MQTT 上报同步
- 界面刷新：事件驱动（每条采集数据触发一次）
- 风险等级：4 级（0 正常 / 1 提示 / 2 隐患 / 3 高危）
