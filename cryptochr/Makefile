
obj-m		:= cryptochr.o 
KERN_SRC	:= /usr/src/linux-headers-$(shell uname -r)
PWD		:= $(shell pwd)
CMOD_NAME	:= cryptochr.ko

modules:
	make -C $(KERN_SRC) M=$(PWD) modules

install:
	make -C $(KERN_SRC) M=$(PWD) modules_install
	depmod -a
	
load:
	insmod $(CMOD_NAME) message_life_timeout=10000

unload:
	rmmod $(CMOD_NAME)
	
clean:
	make -C $(KERN_SRC) M=$(PWD) clean
	
