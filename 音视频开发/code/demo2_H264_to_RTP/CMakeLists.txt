cmake_minimum_required(VERSION 3.15)
project(h264_to_rtp_demo C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(h264_rtp_sender
    src/main.c
    src/console_helper.c
    src/h264_annexb.c
    src/platform_net.c
    src/rtcp.c
    src/rtp_h264.c
)

target_include_directories(h264_rtp_sender PRIVATE include)

add_executable(h264_rtp_receiver
    src/receiver.c
    src/console_helper.c
    src/platform_net.c
    src/rtcp.c
)

target_include_directories(h264_rtp_receiver PRIVATE include)

if (WIN32)
    target_link_libraries(h264_rtp_sender PRIVATE ws2_32)
    target_link_libraries(h264_rtp_receiver PRIVATE ws2_32)
endif()
