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


/* ioctl 相关命令 */
#define READ_STATUS                0x0011
#define READ_FIFO                  0x0012
#define WRITE_STATUS               0x0211
#define RX_FLUSH                   0x0300
#define TX_FLUSH                   0x0310
#define WRITE_DATA_CHANNEL         0x0400
#define REG_RESET                  0x8000
#define SHUTDOWN                   0x0900

#define Bufsize 5
#define EpSize 128
#define MAX_EVENTS 128

static int nrf_fd;
volatile unsigned char data_pipe = 0;
volatile unsigned char data_channel = 0;
unsigned char TxBuf[Bufsize] = {0x05, 0x06, 0x07, 0x08, 0x09};
unsigned char RxBuf[Bufsize] = {0};


/*  在RxBuf中最后一个字节，也就是第4个字节，存放的是发送端的编号，
 *  从这个编号，便可在TX_ADDRESS_LIST中查找出对应的发送端地址
 */
void nrf2401_send()
{
    data_channel = RxBuf[Bufsize - 1];
    printf("receive channel is %d\n", data_channel);
    ioctl(nrf_fd, WRITE_DATA_CHANNEL, data_pipe);
    write(nrf_fd, TxBuf, Bufsize);
}



void nrf24L01_ctrl(int data)
{
    
    switch ( data & 0x0f) {
        case POLLIN:
            data_pipe = ( data & 0xf0 ) >> 4;
            printf("have a msg to read from pipe %d...\n", data_pipe);
            read(nrf_fd, RxBuf, Bufsize);
            printf("1 > %x, 2 > %x, 3 > %x \n", RxBuf[0], RxBuf[1], RxBuf[2]);
//            nrf2401_send();
            break;
        case POLLOUT:
            printf("send ok...\n");
            break;
        case POLLERR:
            printf("when sended, a error appeared...\n");
            break;
        default :
            break;
    }  

    return ;
}

int main(void)
{
    int i, ret;
    int ep_fd, nr_events;
    struct epoll_event ep_event, *events;

    nrf_fd = open("/dev/nrf24l01", O_RDWR);  
    if(nrf_fd < 0)
    {
        perror("Can't open /dev/nrf24l01 \n");
        exit(1);
    }
    printf("open /dev/nrf24l01 success. fd is %x \n", nrf_fd);

    ioctl(nrf_fd, REG_RESET, NULL);

    ep_fd = epoll_create(EpSize);
    if (ep_fd < 0) {
        perror("failed to epoll create");
        exit(1);
    }

    ep_event.data.fd = nrf_fd;
    ep_event.events = EPOLLIN | EPOLLOUT;
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

    write(nrf_fd, TxBuf, Bufsize);
    while(1) {
        nr_events = epoll_wait(ep_fd, events, MAX_EVENTS, -1);
        printf("return mask = %d\n", nr_events);
        if (nr_events < 0) {
            perror("failed to wait");
            free(events);
            break;
        }
        for (i = 0; i < nr_events; i++) {
            printf("event= 0x%x, on fd = 0x%x\n", 
                    events[i].events, events[i].data.fd);
            if (events[i].data.fd == nrf_fd) 
            {
                nrf24L01_ctrl(events[i].events);
            }
        }
        printf("one round is over\n");
    }

    free(events);
    close(nrf_fd);


    return 0;
}


