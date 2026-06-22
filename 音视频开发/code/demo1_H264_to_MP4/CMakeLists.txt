cmake_minimum_required(VERSION 3.10)

project(h264_mp4_muxer C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(h264_mp4_muxer STATIC
    src/h264_mp4_muxer.c
)
target_include_directories(h264_mp4_muxer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(h264_to_mp4
    examples/h264_to_mp4.c
)
target_link_libraries(h264_to_mp4 PRIVATE h264_mp4_muxer)
