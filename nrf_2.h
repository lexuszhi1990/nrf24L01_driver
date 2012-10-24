#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <mach/regs-gpio.h>
#include <mach/hardware.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/ioctl.h>
#include <asm/unistd.h>
#include <linux/interrupt.h>

typedef unsigned int uint16 ;
typedef unsigned char uint8 ;

//和引脚相关的宏定义
#define CE        S3C2410_GPG3
#define CE_OUTP       S3C2410_GPG3_OUTP
#define CSN       S3C2410_GPF3
#define CSN_OUTP      S3C2410_GPF3_OUTP
#define IRQ       S3C2410_GPB0
#define IRQ_INP       S3C2410_GPB0_INP

#define DEVICE_NAME     "nrf24l01" //设备名称，在可以 /proc/devices 查看
#define NRF24L01_MAJOR   258  //主设备号

//NRF24L01端口定义
#define CE_OUT  s3c2410_gpio_cfgpin(CE, CE_OUTP)  //数据线设置为输出 
#define CE_UP        s3c2410_gpio_pullup(CE, 1)        //打开上拉电阻
#define CE_L  s3c2410_gpio_setpin(CE, 0)   //拉低数据线电平 
#define CE_H  s3c2410_gpio_setpin(CE, 1)   //拉高数据线电平

#define IRQ_IN  s3c2410_gpio_cfgpin(IRQ, IRQ_INP) //数据线设置为输入
#define IRQ_UP        s3c2410_gpio_pullup(IRQ, 1)        //打开上拉电阻
#define IRQ_STU  s3c2410_gpio_getpin(IRQ)   //获取中断数据 
#define IRQ_BIT       ( 1 << 10 )

#define CSN_OUT  s3c2410_gpio_cfgpin(CSN, CSN_OUTP) //数据线设置为输出 
#define CSN_UP        s3c2410_gpio_pullup(CSN, 1)        //打开上拉电阻
#define CSN_L  s3c2410_gpio_setpin(CSN, 0)  //拉低数据线电平 
#define CSN_H  s3c2410_gpio_setpin(CSN, 1)  //拉高数据线电平 



//NRF24L01寄存器指令
#define READ_REG        0x00    // 读寄存器指令
#define WRITE_REG       0x20    // 写寄存器指令
#define RD_RX_PLOAD     0x61    // 读取接收数据指令
#define WR_TX_PLOAD     0xA0    // 写待发数据指令
#define FLUSH_TX        0xE1    // 冲洗发送 FIFO指令
#define FLUSH_RX        0xE2    // 冲洗接收 FIFO指令
#define REUSE_TX_PL     0xE3    // 定义重复装载数据指令
#define NOP             0xFF    // 保留


//SPI(nRF24L01)寄存器地址
#define CONFIG          0x00  // 配置收发状态，CRC校验模式以及收发状态响应方式
#define EN_AA           0x01  // 自动应答功能设置
#define EN_RXADDR       0x02  // 可用信道设置
#define SETUP_AW        0x03  // 收发地址宽度设置
#define SETUP_RETR      0x04  // 自动重发功能设置
#define RF_CH           0x05  // 工作频率设置
#define RF_SETUP        0x06  // 发射速率、功耗功能设置
#define STATUS          0x07  // 状态寄存器
#define OBSERVE_TX      0x08  // 发送监测功能
#define CD              0x09  // 地址检测           
#define RX_ADDR_P0      0x0A  // 频道0接收数据地址
#define RX_ADDR_P1      0x0B  // 频道1接收数据地址
#define RX_ADDR_P2      0x0C  // 频道2接收数据地址
#define RX_ADDR_P3      0x0D  // 频道3接收数据地址
#define RX_ADDR_P4      0x0E  // 频道4接收数据地址
#define RX_ADDR_P5      0x0F  // 频道5接收数据地址
#define TX_ADDR         0x10  // 发送地址寄存器
#define RX_PW_P0        0x11  // 接收频道0接收数据长度
#define RX_PW_P1        0x12  // 接收频道0接收数据长度
#define RX_PW_P2        0x13  // 接收频道0接收数据长度
#define RX_PW_P3        0x14  // 接收频道0接收数据长度
#define RX_PW_P4        0x15  // 接收频道0接收数据长度
#define RX_PW_P5        0x16  // 接收频道0接收数据长度
#define FIFO_STATUS     0x17  // FIFO栈入栈出状态寄存器设置


/* nrf24l01 ioctl 相关命令 */
#define READ_STATUS   0x0011
#define READ_FIFO     0x0012
#define WRITE_STATUS  0x0211
#define RX_FLUSH      0x0300
#define TX_FLUSH      0x0310
#define SHUTDOWN      0x0900

uint8 init_NRF24L01(void);
uint8 SPI_RW(uint8 tmp);
uint8 SPI_Read(uint8 reg);
void  SetRX_Mode(void);
void  SetTX_Mode(void);
uint8 SPI_RW_Reg(uint8 reg, uint8 value);
uint8 SPI_Read_Buf(uint8 reg, uint8 *pBuf, uint8 uchars);
uint8 SPI_Write_Buf(uint8 reg, uint8 *pBuf, uint8 uchars);
unsigned char nRF24L01_RxPacket(unsigned char* rx_buf);
void nRF24L01_TxPacket(unsigned char * tx_buf);

uint8 spi_transfer(uint8 data);
int spi_init(void);
void spi_exit(void);

#define DeBug 1
#define NRF24L01_SPI 1
