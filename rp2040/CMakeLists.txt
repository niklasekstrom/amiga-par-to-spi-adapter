cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(par_spi C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(par_spi par_spi.c)

pico_add_extra_outputs(par_spi)

target_link_libraries(par_spi pico_stdlib hardware_spi)
