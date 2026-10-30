TARGET = app
CC = arm-linux-gnueabihf-gcc

# 工程根目录
ROOT := $(shell pwd)/..

# 业务源码（加入 mylv.c）
APP_SRC = main.c linkqueue.c link.c mailbox.c mysqlite.c mqtt.c mylv.c sensors.c

# LVGL路径（lvgl在移植文件夹里面）
LV_PORT_DIR := $(ROOT)/lib/lv_port_linux_frame_buffer
LVGL_DIR    := $(LV_PORT_DIR)/lvgl
LVGL_SRC    := $(shell find $(LVGL_DIR)/src -name "*.c")
# 重点：排除lvgl目录，防止重复收集lvgl源码
PORT_SRC    := $(shell find $(LV_PORT_DIR) -name "*.c" ! -path "$(LVGL_DIR)/*")

# SquareLine UI
UI_SRC := $(wildcard $(LVGL_DIR)/UI/*.c)
UI_SRC += $(wildcard $(LVGL_DIR)/UI/components/*.c)
UI_SRC += $(wildcard $(LVGL_DIR)/UI/screens/*.c)
UI_SRC += $(wildcard $(LVGL_DIR)/UI/images/*.c)
UI_SRC += $(wildcard $(LVGL_DIR)/UI/fonts/*.c)
UI_SRC += $(wildcard $(LVGL_DIR)/UI/data/*.c)

# 全部源码 & obj
SRCS := $(APP_SRC) $(LVGL_SRC) $(PORT_SRC) $(UI_SRC)
OBJS := $(SRCS:.c=.o)

# 头文件
INC  = -I$(ROOT)/include
INC += -I$(ROOT)/include/src
INC += -I$(ROOT)/lib/sqlite3/instal/include
INC += -I$(ROOT)/lib/openssl_install/include
INC += -I$(LVGL_DIR)
INC += -I$(LV_PORT_DIR)
INC += -I$(LVGL_DIR)/UI
INC += -I$(LVGL_DIR)/UI/components
INC += -I$(LVGL_DIR)/UI/screens
INC += -I$(LVGL_DIR)/UI/fonts
INC += -I$(LVGL_DIR)/UI/images
INC += -I$(LVGL_DIR)/UI/data

# 库路径
LIBPATH  = -L$(ROOT)/lib/sqlite3/instal/lib
LIBPATH += -L$(ROOT)/lib/build/output
LIBPATH += -L$(ROOT)/lib/openssl_install/lib

# 编译选项
CFLAGS = -g -Wall -std=c99

# 链接选项
LDFLAGS = -lpthread -lsqlite3 -lpaho-mqtt3c -lrt -lcrypto -lssl -lm

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBPATH) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
