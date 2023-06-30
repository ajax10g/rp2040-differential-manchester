cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(rp2040_hid_diff_manchester)
# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(${CMAKE_PROJECT_NAME})

pico_generate_pio_header(${CMAKE_PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/src/differential_manchester.pio)
target_sources(${CMAKE_PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/src/main.c
        ${CMAKE_CURRENT_LIST_DIR}/src/usb_descriptors.c
        ${CMAKE_CURRENT_LIST_DIR}/src/differential_manchester.c
        )

# Make sure TinyUSB can find tusb_config.h
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
        $ENV{PICO_SDK_PATH}/lib/tinyusb/hw
        $ENV{PICO_SDK_PATH}/src/rp2_common/hardware_pio/include
        $ENV{PICO_SDK_PATH}/src/rp2_common/pico_multicore/include
        $ENV{PICO_SDK_PATH}/src/rp2_common/hardware_dma/include
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/src
        )

# In addition to pico_stdlib required for common PicoSDK functionality, add dependency on tinyusb_device
# for TinyUSB device support and tinyusb_board for the additional board support library used by the example
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC pico_stdlib pico_unique_id tinyusb_device tinyusb_board hardware_pio pico_multicore hardware_dma)

# Uncomment this line to enable fix for Errata RP2040-E5 (the fix requires use of GPIO 15)
#target_compile_definitions(dev_hid_composite PUBLIC PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1)

pico_add_extra_outputs(${CMAKE_PROJECT_NAME})

add_compile_definitions(CFG_BOARD_UART_BAUDRATE=115200)
add_compile_definitions(CFG_TUSB_OS=OPT_OS_PICO)
add_compile_definitions(PICO_STDIO_UART=1)
add_compile_definitions(PICO_STDIO_USB=0)

## add url via pico_set_program_url
#example_auto_set_url(${CMAKE_PROJECT_NAME})
