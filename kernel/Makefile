obj-m += ram-disk.o


KERNEL_DIR=/home/rpease/Linux6


#  If we were cross-compiling we would add: 	ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \  to each of these
all:
	ARCH=x86_64 make -C $(KERNEL_DIR) M=$(shell pwd) modules

clean:
	make -C $(KERNEL_DIR) M=$(shell pwd) clean 



