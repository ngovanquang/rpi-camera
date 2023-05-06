#!/bin/bash

cd ..

if [ -f "./build/h264capture" ]; then

    echo ""
    echo "Sending executable file to Raspberry Pi"
    echo "----------------------------------"
    echo ""
    scp ./build/h264capture pi@raspberrypi.local:~/cross/h264capture


else
    echo ""
    echo "Can not found excutable file"
    echo ""
fi