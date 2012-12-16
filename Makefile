ifneq ($(KERNELRELEASE),)

obj-m := nrf_dev.o

else
 
KDIR :=~/linux-kernel/opt/EmbedSky/linux-2.6.30.4

all:
	make -C $(KDIR) M=$(PWD) modules -Wall
	arm-linux-gcc nrf_test.c -o nrf
	cp nrf_dev.ko nrf  ~/Shared/2440Shared/
clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers nrf

endif
