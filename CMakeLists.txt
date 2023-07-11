cmake_minimum_required(VERSION 3.21)
project(rpi-camera)

#--------------------------------------------------------------------------
message(STATUS "IPC_TYPE: ${IPC_TYPE}")

# enable one of following flags ON

if(${IPC_TYPE} STREQUAL "INDOOR_TYPE")
        set(PLATFORM_IPC_MEARI_RPI_INDOOR       ON)     #rpi arm
        set(PLATFORM_IPC_MEARI_OTHER_OUTDOOR    OFF)
elseif(${IPC_TYPE} STREQUAL "OUTDOOR_TYPE")
        set(PLATFORM_IPC_MEARI_RPI_INDOOR       OFF)     #rpi arm
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
        set(CMAKE_C_COMPILER            "/opt/cross-pi-gcc/bin/arm-linux-gnueabihf-gcc")
        set(CMAKE_CXX_COMPILER          "/opt/cross-pi-gcc/bin/arm-linux-gnueabihf-g++")

        set(PLATFORM raspberry_pi)

        add_definitions(-DINDOOR_TYPE)

# Toolchain for RPI
elseif(${PLATFORM_IPC_MEARI_OTHER_OUTDOOR})
        message(STATUS "Compile for Meari Outdoor IPC (Other chipset)")

        set(CMAKE_C_STANDARD 99)
        set(CMAKE_C_COMPILER            "gcc")
        set(CMAKE_CXX_COMPILER          "g++")
        set(PLATFORM other_chipset)

        add_definitions(-DOUTDOOR_TYPE)

else()

        message(FATAL_ERROR "You do not choose toolchain")

endif()

#--------------------------------------------------------------------------
#Libraries
set(LIBS_ROOT_DIR               ${CMAKE_SOURCE_DIR}/libs)
set(LIBS_RPI_ROOT_DIR           ${LIBS_ROOT_DIR}/meari/raspberry)
set(LIBS_X86_64_ROOT_DIR        ${LIBS_ROOT_DIR}/meari/x86_64)

if(${PLATFORM_IPC_MEARI_RPI_INDOOR})
        add_library(libsrt                  STATIC  IMPORTED)
        add_library(libatomic               STATIC  IMPORTED)
        add_library(libflv                  STATIC  IMPORTED)
        add_library(libmpeg                 STATIC  IMPORTED)
        add_library(libasound               STATIC  IMPORTED)
        add_library(libfdk-aac              STATIC  IMPORTED)
        
        set_property(TARGET libsrt          PROPERTY IMPORTED_LOCATION  "${LIBS_RPI_ROOT_DIR}/libsrt.a")
        set_property(TARGET libatomic       PROPERTY IMPORTED_LOCATION  "${LIBS_RPI_ROOT_DIR}/libatomic.a")
        set_property(TARGET libmpeg         PROPERTY IMPORTED_LOCATION  "${LIBS_RPI_ROOT_DIR}/libmpeg.a")
        set_property(TARGET libflv          PROPERTY IMPORTED_LOCATION  "${LIBS_RPI_ROOT_DIR}/libflv.a")
        set_property(TARGET libasound       PROPERTY IMPORTED_LOCATION  "${LIBS_RPI_ROOT_DIR}/libasound.a")
        set_property(TARGET libfdk-aac       PROPERTY IMPORTED_LOCATION  "${LIBS_RPI_ROOT_DIR}/libfdk-aac.a")
        link_directories(${LIBS_RPI_ROOT_DIR}/)

elseif(${PLATFORM_IPC_MEARI_OTHER_OUTDOOR})
        add_library(libsrt                  STATIC  IMPORTED)
        add_library(libflv                  STATIC  IMPORTED)
        add_library(libmpeg                 STATIC  IMPORTED)
        add_library(libmbedcrypto           STATIC  IMPORTED)
        add_library(libmbedx509             STATIC  IMPORTED)
        add_library(libmbedtls              STATIC  IMPORTED)
        
        
        set_property(TARGET libsrt          PROPERTY IMPORTED_LOCATION  "${LIBS_X86_64_ROOT_DIR}/libsrt.a")
        set_property(TARGET libflv          PROPERTY IMPORTED_LOCATION  "${LIBS_X86_64_ROOT_DIR}/libflv.a")
        set_property(TARGET libmpeg         PROPERTY IMPORTED_LOCATION  "${LIBS_X86_64_ROOT_DIR}/libmpeg.a")

        set_property(TARGET libmbedcrypto   PROPERTY IMPORTED_LOCATION  "${LIBS_X86_64_ROOT_DIR}/libmbedcrypto.a")
        set_property(TARGET libmbedx509     PROPERTY IMPORTED_LOCATION  "${LIBS_X86_64_ROOT_DIR}/libmbedx509.a")
        set_property(TARGET libmbedtls      PROPERTY IMPORTED_LOCATION  "${LIBS_X86_64_ROOT_DIR}/libmbedtls.a")
        


        link_directories(${LIBS_RPI_ROOT_DIR}/)
endif()

include_directories("../src")
include_directories("../src/srt/")
include_directories("../src/flv/")
# include_directories("../src/alsa/")
include_directories("../src/mpegts/")

#
add_executable(h264capture
        src/main.cpp
        src/h264_capture.cpp src/h264_capture.h
        src/pcm_capture.cpp src/pcm_capture.h
        src/aac_encode.cpp src/aac_encode.h
        src/media_stream.h
        src/srt_proto.cpp src/srt_proto.h
        src/media_queue.cpp src/media_queue.h
        src/tcp_control.cpp src/tcp_control.h
        src/ptz_control.cpp src/ptz_control.h

)


if(${PLATFORM_IPC_MEARI_RPI_INDOOR})
target_link_libraries(h264capture
        libasound
        libsrt
        libatomic
        pthread
        libflv
        libmpeg
        libfdk-aac
        dl
)


elseif(${PLATFORM_IPC_MEARI_OTHER_OUTDOOR})
target_link_libraries(h264capture
        libsrt
        crypto
        libflv
        libmpeg
        libmbedcrypto
        libmbedx509
        libmbedtls
        asound
        fdk-aac
)


endif()

target_include_directories(h264capture PRIVATE
        src)

# target_include_directories(srt_demo PRIVATE
#         src)