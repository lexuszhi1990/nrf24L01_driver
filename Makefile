ifneq ($(KERNELRELEASE),)

obj-m := nrf_1.o

else
 
KDIR :=~/linux-kernel/opt/EmbedSky/linux-2.6.30.4

all:
	make -C $(KDIR) M=$(PWD) modules -Wall
	arm-linux-gcc -I $(KDIR) nrf_test.c -o nrf -Wall
	cp nrf nrf_1.ko ~/Shared/2440Shared/
clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers nrf

endif
