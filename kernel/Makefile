obj-m += ram-disk-bio.o ram-disk-queue.o


KERNEL_DIR=/lib/modules/`uname -r`/build


#  If we were cross-compiling we would add: 	ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \  to each of these

all:
	ARCH=x86_64 make -C $(KERNEL_DIR) M=$(shell pwd) modules
	@echo
	@echo "     % ln -s ram-disk-bio.ko ram-disk.ko   (to use the bio-only version)  "
	@echo "     % ln -s ram-disk-queue.ko ram-disk.ko   (to use the queue version)  "
	@echo

clean:
	make -C $(KERNEL_DIR) M=$(shell pwd) clean 



