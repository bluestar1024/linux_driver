#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#define  CLOSE_CMD              _IO(0xef,1)
#define  OPEN_CMD               _IO(0xef,2)
#define  SETPERIOD_CMD          _IOW(0xef,3,int)

int main(int argc,char *argv[])
{
    int fd = 0;
    int ret = 0;
    unsigned long cmd = 0;
    unsigned long data = 0;

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
        printf("please input cmd:\n");
        ret = scanf("%d",&cmd);
        if(ret != 1)
        {
            printf("get cmd error!\n");
            exit(-1);
        }
        if(cmd == 1)
            cmd = CLOSE_CMD;
        else if(cmd == 2)
            cmd = OPEN_CMD;
        else if(cmd == 3)
        {
            cmd = SETPERIOD_CMD;
            printf("please input timer period:\n");
            ret = scanf("%d",&data);
            if(ret != 1)
            {
                printf("get data error!\n");
                exit(-1);
            }
        }
        ret = ioctl(fd,cmd,data);
        if(ret < 0)
        {
            printf("ioctl file %s error!\n",argv[1]);
            exit(-1);
        }
    }

    ret = close(fd);
    if(ret < 0)
    {
        printf("close file %s error!\n",argv[1]);
        exit(-1);
    }
    return 0;
}