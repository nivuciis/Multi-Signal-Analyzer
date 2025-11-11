# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion RISCV_ZCB_RPI_2_2_0_3)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Multi_Signal_Analyzer C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(Multi_Signal_Analyzer 
            Multi_Signal_Analyzer.c 
            led_control.c
            capture_data.c)


pico_set_program_name(Multi_Signal_Analyzer "Multi_Signal_Analyzer")
pico_set_program_version(Multi_Signal_Analyzer "1.0")   


# Generate PIO header
pico_generate_pio_header(Multi_Signal_Analyzer 
                        ${CMAKE_CURRENT_LIST_DIR}/blink.pio
                        ${CMAKE_CURRENT_LIST_DIR}/capture.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Multi_Signal_Analyzer 0)

target_link_libraries(Multi_Signal_Analyzer 
        hardware_dma
        hardware_pio
        pico_stdlib
        pico_stdio_usb
        )

#Enables USB output
pico_enable_stdio_usb(Multi_Signal_Analyzer 1)  
# Add the standard include files to the build
target_include_directories(Multi_Signal_Analyzer PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries


pico_add_extra_outputs(Multi_Signal_Analyzer)

