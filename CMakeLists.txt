cmake_minimum_required(VERSION 3.21)
project(rpi-camera)

#--------------------------------------------------------------------------
message(STATUS "IPC_TYPE: ${IPC_TYPE}")

# enable one of following flags ON

if(${IPC_TYPE} STREQUAL "INDOOR_TYPE")
        set(PLATFORM_IPC_MEARI_RPI_INDOOR   ON)     #rpi arm
        set(PLATFORM_IPC_MEARI_OTHER_OUTDOOR    OFF)
elseif(${IPC_TYPE} STREQUAL "OUTDOOR_TYPE")
        set(PLATFORM_IPC_MEARI_RPI_INDOOR   OFF)     #rpi arm
        set(PLATFORM_IPC_MEARI_OTHER_OUTDOOR    ON)
endif()

#--------------------------------------------------------------------------
if(${PLATFORM_IPC_MEARI_RPI_INDOOR} AND ${PLATFORM_IPC_MEARI_OTHER_OUTDOOR})
        message(FATAL_ERROR "Please choose only one option building for Indoor or Outdoor")
endif()

# Toolchain for RPI
if(${PLATFORM_IPC_MEARI_RPI_INDOOR})
        message(STATUS "Compile for Meari Indoor IPC (Raspberry Pi)")

        set(CMAKE_C_STANDARD 99)
        set(CMAKE_C_COMPILER    "/opt/cross-pi-gcc/bin/arm-linux-gnueabihf-gcc")
        set(CMAKE_CXX_COMPILER    "/opt/cross-pi-gcc/bin/arm-linux-gnueabihf-g++")

        set(PLATFORM raspberry_pi)

        add_definitions(-DINDOOR_TYPE)

# Toolchain for RPI
elseif(${PLATFORM_IPC_MEARI_OTHER_OUTDOOR})
        message(STATUS "Compile for Meari Outdoor IPC (Other chipset)")

        set(CMAKE_C_STANDARD 99)
        set(CMAKE_C_COMPILER    "/opt/arm")
        set(CMAKE_CXX_COMPILER    "/opt/arm")

        set(PLATFORM other_chipset)

        add_definitions(-DOUTDOOR_TYPE)

else()

        message(FATAL_ERROR "You do not choose toolchain")

endif()

#--------------------------------------------------------------------------
#Libraries
set(LIBS_ROOT_DIR ${CMAKE_SOURCE_DIR}/libs)
set(LIBS_RPI_ROOT_DIR    ${LIBS_ROOT_DIR}/meari/raspberry)

if(${PLATFORM_IPC_MEARI_RPI_INDOOR})
        add_library(libsrt          STATIC  IMPORTED)
        
        set_property(TARGET libsrt          PROPERTY IMPORTED_LOCATION  "${LIBS_RPI_ROOT_DIR}/libsrt.a")
        link_directories(${LIBS_RPI_ROOT_DIR}/)

endif()

include_directories("../src")
include_directories("../src/srt/")

#
add_executable(h264capture
        src/h264_capture.c src/h264_capture.h)

if(${PLATFORM_IPC_MEARI_RPI_INDOOR})
target_link_libraries(h264capture
        pthread

) 

endif()

target_include_directories(h264capture PRIVATE
        src)