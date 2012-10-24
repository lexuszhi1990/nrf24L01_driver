/*
 * SPI testing utility (using spidev driver) 
 * 
 * Copyright (c) 2007  MontaVista Software, Inc. 
 * Copyright (c) 2007  Anton Vorontsov <avorontsov@ru.mvista.com> 
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License. 
 * 
 * Cross-compile with cross-gcc -I/path/to/cross-kernel/include 
 */  
  
#include <linux/spi/spidev.h>
#include "nrf_2.h"
  
static void pabort(const char *s)  
{  
        perror(s);  
        exit(1);
}  
          
static const char *device = "/dev/spidev0.0";  
static uint8_t mode;  
static uint8_t bits = 8;  
static uint32_t speed = 500000;  
static uint16_t delay;  

int nrf_fd;  
uint8_t tx[1] = {-1 }; 
uint8_t rx[1] = {-1 };  

uint8_t spi_transfer(uint8_t data)
{
        int ret;  
        tx[0] = data;
        struct spi_ioc_transfer tr = {  
                .tx_buf = (unsigned long)tx,  
                .rx_buf = (unsigned long)rx,  
                .len = 1,  
                .delay_usecs = delay,  
                .speed_hz = speed,  
                .bits_per_word = bits,  
        };  
  
        ret = ioctl(nrf_fd, SPI_IOC_MESSAGE(1), &tr);  
        if (ret == 1)  
                pabort("can't send spi message");  
  
       return rx[0];
}
 
int spi_init(void)  
{  
        int ret = 0;  
  
  
        nrf_fd = open(device, O_RDWR);  
        if (nrf_fd < 0)  
                pabort("can't open device");  
  
        /* 
         * spi mode 
         */  
        ret = ioctl(nrf_fd, SPI_IOC_WR_MODE, &mode);  
        if (ret == -1)  
                pabort("can't set spi write mode");  
  
        ret = ioctl(nrf_fd, SPI_IOC_RD_MODE, &mode);  
        if (ret == -1)  
                pabort("can't get spi read mode");  
  
        /* 
         * bits per word 
         */  
        ret = ioctl(nrf_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);  
        if (ret == -1)  
                pabort("can't set bits per word");  
  
        ret = ioctl(nrf_fd, SPI_IOC_RD_BITS_PER_WORD, &bits);  
        if (ret == -1)  
                pabort("can't get bits per word");  
  
        /* 
         * max speed hz 
         */  
        ret = ioctl(nrf_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);  
        if (ret == -1)  
                pabort("can't set max speed hz");  
  
        ret = ioctl(nrf_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);  
        if (ret == -1)  
                pabort("can't get max speed hz");  
  
        printk("spi mode: %d\n", mode);  
        printk("bits per word: %d\n", bits);  
        printk("max speed: %d Hz (%d KHz)\n", speed, speed/1000);  
  
        return ret;  
}  

void init_exit(void)
{
    close(nrf_fd);  
}
