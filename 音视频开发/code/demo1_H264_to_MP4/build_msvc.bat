@echo off
setlocal

if not exist build mkdir build
cl /nologo /W4 /TC /Iinclude src\h264_mp4_muxer.c examples\h264_to_mp4.c /Febuild\h264_to_mp4.exe
if errorlevel 1 exit /b 1

echo Built build\h264_to_mp4.exe
