KERNEL_PATH := /home/dong/linux/IMX6ULL/linux_srccode/alientek_linux/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek
CURRENT_PATH := $(shell pwd)
INSTALL_PATH := /home/dong/linux/nfs/rootfs/lib/modules/4.1.15/

MODULE_NAME1 := platleddrv
MODULE_NAME2 := platleddev
APP_NAME := platledAPP

obj-m = $(MODULE_NAME1).o
obj-m += $(MODULE_NAME2).o

kernel_module:
	$(MAKE) -C $(KERNEL_PATH) M=$(CURRENT_PATH) modules
	arm-linux-gnueabihf-gcc $(APP_NAME).c -o $(APP_NAME)
clean:
	$(MAKE) -C $(KERNEL_PATH) M=$(CURRENT_PATH) clean
	rm -rf $(APP_NAME)
install:
	cp -raf $(MODULE_NAME1).ko $(MODULE_NAME2).ko $(APP_NAME) $(INSTALL_PATH)