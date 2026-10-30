#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef enum
{
    X, //与x轴的角度
    Y, //与y轴的角度
    Z  //与z轴的角度
}DIR_t;

short ADXL345_get_result(float x, float y, float z, unsigned char dir) //计算角度
{
    float t;
    float res;

    switch(dir)
    {
        case X:
            t = x / sqrt(y*y + z*z);
            res = atan(t); //求出弧度
            break;
        case Y:
            t = y / sqrt(x*x + z*z);
            res = atan(t);
            break;
        case Z:
            t = sqrt(x*x + y*y) / z;
            res = atan(t);
            break;
        default:
            break;
    }

    return (res * 180 / 3.14); //求出角度
}

int main(void)
{
    int fd = 0;
    short data[3] = {0};
    ssize_t nret = 0;
    short rx, ry, rz;

    fd = open("/dev/misc_adxl345", O_RDWR);
    if (-1 == fd)
    {
        perror("fail to open");
        return -1;
    }

    while (1)
    {
        nret = read(fd, data, sizeof(data));
        #if 0
        printf("x = %07d,y = %07d,z = %07d\r", data[0], data[1], data[2]);        
        #endif
        rx = ADXL345_get_result(data[0], data[1], data[2], X);
        ry = ADXL345_get_result(data[0], data[1], data[2], Y);
        rz = ADXL345_get_result(data[0], data[1], data[2], Z);
        printf("rx = %03d, ry = %03d, rz = %03d\r", rx, ry, rz);
        fflush(stdout);
        usleep(100000);
    }

    close(fd);

    return 0;
}