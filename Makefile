ccflags-y += -I$(src)

ifneq ($(KERNELRELEASE),)
obj-m := simple_binder.o

else
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KERNEL_SRC) V=0 M=$$PWD
	g++ -o service_manager service_manager.cpp
	gcc -o binder_server binder_server.c
	gcc -o binder_client binder_client.c

clean:
	rm -rf *.o *.ko *.mod.c *.symvers *.order .*.cmd .tmp_versions *.mod service_manager binder_server binder_client
endif