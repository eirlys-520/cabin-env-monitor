#include <asm/delay.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

// dht11引脚编号
int dht11_no = 0;

extern void msleep(unsigned int msecs);

// dht11复位
static int dht11_reset(void)
{
    int time = 0;

    // 1. 设置引脚为输出
    gpio_direction_output(dht11_no, 1);
    // 2. 引脚拉低18ms
    gpio_set_value(dht11_no, 0);
    msleep(18);
    // 3. 拉高引脚
    gpio_set_value(dht11_no, 1);

    // 1. 设置引脚为输入
    gpio_direction_input(dht11_no);
    // 2. 检测低电平到来
    while (gpio_get_value(dht11_no) && time < 20)
    {
        udelay(10);
        time++;
    }
    // 3. 超时dht11没有回复响应
    if (time >= 20)
    {
        pr_info("[driver]: dht11 no response!\n");
        return -1;
    }

    // 4. 检测高电平到来
    while (!gpio_get_value(dht11_no));

    // 5. 检测低电平到来
    while (gpio_get_value(dht11_no));

    return 0;
}

// 读取dht11温湿度数据
static int read_dht11_data(unsigned char *pdata, int len)
{
    int i = 0;
    int n = 0;
    int time = 0;
    unsigned char data[5] = {0};

    // 1. 设置引脚为输入
    gpio_direction_input(dht11_no);

    // 2. 读取5个字节(湿度整数 + 湿度小数 + 温度整数 + 温度小数 + 校验和)
    for (i = 0; i < 5; i++)
    {
        for (n = 7; n >= 0; n--) 
        {
            time = 0;
            // 等待高电平
            while (!gpio_get_value(dht11_no));
            // 判断高电平持续时间
            while (gpio_get_value(dht11_no))
            {
                udelay(10);
                time++;
            }
            // dht11发送'1'
            if (time > 4)
            {
                data[i] |= (1 << n);
            }
        }
    }

    // 3. 设置引脚输出
    gpio_direction_output(dht11_no, 1);

    // 4. 判断校验是否通过
    if (data[0] + data[1] + data[2] + data[3] != data[4])
    {
        pr_info("[driver]:dht11_check failed!\n");
        return -1;
    }

    // 5. 数据拷贝
    memcpy(pdata, data, len);

    return 0;
}

// 驱动层read函数
static ssize_t dht11_read(struct file *fp, char __user *puser, size_t n, loff_t *off)
{
    int ret = 0;
    unsigned long nsize = 0;
    unsigned char data[4] = {0};

    // 1. 复位dht11
    ret = dht11_reset();
    if (ret)
    {
        pr_info("[driver]:dht11_reset failed!\n");
        return ret;
    }

    // 2. 读取dht11温湿度
    ret = read_dht11_data(data, 4);
    if (ret)
    {
        pr_info("[driver]:dht11_read failed!\n");
        return ret;
    }

    // 3. 温湿度数据拷贝给应用层
    nsize = copy_to_user(puser, data, 4);
    if (nsize)
    {
        pr_info("[driver]:copy_to_user failed!\n");
        return nsize;        
    }

    return sizeof(data);
}

// 驱动操作函数集合
static struct file_operations dht11_fops = {
    .read = dht11_read,
};


// 混杂设备结构体
static struct miscdevice misc_dht11 = {
    .minor = MISC_DYNAMIC_MINOR,
    .name =  "misc_dht11",
    .fops = &dht11_fops,
};


// probe函数（执行时机：驱动和设备匹配成功自动执行）
int dht11_probe(struct platform_device *pdev)
{
    int ret = 0;

    // 1. 注册混杂设备
    misc_register(&misc_dht11);

    // 2. 从设备树中通过dht11节点返回引脚编号
    dht11_no = of_get_named_gpio(pdev->dev.of_node, "gpio-dht11", 0);
    if (dht11_no < 0)
    {
        pr_info("[driver]:of_get_named_gpio failed!\n");
        return -1;
    }

    // 3. 申请gpio资源
    ret = devm_gpio_request(misc_dht11.this_device, dht11_no, "dht11_gpio");
    if (ret)
    {
        pr_info("[driver]:devm_gpio_request failed!\n");
        return ret;        
    }

    // 4. 设置GPIO为输出
    gpio_direction_output(dht11_no, 1);

    // 5. 打印调试
    pr_info("dht11_probe is ok!\n");

    return 0;
}

// remove函数（执行时机：驱动和设备删除时自动执行）
int dht11_remove(struct platform_device *pdev)
{
    // 1. 注销混杂设备
    misc_deregister(&misc_dht11);

    // 2. 打印
    pr_info("dht11_remove is ok!\n");

    return 0;
}

// id_table匹配
static struct platform_device_id dht11_id_table[] = {
    {.name = "putedht11"},
    {},
};

// 设备树匹配
static struct of_device_id dht11_match_table[] = {
    {.compatible = "pute,dht11"},
    {},
};

// platfotm_driver结构体(probe、remove、匹配方法)
static struct platform_driver dht11_drv = {
    .probe = dht11_probe,
    .remove = dht11_remove,
    .driver = {
        .name = "pute_dht11",
        .owner = THIS_MODULE,
        .of_match_table = dht11_match_table,
    },
    .id_table = dht11_id_table,
};

module_platform_driver(dht11_drv); // insmod dht11_drv.ko -> platform_register_driver rmmod dht11_drv -> platform_unregister_driver

MODULE_LICENSE("GPL");  //GPL
MODULE_AUTHOR("PUTE");  