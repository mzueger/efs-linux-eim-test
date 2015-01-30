# ========================================================
# File        : Makefile
# Author      : martin.zueger@ntb.ch
# Date        : 2015-01-27
# License     : GPLv2
# Description : Makefile for building the EIM test module
# ========================================================

ifeq ($(KERNELRELEASE),)
	KERNELDIR ?= ~/Projects/efs/linux-toradex
	PWD := $(shell pwd)

modules:
	$(MAKE) ARCH=arm CROSS_COMPILE=~/Projects/efs/buildroot/output/host/usr/bin/arm-buildroot-linux-uclibcgnueabihf- -C $(KERNELDIR) M=$(PWD) modules
	
copy2board:
	scp eim_char.ko root@146.136.39.224:/tmp/
	
clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

.PHONY: modules copy2board clean

else
    obj-m := eim_char.o
endif
