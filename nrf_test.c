#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>

#define Bufsize 5
#define EpSize 128
#define MAX_EVENTS 128


/* ioctl 相关命令 */
#define READ_STATUS   0x0011
#define READ_FIFO     0x0012
#define WRITE_STATUS  0x0211
#define RX_FLUSH      0x0300
#define TX_FLUSH      0x0310
#define SHUTDOWN      0x0900


char TxBuf[Bufsize] = {0x05, 0x06, 0x07, 0x08, 0x09};
char RxBuf[Bufsize] = {0};

int main(void)
{
    int i, count = 1, ret;
    int ep_fd, nrf_fd, nr_events;
    struct epoll_event ep_event, *events;

    nrf_fd = open("/dev/nrf24l01", O_RDWR);  
    if(nrf_fd < 0)
    {
        perror("Can't open /dev/nrf24l01 \n");
        exit(1);
    }
    printf("open /dev/nrf24l01 success. fd is %x \n", nrf_fd);

#if 0
    while (1) 
    {
        printf("fifo %x\n", ioctl(nrf_fd, READ_FIFO, NULL));
        printf("status %x\n", ioctl(nrf_fd, READ_STATUS, NULL));
        ioctl(nrf_fd, RX_FLUSH, NULL);
        if (ioctl(nrf_fd, READ_FIFO, NULL) & 0x12) 
        {
            ioctl(nrf_fd, TX_FLUSH, NULL);
        }

        if (ioctl(nrf_fd, READ_STATUS, NULL) == 0x1e) 
        {
            ioctl(nrf_fd, WRITE_STATUS, 0x10);
        }
        read(nrf_fd, RxBuf, Bufsize);
        printf("rxbuf %s\n", RxBuf);
        sleep(10);
    }


#else
    ioctl(nrf_fd, WRITE_STATUS, 0xff);
    ep_fd = epoll_create(EpSize);
    if (ep_fd < 0) {
        perror("failed to epoll create");
        exit(1);
    }

    ep_event.data.fd = nrf_fd;
    ep_event.events = EPOLLIN;
    ret = epoll_ctl(ep_fd, EPOLL_CTL_ADD, nrf_fd, &ep_event);
    if (ret) {
        perror("failed to epoll_ctl");
        exit(1);
    }

    events = malloc(sizeof(struct epoll_event) * MAX_EVENTS);
    if (!events) {
        perror("failed to malloc events");
        exit(1);
    }

    while(1) {
        nr_events = epoll_wait(ep_fd, events, MAX_EVENTS, -1);
        if (nr_events < 0) {
            perror("failed to wait");
            free(events);
            break;
        }
        for (i = 0; i < nr_events; i++) {
            printf("event= 0x%x, on fd = 0x%x\n", 
                    events[i].events, events[i].data.fd);

        }
        if(nr_events & POLLIN) {
            printf("have a msg to read\n");
            read(nrf_fd, RxBuf, Bufsize);
            printf("rxbuf %s\n", RxBuf);
        }
        write(nrf_fd, TxBuf, strlen(TxBuf));
        printf("one round is over\n");
        sleep(4);
    }

    free(events);
#endif
    close(nrf_fd);
    return 0;
}


