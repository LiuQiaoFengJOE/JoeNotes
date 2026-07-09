cmake_minimum_required(VERSION 3.10)

project(mipi_csi2_packet_demo C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(mipi_csi2_packet_demo
    src/mipi_csi2_packet_demo.c
)
