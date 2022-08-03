#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char *argv[])
{
    int fd = 0;
    int ret = 0;
    char writebuf[50],readbuf[50];
    char userdata[] = {"user data!"};

    if(argc != 3)
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

    if(atoi(argv[2]) == 1)
    {
        memcpy(writebuf,userdata,sizeof(userdata));
        ret = write(fd,writebuf,strlen(writebuf));
        if(ret < 0)
        {
            printf("write file %s error!\n",argv[1]);
            exit(-1);
        }
        printf("user write count:%d!\n",ret);
    }

    if(atoi(argv[2]) == 2)
    {
        ret = read(fd,readbuf,sizeof(readbuf));
        if(ret < 0)
        {
            printf("read file %s error!\n",argv[1]);
            exit(-1);
        }
        printf("APP read data:%s\n",readbuf);
        printf("user read count:%d!\n",ret);
    }

    ret = close(fd);
    if(ret < 0)
    {
        printf("close file %s error!\n",argv[1]);
        exit(-1);
    }
    return 0;
}