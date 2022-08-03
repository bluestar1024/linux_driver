#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <poll.h>
#include <signal.h>
#include <linux/input.h>

int main(int argc,char *argv[])
{
    int fd = 0;
    int ret = 0;
    unsigned short data[3];
    unsigned short ir = 0,als = 0,ps = 0;

    if(argc != 2)
    {
        printf("usage error!\n");
        exit(-1);
    }

    fd = open(argv[1],O_RDWR);
    if(fd < 0)
    {
        printf("open file %s error!\n",argv[1]);
        exit(-1);
    }

    while(1)
    {
        ret = read(fd,data,sizeof(data));
        if(ret == 0)
        {
            ir = data[0];
            als = data[1];
            ps = data[2];
            printf("ap3216c ir = %d,als = %d,ps = %d!\n",ir,als,ps);
        }
        usleep(200000);
    }

    ret = close(fd);
    if(ret < 0)
    {
        printf("close file %s error!\n",argv[1]);
        exit(-1);
    }
    return 0;
}