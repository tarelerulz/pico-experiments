cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(picollama260k C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(picollama260k picollama.c)

target_include_directories(picollama260k PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# talk over the USB CDC serial port, not the UART pins
pico_enable_stdio_usb(picollama260k 1)
pico_enable_stdio_uart(picollama260k 0)

target_link_libraries(picollama260k pico_stdlib)

# produce .uf2 (drag-and-drop), plus .elf/.bin/.map
pico_add_extra_outputs(picollama260k)
