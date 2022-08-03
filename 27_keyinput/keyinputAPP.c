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
    struct input_event event;

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
        ret = read(fd,&event,sizeof(struct input_event));
        if(ret > 0)
        {
            switch(event.type)
            {
                case EV_SYN:
                    printf("SYN event!\n");
                    printf("value:%d!\n",event.value);
                    break;
                case EV_KEY:
                    if(event.code < BTN_MISC)
                        printf("KEY%d:%s!\n",event.code,event.value?"press":"release");
                    else
                        printf("BUTTON%d:%s!\n",event.code,event.value?"press":"release");
                    printf("value:%d!\n",event.value);
                    break;
            }
        }
        else
        {
            printf("read file %s error!\n",argv[1]);
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