obj-m += neopixel_driver.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

# Load the module to the Raspberry Pi
load:
	sudo insmod neopixel_driver.ko

# Remove the module from the Raspberry Pi
unload:
	sudo rmmod neopixel_driver
