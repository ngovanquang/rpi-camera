
MODULE_NAME := motor_driver

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := stepper_driver.o
obj-m := $(MODULE_NAME).o

modules:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	
# Load the module to the Raspberry Pi
load:
	sudo insmod motor_driver.ko

# Remove the module from the Raspberry Pi
unload:
	sudo rmmod motor_driver
