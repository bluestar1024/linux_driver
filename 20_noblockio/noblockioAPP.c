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

int main(int argc,char *argv[])
{
    int fd = 0;
    int ret = 0;
    int data = 0;
#if 0
    fd_set readfds;
    struct timeval timeout;
#endif
    struct pollfd fds[1];
    int timeout;

    if(argc != 2)
    {
        printf("usage error!\n");
        exit(-1);
    }

    fd = open(argv[1],O_RDWR|O_NONBLOCK);
    if(fd < 0)
    {
        printf("open file %s error!\n",argv[1]);
        exit(-1);
    }
#if 0
    while(1)
    {
        FD_ZERO(&readfds);
        FD_SET(fd,&readfds);
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        ret = select(fd+1,&readfds,NULL,NULL,&timeout);
        switch(ret)
        {
            case 0: printf("timeout,device not ready!\n"); 
                    break;
            case -1:printf("select error!\n");
                    break;
            default:if(FD_ISSET(fd,&readfds))
                    {
                        ret = read(fd,&data,sizeof(data));
                        if(ret == 0){
                            printf("KEY0:%#x!\n",data);
                        }
                    }
                    break;
        }
    }
#endif
#if 1
    fds[0].fd = fd;
    fds[0].events = POLLIN;
    timeout = 10000;
    while(1)
    {
        ret = poll(fds,1,timeout);
        switch(ret)
        {
            case 0: printf("timeout,device not ready!\n");
                    break;
            case -1:printf("poll error!\n");
                    break;
            default:if(fds[0].events & fds[0].revents)
                    {
                        ret = read(fd,&data,sizeof(data));
                        if(ret == 0){
                            printf("KEY0:%#x!\n",data);
                        }
                    }
                    break;
        }
    }
#endif
    ret = close(fd);
    if(ret < 0)
    {
        printf("close file %s error!\n",argv[1]);
        exit(-1);
    }
    return 0;
}