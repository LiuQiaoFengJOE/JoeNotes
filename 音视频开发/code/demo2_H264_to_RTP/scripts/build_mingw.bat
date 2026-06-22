@echo off
setlocal

if not exist build\mingw mkdir build\mingw
if not exist build\mingw\sender_app mkdir build\mingw\sender_app
if not exist build\mingw\receiver_app mkdir build\mingw\receiver_app

rem Remove old flat-layout binaries from earlier versions of this demo.
if exist build\mingw\h264_rtp_sender.exe del /Q build\mingw\h264_rtp_sender.exe
if exist build\mingw\h264_rtp_receiver.exe del /Q build\mingw\h264_rtp_receiver.exe
if exist build\mingw\received_output.h264 del /Q build\mingw\received_output.h264

gcc -std=c99 -Wall -Wextra -Iinclude ^
  src\main.c ^
  src\console_helper.c ^
  src\h264_annexb.c ^
  src\platform_net.c ^
  src\rtcp.c ^
  src\rtp_h264.c ^
  -lws2_32 ^
  -o build\mingw\sender_app\h264_rtp_sender.exe

if errorlevel 1 (
  echo Sender build failed.
  exit /b 1
)

gcc -std=c99 -Wall -Wextra -Iinclude ^
  src\receiver.c ^
  src\console_helper.c ^
  src\platform_net.c ^
  src\rtcp.c ^
  -lws2_32 ^
  -o build\mingw\receiver_app\h264_rtp_receiver.exe

if errorlevel 1 (
  echo Receiver build failed.
  exit /b 1
)

echo Build OK:
echo   build\mingw\sender_app\h264_rtp_sender.exe
echo   build\mingw\receiver_app\h264_rtp_receiver.exe

if exist output.h264 (
  copy /Y output.h264 build\mingw\sender_app\output.h264 >nul
  echo Copied sample input:
  echo   build\mingw\sender_app\output.h264
)
