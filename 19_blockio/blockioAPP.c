#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

int main(int argc,char *argv[])
{
    int fd = 0;
    int ret = 0;
    int data = 0;

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
        ret = read(fd,&data,sizeof(data));
        if(ret == 0){
            printf("KEY0:%#x!\n",data);
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