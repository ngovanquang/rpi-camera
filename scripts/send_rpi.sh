#!/bin/bash

cd ..

if [ -f "./build/h264capture" ] || [ -f "./build/srt_demo" ]; then

    echo ""
    echo "Sending executable file to Raspberry Pi"
    echo "----------------------------------"
    echo ""
    scp ./build/h264capture pi@raspberrypi.local:~/cross/h264capture
    scp ./build/srt_demo pi@raspberrypi.local:~/cross/srt_demo


else
    echo ""
    echo "Can not found excutable file"
    echo ""
fi