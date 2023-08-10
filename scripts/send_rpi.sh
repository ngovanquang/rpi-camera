#!/bin/bash

cd ..

if [ -f "./build/h264capture" ]; then

    echo ""
    echo "Sending executable file to Raspberry Pi"
    echo "----------------------------------"
    echo ""
    # scp ./build/h264capture pi@raspberrypi.local:~/cross/h264capture

    if  [ "$1" == "4" ]; then
        echo "raspberry pi 4"
        scp ./build/h264capture pi@raspberrypi4.local:~/cross/h264capture
    else
        echo "raspberry pi Zero"
        scp ./build/h264capture pi@raspberrypi.local:~/cross/h264capture
    fi
else
    echo ""
    echo "Can not found excutable file"
    echo ""
fi