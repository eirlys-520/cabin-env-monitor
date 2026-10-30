#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

int main(void)
{
    int fd = 0;
    short data = 0;

    // 1. 打开bh1750节点
    fd = open("/dev/misc_bh1750", O_RDWR);
    if (fd < 0)
    {
        perror("fail to open!");
        return -1;
    }

    // 2. 读取bh1750光照强度
    while (1)
    {
        read(fd, &data, sizeof(data));
        printf("light = %.2lflx\n", data / 1.2);
        sleep(1);
    }

    close(fd);

    return 0;
}