
obj-m		:= cryptochr.o 
KERN_SRC	:= /usr/src/kernels/$(shell uname -r)
PWD		:= $(shell pwd)
CMOD_NAME	:= cryptochr.ko

modules:
	make -C $(KERN_SRC) M=$(PWD) modules

install:
	make -C $(KERN_SRC) M=$(PWD) modules_install
	depmod -a
	
load:
	insmod $(CMOD_NAME)

unload:
	rmmod $(CMOD_NAME)
	
clean:
	make -C $(KERN_SRC) M=$(PWD) clean
