@echo off
setlocal

if not exist build\msvc mkdir build\msvc
if not exist build\msvc\sender_app mkdir build\msvc\sender_app
if not exist build\msvc\receiver_app mkdir build\msvc\receiver_app

rem Remove old flat-layout binaries from earlier versions of this demo.
if exist build\msvc\h264_rtp_sender.exe del /Q build\msvc\h264_rtp_sender.exe
if exist build\msvc\h264_rtp_receiver.exe del /Q build\msvc\h264_rtp_receiver.exe
if exist build\msvc\received_output.h264 del /Q build\msvc\received_output.h264

cl /nologo /W4 /TC /Iinclude ^
  src\main.c ^
  src\console_helper.c ^
  src\h264_annexb.c ^
  src\platform_net.c ^
  src\rtcp.c ^
  src\rtp_h264.c ^
  /Febuild\msvc\sender_app\h264_rtp_sender.exe ^
  /link Ws2_32.lib

if errorlevel 1 (
  echo Sender build failed.
  exit /b 1
)

cl /nologo /W4 /TC /Iinclude ^
  src\receiver.c ^
  src\console_helper.c ^
  src\platform_net.c ^
  src\rtcp.c ^
  /Febuild\msvc\receiver_app\h264_rtp_receiver.exe ^
  /link Ws2_32.lib

if errorlevel 1 (
  echo Receiver build failed.
  exit /b 1
)

echo Build OK:
echo   build\msvc\sender_app\h264_rtp_sender.exe
echo   build\msvc\receiver_app\h264_rtp_receiver.exe

if exist output.h264 (
  copy /Y output.h264 build\msvc\sender_app\output.h264 >nul
  echo Copied sample input:
  echo   build\msvc\sender_app\output.h264
)
