#include <asm/delay.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/time.h>

extern void msleep(unsigned int msecs);

struct i2c_client *pbh1750_client;

// 驱动层read函数
static ssize_t bh1750_read(struct file *fp, char __user *puser, size_t n, loff_t *off)
{
  struct i2c_msg msg;
  unsigned char buff[2] = {0};  // buff[0]：high  buff[1]:low
  short data = 0;
  int ret = 0;
  unsigned long nret = 0;

  // 1. 发送通电指令
  buff[0] = 0x01;
  msg.addr = pbh1750_client->addr;
  msg.flags = 0;
  msg.buf = buff;
  msg.len = 1;

  ret = pbh1750_client->adapter->algo->master_xfer(pbh1750_client->adapter, &msg, 1);
  if (ret < 0)
  {
    pr_info("[driver]:master_xfer_power_on failed: %d\n", ret);
    return ret;
  }

  // 2. 发送连续高分辨率测量模式
  buff[0] = 0x10;
  msg.addr = pbh1750_client->addr;
  msg.flags = 0;
  msg.buf = buff;
  msg.len = 1;

  ret = pbh1750_client->adapter->algo->master_xfer(pbh1750_client->adapter, &msg, 1);
  if (ret < 0)
  {
    pr_info("[driver]:master_xfer_mode failed: %d\n", ret);
    return ret;
  }

  // 3. 等待测量完成
  msleep(180);

  // 4. 读取2个字节光照强度
  msg.addr = pbh1750_client->addr;
  msg.flags |= I2C_M_RD;  
  msg.buf = buff;
  msg.len = 2;

  ret = pbh1750_client->adapter->algo->master_xfer(pbh1750_client->adapter, &msg, 1);
  if (ret < 0)
  {
    pr_info("[driver]:master_xfer_read failed: %d\n", ret);
    return ret;
  }

  // 5. 光强原始数据
  data = (buff[0] << 8) | buff[1];

  // 6. 拷贝给用户层
  nret = copy_to_user(puser, &data, sizeof(data));
  if (nret)
  {
    pr_info("[driver]:copy_to_user failed!\n");
    return nret;    
  }

  return sizeof(data);
}

// 驱动层操作函数集合
static struct file_operations fops = {
    .read = bh1750_read,
};

// 杂项设备结构体 /dev/misc_bh1750
static struct miscdevice misc_bh1750 = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "misc_bh1750",
    .fops = &fops,
};

// probe函数
static int bh1750_probe(struct i2c_client *pclient, const struct i2c_device_id *pdevice)
{
  int ret = 0;

  // 1. 注册杂项设备(10 自动生成设备节点)
  ret = misc_register(&misc_bh1750);
  if (ret)
  {
    pr_info("[driver]:misc_register failed!\n");
    return ret;
  }

  // 2. i2c_client赋值
  pbh1750_client = pclient;

  // 3. 打印
  pr_info("bh1750_probe successful!\n");

  return 0;
}

// remove函数
static int bh1750_remove(struct i2c_client *pclient)
{
  // 1. 注销杂项设备
  misc_deregister(&misc_bh1750);

  // 2. 打印
  pr_info("bh1750_remove successful!\n");

  return 0;
}

// idtable匹配
struct i2c_device_id device_table[] = {
    {.name = "putebh1750"},
    {},
};

// 设备树匹配
struct of_device_id bh1750_table[] = {
    {.compatible = "pute,bh1750"},
    {},
};

// i2c_driver结构体
static struct i2c_driver bh1750_driver = {
    .probe = bh1750_probe,
    .remove = bh1750_remove,
    .driver =
        {
            .name = "putebh1750",
            .owner = THIS_MODULE,
            .of_match_table = bh1750_table,
        },
    .id_table = device_table,
};

module_i2c_driver(bh1750_driver);  // i2c_add_driver + i2c_del_driver

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PUTE");