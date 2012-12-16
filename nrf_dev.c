#include "nrf_dev.h"

/* interupt resgist */
static struct button_irq_desc button_irqs [] = {
    {IRQ_EINT18, IRQ, S3C2410_GPG10_EINT18, 0, "NRF"}, /* NRF */
};

//函数：uint8 SPI_RW(uint8 tmp)
//功能：NRF24L01的SPI写时序tmp
uint8 SPI_RW(uint8 tmp)
{
    uint8 bit_ctr;
    for(bit_ctr=0 ;bit_ctr<8 ;bit_ctr++) // output 8-bit
    {
        if(tmp & 0x80)         // output 'tmp', MSB to MOSI
            MOSI_H;
        else
            MOSI_L;

        tmp <<= 1;           // shift next bit into MSB..

        SCK_H;                   // Set SCK high..
        if(MISO_STU)
            tmp |= 0x01;          // capture current MISO bit
        SCK_L;                   // ..then set SCK low again
    }

    return(tmp);                    // return read tmp 
}


//函数：uint8 SPI_Read(uint8 reg)
//功能：NRF24L01的SPI时序
uint8 SPI_Read(uint8 reg)
{
    uint8 reg_val;

    CSN_L;         // CSN low, initialize SPI communication...
    ndelay(60);
    SPI_RW(reg);             // Select register to read from..
    reg_val = SPI_RW(READ_REG);     // ..then read registervalue
    CSN_H;            // CSN high, terminate SPI communication
    ndelay(60);

    return(reg_val);           // return register value
}



//功能：NRF24L01读写寄存器函数
uint8 SPI_RW_Reg(uint8 reg, uint8 value)
{
    uint8 status;  

    CE_L;
    CSN_L;                   // CSN low, init SPI transaction
    ndelay(60);
    status = SPI_RW(reg);      // select register
    SPI_RW(value);             // ..and write value to it..
    CSN_H;                   // CSN high again
    ndelay(60);

    return(status);            // return nRF24L01 status uint8
}



//函数：uint8 SPI_Read_Buf(uint8 reg, uint8 *pBuf, uint8 uchars)
//功能: 用于读数据，reg：为寄存器地址，pBuf：为待读出数据地址，uchars：读出数据的个数
uint8 SPI_Read_Buf(uint8 reg, uint8 *pBuf, uint8 uchars)
{
    uint8 status,uint8_ctr;

    CSN_L;                            // Set CSN low, init SPI tranaction
    ndelay(60);
    status = SPI_RW(reg);               // Select register to write to and read status uint8

    for(uint8_ctr = 0; uint8_ctr < uchars; uint8_ctr++)
    {
        pBuf[uint8_ctr] = SPI_RW(0);    // 
        ndelay(20);
    }

    CSN_H;
    ndelay(60);

    return(status);                    // return nRF24L01 status uint8
}


//函数：uint8 SPI_Write_Buf(uint8 reg, uint8 *pBuf, uint8 uchars)
//功能: 用于写数据：为寄存器地址，pBuf：为待写入数据地址，uchars：写入数据的个数
uint8 SPI_Write_Buf(uint8 reg, uint8 *pBuf, uint8 uchars)
{
    uint8 status,uint8_ctr;

    CSN_L;            //SPI使能  
    ndelay(60);
    status = SPI_RW(reg);   
    for(uint8_ctr=0; uint8_ctr<uchars; uint8_ctr++) {
        SPI_RW(*pBuf++);
        ndelay(20);
    }
    CSN_H;           //关闭SPI
    ndelay(60);
    return(status);    // 
}

/* 清空发送和接受fifo数据。同时清空中断 */
void nrf24L01_RegReset(void)
{
    SPI_RW_Reg(FLUSH_TX, 0x00);
    SPI_RW_Reg(FLUSH_RX, 0x00);
    SPI_RW_Reg(WRITE_REG + STATUS, 0xff);
}

//函数：void SetTX_Mode(void)
//功能：数据接收配置 
void SetTX_Mode(void)
{

    CE_L;
    ndelay(60);
    SPI_RW_Reg(WRITE_REG + CONFIG, 0x0e);    
    CE_H;
    udelay(130);
}

//函数：void SetRX_Mode(void)
//功能：数据接收配置 
void SetRX_Mode(void)
{

    CE_L;
    ndelay(60);
    SPI_RW_Reg(WRITE_REG + CONFIG, 0x0f);    // IRQ收发完成中断响应，16位CRC ，主接收
    udelay(1);
    CE_H;
    udelay(130);
}


/* shutdown the nrf2401 */
void nrf24l01_ShutDown(void)
{
    SPI_RW_Reg( WRITE_REG + CONFIG, 0x0);
    CE_L;
}

/* reWrite the data pipe */
void nrf24l01_pipe_write(unsigned long arg)
{
    if(arg > 255) {
        printk("failed to write DATA_CHANNEL...\n");
        return;
    }
    DATA_CHANNEL = (unsigned char)arg;
    printk("DATA_CHANNEL is %d\n", DATA_CHANNEL);
    return ;
}


static int nrf24l01_ioctl( struct inode *inode, struct file *file, 
        unsigned int cmd, unsigned long arg)
{

    switch(cmd)
    {
        case READ_STATUS:
            return (unsigned int)SPI_Read(STATUS);break;

        case READ_FIFO:
            return (unsigned int)SPI_Read(FIFO_STATUS);break;

        case WRITE_STATUS:
            return SPI_RW_Reg(WRITE_REG + STATUS, arg);break;

        case RX_FLUSH:
            return SPI_RW_Reg(FLUSH_RX, 0x00);  break;

        case TX_FLUSH:
            return SPI_RW_Reg(FLUSH_TX, 0x00);break;

        case WRITE_DATA_CHANNEL:
            nrf24l01_pipe_write(arg);break;

        case REG_RESET:
            nrf24L01_RegReset();break;

        case SHUTDOWN:
            nrf24l01_ShutDown();break;

        default:
            return -EINVAL;break;
    }
    return 0 ;
}

static irqreturn_t irq_interrupt(int irq, void *dev_id)
{
    struct button_irq_desc *button_irqs = (struct button_irq_desc *)dev_id;
    nrf24l01_irq++;

#ifdef DEBUG
    int down = (int)IRQ_STU;
    printk("sta 0x%x \n", SPI_Read(STATUS));
    printk("irq pin : 0x%x, irq time : 0x%d\n", down, nrf24l01_irq);
    if(down == IRQ_BIT) {
        /* it seems that it should not be this stat... */
    } else if(down == 0)
    {
        if(SPI_Read(STATUS) & RX_DR){
            printk("Receive OK! \n");
        } else if (SPI_Read(STATUS) & TX_DS) {
            printk("Send OK!... \n");
        } else if(SPI_Read(STATUS) & MAX_RT) {
            printk("Send failed \n");
        }
    }
    printk("irq : 0x%x, pin : 0x%x, pin-setting : 0x%x, number : 0x%x, name :%s\n ", button_irqs->irq, button_irqs->pin, button_irqs->pin_setting, button_irqs->number, button_irqs->name);
#endif

    /* 
     * it seems that it doesn't work... 
     */
    if(SPI_Read(STATUS) & MAX_RT) {
        SPI_RW_Reg(WRITE_REG + STATUS, MAX_RT);
    }
    if (strncmp("NRF", button_irqs->name, 3) == 0) {
        wake_up_interruptible(&button_waitq);
    }
    return IRQ_RETVAL(IRQ_HANDLED);

}

int nrf24l01_irq_init(void)
{
    int i, j;
    volatile int err;
    j = sizeof(button_irqs)/sizeof(button_irqs[0]);
    for (i = 0; i < j; i++)
    {
        if (button_irqs[i].irq < 0)
            continue;
        err = request_irq(button_irqs[i].irq, irq_interrupt, IRQ_TYPE_EDGE_FALLING, button_irqs[i].name, (void *)&button_irqs[i]);
        if (err)
            break;
    }

    if (err)
    {
        i--;
        for (; i >= 0; i--)
        {
            if (button_irqs[i].irq < 0)
                continue;
            disable_irq(button_irqs[i].irq);
            free_irq(button_irqs[i].irq, (void *)&button_irqs[i]);
        }
        return -EBUSY;
    }    


    return  1;
}

void nrf24l01_channel_init(void)
{
    // 写本地地址 
    SPI_Write_Buf(WRITE_REG + TX_ADDR, TX_ADDRESS_LIST[0], TX_ADR_WIDTH);
    // 写接收端地址0  设置接收数据长度，本次设置为5字节 
    SPI_Write_Buf(WRITE_REG + RX_ADDR_P0, RX_ADDRESS, RX_ADR_WIDTH);
    SPI_RW_Reg(WRITE_REG + RX_PW_P0, RX_PLOAD_WIDTH); 
    // 写接收端地址1
    SPI_Write_Buf(WRITE_REG + RX_ADDR_P1, RX_ADDRESS_P1, RX_ADR_WIDTH); 
    SPI_RW_Reg(WRITE_REG + RX_PW_P1, RX_PLOAD_WIDTH); 
    // 写接收端地址2
    SPI_RW_Reg(WRITE_REG + RX_ADDR_P2, RX_ADDRESS_P2);
    SPI_RW_Reg(WRITE_REG + RX_PW_P2, RX_PLOAD_WIDTH); 
    // 写接收端地址3
    SPI_RW_Reg(WRITE_REG + RX_ADDR_P3, RX_ADDRESS_P3);
    SPI_RW_Reg(WRITE_REG + RX_PW_P3, RX_PLOAD_WIDTH); 
    // 写接收端地址4
    SPI_RW_Reg(WRITE_REG + RX_ADDR_P4, RX_ADDRESS_P4);
    SPI_RW_Reg(WRITE_REG + RX_PW_P2, RX_PLOAD_WIDTH); 
    // 写接收端地址5
    SPI_RW_Reg(WRITE_REG + RX_ADDR_P5, RX_ADDRESS_P5);
    SPI_RW_Reg(WRITE_REG + RX_PW_P5, RX_PLOAD_WIDTH); 
}

/* NRF24L01初始化 */
uint8 init_NRF24L01(void)
{
    MISO_UP;

    CE_OUT;
    CSN_OUT;
    SCK_OUT;
    MOSI_OUT;
    MISO_IN;
    IRQ_IN;

    nrf24l01_irq_init();
    udelay(500);
    CE_L;    // chip enable
    ndelay(60);
    CSN_H;   // Spi disable 
    ndelay(60);
    SCK_L;   // Spi clock line init high
    ndelay(60);
    CE_L;    
    ndelay(60);

    SPI_RW_Reg(WRITE_REG + EN_AA, 0x3f);   
    SPI_RW_Reg(WRITE_REG + EN_RXADDR, 0x3f);
    nrf24l01_channel_init();
    SPI_RW_Reg(WRITE_REG + RF_CH, 0);  //设置信道工作为2.4GHZ，收发必须一致
    SPI_RW_Reg(WRITE_REG + SETUP_AW, 0x02);
    SPI_RW_Reg(WRITE_REG + SETUP_RETR, 0x1a);
    SPI_RW_Reg(WRITE_REG + RF_SETUP, 0x07);         //设置发射速率为1MHZ，发射功率为最大值0dB
    SPI_RW_Reg(WRITE_REG + CONFIG, 0x0f);           // IRQ收发完成中断响应，16位CRC ，主接收

    mdelay(10);
    return (1);
}


//函数：void nRF24L01_TxPacket(unsigned char * tx_buf)
//功能：发送 tx_buf中数据
void nRF24L01_TxPacket(unsigned char * tx_buf)
{
    CE_L;           //StandBy I模式 
    ndelay(60);
    SPI_Write_Buf(WRITE_REG + TX_ADDR, TX_ADDRESS_LIST[DATA_CHANNEL], TX_ADR_WIDTH);
    SPI_Write_Buf(WRITE_REG + RX_ADDR_P0, TX_ADDRESS_LIST[DATA_CHANNEL], TX_ADR_WIDTH); // 装载接收端地址
    SPI_Write_Buf(WR_TX_PLOAD, tx_buf, TX_PLOAD_WIDTH);              // 装载数据 
    SetTX_Mode();
}


//函数：unsigned char nRF24L01_RxPacket(unsigned char* rx_buf)
//功能：数据读取后放如rx_buf接收缓冲区中
unsigned char nRF24L01_RxPacket(unsigned char* rx_buf)
{
    unsigned char revale = 0;
    unsigned char sta = 0;

    // 读取状态寄存其来判断数据接收状况
    sta = SPI_Read(STATUS);
    printk("before read  status: 0x%x\n", sta);
    if(sta & (RX_DR))     // 判断是否接收到数据
    {
        CE_L;             //SPI使能
        udelay(50);
        SPI_Read_Buf(RD_RX_PLOAD, rx_buf, TX_PLOAD_WIDTH);// read receive payload from RX_FIFO buffer
        SPI_RW_Reg(WRITE_REG+STATUS, RX_DR); //接收到数据后RX_DR,TX_DS,MAX_PT都置高为1，通过写1来清楚中断标志
        revale = 1;          //读取数据完成标志
    }
    printk("after read  status: 0x%x\n", SPI_Read(STATUS));
    return revale;
}

//读函数
static ssize_t nrf24l01_read(struct file *filp, char __user *buffer, size_t count, loff_t *ppos) 
{ 
    printk("come to read...\n");
    if (nRF24L01_RxPacket(RxBuf)) 
    {
        if( copy_to_user( buffer, RxBuf, count))  
        {
            printk("Can't Copy Data !");
            return -EFAULT;
        }
        return (sizeof(RxBuf));
    }

    return 0;
}

//文件的写函数
static ssize_t nrf24l01_write(struct file *filp, const char __user *buffer, size_t count, loff_t *ppos)
{
    //从内核空间复制到用户空间
    if( copy_from_user( TxBuf, buffer, count ) )
    {
        printk("Can't Copy Data !");
        return -EFAULT;
    }
    nRF24L01_TxPacket(TxBuf);
    return(sizeof(TxBuf));
}

/* 文件的打开 */
static int nrf24l01_open(struct inode *node, struct file *file)
{
    uint8 flag = 0;

    if(opencount > 0)
        return -EBUSY;

    flag = init_NRF24L01();

    mdelay(100);
    if(flag == 0)
    {
        printk("uable to open device!\n");
        return -1;
    }
    else
    {
        opencount++;
        printk("device opened !\n");
        return 0;
    }
}

static unsigned int nrf24l01_poll( struct file *file, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    if (SPI_Read(FIFO_STATUS) & 0x2) {
        SPI_RW_Reg(FLUSH_RX, 0x00);
    }
    printk("fifo statment 0x%x\n ", SPI_Read(FIFO_STATUS));
    printk("sta 0x%x \n", SPI_Read(STATUS));
    SetRX_Mode();
    poll_wait(file, &button_waitq, wait);
    if (SPI_Read(STATUS) & RX_DR) {
        DATA_PIPE =  ((SPI_Read(STATUS) & 0x0e ) >> 1 );
        printk("it Receive from channel: %d\n", DATA_PIPE);
        mask |= ( DATA_PIPE << 4);
        mask |= POLLIN;
    } 
    if (SPI_Read(STATUS) & TX_DS) {
        printk("it Send OK... \n");
        mask |= POLLOUT;
        SPI_RW_Reg(WRITE_REG + STATUS, TX_DS);
    } 
    if (SPI_Read(STATUS) & MAX_RT) {
        SPI_RW_Reg(WRITE_REG + STATUS, MAX_RT);
        printk("it Send failed\n");
        mask |= POLLERR;
    }

    SetRX_Mode();
    printk("mask = 0x%x, out = 0x%x, in=0x%x, err=%x\n", 
            mask, POLLOUT, POLLIN, POLLERR);
    return mask;
}



static int nrf24l01_release(struct inode *node, struct file *file)
{
    int i;
    int j = sizeof(button_irqs)/sizeof(button_irqs[0]);
    for (i = 0; i < j; i++)
    {
        if (button_irqs[i].irq < 0)
            continue;
        free_irq(button_irqs[i].irq, (void *)&button_irqs[i]);
    }
    nrf24l01_ShutDown();
    opencount--;
    printk(DEVICE_NAME " released !\n");
    return 0;
}

static struct file_operations nrf24l01_fops = {
    .owner = THIS_MODULE,
    .open = nrf24l01_open,
    .write = nrf24l01_write,
    .read = nrf24l01_read,
    .poll = nrf24l01_poll,
    .ioctl = nrf24l01_ioctl,
    .release = nrf24l01_release,
};

static struct miscdevice misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &nrf24l01_fops,
};

static int __init nrf24l01_init(void)
{
    int ret;

    printk("Initial driver for NRF24L01......................\n");

    ret = misc_register(&misc);

    printk (DEVICE_NAME" initialized\n");
    return ret ;
}

static void __exit nrf24l01_exit(void)
{
    misc_deregister(&misc);
    printk("NRF24L01 unregister success \n");
}


module_init(nrf24l01_init);
module_exit(nrf24l01_exit);
MODULE_AUTHOR("jammy_lee@163.com");
MODULE_DESCRIPTION("nrf24l01 driver for TQ2440");
MODULE_LICENSE("GPL");
