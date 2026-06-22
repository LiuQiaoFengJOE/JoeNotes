@echo off
setlocal

rem 本脚本用于快速验证两个程序是否能在本机跑通。
rem 新手学习时更推荐手动开两个命令行窗口运行，这样能分别观察日志。

if not exist build\mingw\sender_app\h264_rtp_sender.exe (
  call scripts\build_mingw.bat
  if errorlevel 1 exit /b 1
)

if not exist build\mingw\receiver_app\h264_rtp_receiver.exe (
  call scripts\build_mingw.bat
  if errorlevel 1 exit /b 1
)

echo Starting receiver...
start "RTP Receiver" /D build\mingw\receiver_app h264_rtp_receiver.exe received_output.h264 5004 5005 15

timeout /t 1 /nobreak >nul

echo Starting sender...
pushd build\mingw\sender_app
h264_rtp_sender.exe output.h264 127.0.0.1 5004 5005 25 5007
popd

echo.
echo Demo finished. Check build\mingw\receiver_app\received_output.h264.
