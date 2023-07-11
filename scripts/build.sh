#!/bin/bash

type="INDOOR_TYPE"

fw_version=1.0
fw_name=RPICAM

while getopts t:n:v: flag
do
    case "${flag}" in
        t) type=${OPTARG};;
        n) fw_name=${OPTARG};;
        v) fw_version=${OPTARG};;
    esac
done

echo "IPC TYPE: $type"
echo "FW Name: $fw_name"
echo "FW Version: $fw_version"

cd ..
rm -rf ./build/*

cmake -S . -Bbuild -DIPC_TYPE=$type && cmake --build build

if [ -f "./build/h264capture" ] || [ -f "./build/srt_demo" ]; then

    echo ""
    echo "Build for $type: SUCCESS"
    echo "----------------------------------"
    echo ""


else
    echo "Error: Can not find file build/h264capture"
fi