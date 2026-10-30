#include <fcntl.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <unistd.h>

int main(void)
{
  int fd = 0;
  ssize_t nsize = 0;
  unsigned char data[4] = {0};
  double temp = 0;
  double humity = 0;

  // 1. 打开dht11设备节点  /dev/misc_dht11
  fd = open("/dev/misc_dht11", O_RDWR);
  if (fd < 0)
  {
    perror("[APP]:fail to open misc_dht11!");
    return -1;
  }

  // 2. 读取温湿度数据
  while (1)
  {
    nsize = read(fd, data, 4);
    if (nsize != 4)
    {
      perror("[APP]:fail to read dht11!");
      continue;
    }

    humity = data[0] + (data[1] / 100.0);
    temp = data[2] + (data[3] / 100.0);
    printf("temp = %.2lf, humity = %.2lf\n", temp, humity);
    sleep(2);
  }

  return 0;
}