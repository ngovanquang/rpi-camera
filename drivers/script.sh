#!/bin/bash
# ssh pi@raspberrypi.local "mkdir ~/drivers"
# ssh pi@raspberrypi.local "mkdir ~/drivers/neopixel"
# ssh pi@raspberrypi.local "mkdir ~/drivers/ptz"
# ssh pi@raspberrypi.local "rm -rf ~/drivers/stepper_motor/*"
ssh pi@raspberrypi.local "rm -rf ~/drivers/ptz/*"
scp -r ./motor/* pi@raspberrypi.local:~/drivers/ptz
# ssh pi@raspberrypi.local "rm -rf ~/drivers/neopixel/*"
# scp ./neopixel/* pi@raspberrypi.local:~/drivers/neopixel