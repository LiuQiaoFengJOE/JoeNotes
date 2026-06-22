# H264 to RTP/RTCP C Demo

这是一个面向新手学习的 C 语言 demo：读取 `output.h264` 这类 H264 Annex-B 裸流文件，把每个 NALU 按 RFC 6184 封装成 RTP 包，通过 UDP 发送出去，并周期性发送 RTCP Sender Report。

现在工程包含两个程序：

```text
h264_rtp_sender.exe    发送端：H264 Annex-B -> RTP/RTCP -> UDP
h264_rtp_receiver.exe  接收端：UDP -> RTP/RTCP -> H264 Annex-B
```

代码目标：

- 可以在 Windows 上编译、运行、调试。
- RTP/H264 封包逻辑尽量独立，方便移植到小内存平台。
- 使用固定缓冲区，默认不把整个视频文件读入内存。
- 注释写得比较细，便于理解 IPC 摄像头常见的 RTP/RTCP 发送流程。

## 目录结构

```text
include/                 头文件
src/                     C 源码
docs/rtp_rtcp_notes.md   RTP/RTCP 学习笔记
scripts/                 Windows 构建脚本
output.h264              示例输入文件，保留你的原文件
```

## Windows 编译

### 方式 1：Visual Studio cl

打开 “x64 Native Tools Command Prompt for VS”，进入本目录：

```bat
scripts\build_msvc.bat
```

生成：

```text
build\msvc\sender_app\h264_rtp_sender.exe
build\msvc\receiver_app\h264_rtp_receiver.exe
```

### 方式 2：MinGW gcc

如果已经安装 MinGW 并且 `gcc` 在 PATH 中：

```bat
scripts\build_mingw.bat
```

生成：

```text
build\mingw\sender_app\h264_rtp_sender.exe
build\mingw\receiver_app\h264_rtp_receiver.exe
```

## 运行

最推荐的新手实验：开两个命令行窗口。

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

更详细步骤见 [RUN_TWO_PROGRAMS.md](RUN_TWO_PROGRAMS.md)。

发送端参数含义：

```text
参数1: H264 Annex-B 裸流文件
参数2: 目标 IP
参数3: RTP UDP 端口
参数4: 对方 RTCP UDP 端口
参数5: 帧率 fps
参数6: 本地 RTCP UDP 端口，用于接收 Receiver Report
```

## 用 Wireshark 观察

1. 启动 Wireshark，监听 Loopback Adapter 或对应网卡。
2. 过滤：

```text
udp.port == 5004 || udp.port == 5005
```

3. 如果 Wireshark 没自动识别 RTP，可以右键 UDP 包，选择 “Decode As...”，把 5004 解码为 RTP。

## 学习路线

建议按这个顺序看代码：

1. [src/main.c](src/main.c)：整体流程。
2. [src/h264_annexb.c](src/h264_annexb.c)：从 H264 裸流里找 NALU。
3. [src/rtp_h264.c](src/rtp_h264.c)：H264 NALU 到 RTP 包，重点看 Single NAL 和 FU-A。
4. [src/receiver.c](src/receiver.c)：RTP 接收、H264 FU-A 重组、写回 `.h264` 文件。
5. [src/rtcp.c](src/rtcp.c)：RTCP Sender Report 和 Receiver Report。
6. [src/platform_net.c](src/platform_net.c)：Windows UDP 封装，移植时优先替换它。

更详细的协议解释见 [docs/rtp_rtcp_notes.md](docs/rtp_rtcp_notes.md)。
