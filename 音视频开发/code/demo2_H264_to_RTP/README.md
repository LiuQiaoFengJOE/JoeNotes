# H264 to RTP/RTCP C Demo

这是一个面向新手的 C 语言 demo，用来学习：

- H264 Annex-B 裸流是什么
- RTP 如何把 H264 NALU 封装成网络包
- RTCP 如何做统计反馈和简单握手
- 接收端如何把 RTP 再还原成 `.h264`

工程里有两个程序：

```text
h264_rtp_sender.exe    发送端：H264 Annex-B -> RTP/RTCP -> UDP
h264_rtp_receiver.exe  接收端：UDP -> RTP/RTCP -> H264 Annex-B
```

## 目录

```text
include/               头文件
src/                   C 源码
docs/rtp_rtcp_notes.md RTP/RTCP 学习笔记
scripts/               Windows 构建脚本
```

## 先看什么

建议按这个顺序读代码：

1. `src/main.c`
2. `src/h264_annexb.c`
3. `src/rtp_h264.c`
4. `src/receiver.c`
5. `src/rtcp.c`
6. `src/platform_net.c`

## 编译

### MinGW

```bat
scripts\build_mingw.bat
```

### Visual Studio

先打开 `x64 Native Tools Command Prompt for VS`，再执行：

```bat
scripts\build_msvc.bat
```

### CMake

```bat
cmake -S . -B build
cmake --build build
```

## 运行

最推荐的学习方式是开两个命令行窗口。

窗口 A 先启动接收端：

```bat
cd build\mingw\receiver_app
h264_rtp_receiver.exe
```

窗口 B 再启动发送端：

```bat
cd build\mingw\sender_app
h264_rtp_sender.exe
```

交互时直接回车可以使用默认值。

## 默认端口

- RTP: `5004`
- RTCP: `5005`
- 发送端本地 RTCP: `5007`

## 你会看到什么

- 发送端会把 `output.h264` 读出来
- 发送端把每个 NALU 封装成 RTP
- 发送端周期性发 RTCP SR
- 接收端收到 SR 后回 RTCP RR
- 接收端把收到的 RTP 重组回 `received_output.h264`

## 学习重点

- RTP 负责“怎么分包、怎么编号、怎么加时间戳”
- RTCP 负责“怎么反馈统计信息”
- H264 Annex-B 负责“怎么从裸流里找出一个个 NALU”

## 说明

这个 demo 故意省掉了很多工业级复杂度，比如：

- 没有 RTSP/SDP
- 没有完整乱序重排
- 没有 jitter buffer
- 没有完整的丢包恢复

但核心的 RTP/RTCP/H264 封包路径都在。
