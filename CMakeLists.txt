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
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)
set(PICO_BOARD edge_logic_analyzer CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(multi_signal_analyzer C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

set(SOURCE_DIRS ${CMAKE_CURRENT_LIST_DIR}/src)
set(INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/includes)

# Add executable. Default name is the project name, version 0.1

file(GLOB_RECURSE SOURCE_FILES
            ${SOURCE_DIRS}/*.c
        )

add_executable(multi_signal_analyzer
            ${SOURCE_FILES}
        )

include_directories(multi_signal_analyzer
            ${INCLUDE_DIRS}
            ${CMAKE_CURRENT_LIST_DIR}/boards
        )

pico_set_program_name(multi_signal_analyzer "multi_signal_analyzer")
pico_set_program_version(multi_signal_analyzer "1.0")   

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(multi_signal_analyzer 0)

target_link_libraries(multi_signal_analyzer 
            hardware_adc
            hardware_vreg
            hardware_clocks
            hardware_timer
            pico_stdlib
            pico_multicore
            tinyusb_device
            tinyusb_board
        )

#Enables USB output
pico_enable_stdio_usb(multi_signal_analyzer 0)  
# Add the standard include files to the build
target_include_directories(multi_signal_analyzer PRIVATE
        ${INCLUDE_DIRS}
        ${SOURCE_DIRS}
        ${CMAKE_CURRENT_LIST_DIR}
)


target_link_options(multi_signal_analyzer PRIVATE
    "-Wl,-Map=${CMAKE_BINARY_DIR}/multi_signal_analyzer.map"
)

pico_add_extra_outputs(multi_signal_analyzer)

# Generate detailed firmware size report with memory usage analysis
add_custom_command(TARGET multi_signal_analyzer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Generating firmware size report..."
    COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/tools/firmware_size_report.py 
            $<TARGET_FILE:multi_signal_analyzer>
            --log ${CMAKE_BINARY_DIR}/size_history.csv
    VERBATIM
)

