obj-m += ram-disk-bio.o ram-disk-queue.o


# To compile on your host system
KERNEL_DIR=/lib/modules/`uname -r`/build
# To compile for a qemu version (if different)
#KERNEL_DIR=/home/rpease/Linux6


#  If we were cross-compiling we would add: 	ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \  to each of these

all:
	ARCH=x86_64 make -C $(KERNEL_DIR) M=$(shell pwd) modules

clean:
	make -C $(KERNEL_DIR) M=$(shell pwd) clean 



