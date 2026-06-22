# 两个程序跑起来：RTP/RTCP 本机实验

这个工程现在有两个可执行程序：

```text
h264_rtp_sender.exe    发送端：读 output.h264，封装成 RTP/RTCP 后发出去
h264_rtp_receiver.exe  接收端：监听 RTP/RTCP，重组 H264，写 received_output.h264
```

编译后它们会分别放在自己的文件夹里，模拟两台设备各有自己的文件目录：

```text
build\mingw\sender_app\
  h264_rtp_sender.exe
  output.h264

build\mingw\receiver_app\
  h264_rtp_receiver.exe
  received_output.h264    运行后生成
```

## 1. 先编译

如果你用 MinGW：

```bat
scripts\build_mingw.bat
```

如果你用 Visual Studio 的 x64 Native Tools Command Prompt：

```bat
scripts\build_msvc.bat
```

## 2. 开两个命令行窗口

你也可以直接双击 exe。双击时程序会进入交互式终端，不会一闪而过。

学习时推荐用命令行打开，这样当前工作目录就是 exe 所在文件夹，文件也会保存到正确的位置。

### 窗口 A：先启动接收端

```bat
cd build\mingw\receiver_app
h264_rtp_receiver.exe
```

它会一步一步询问：

```text
Output H264 file in receiver folder [received_output.h264]:
Receiver local RTP port [5004]:
Receiver local RTCP port [5005]:
Receive seconds [20]:
```

直接按回车就是使用默认值。

然后接收端会进入：

```text
STEP 3: Wait for sender pairing
still waiting for sender HELLO...
```

### 窗口 B：再启动发送端

```bat
cd build\mingw\sender_app
h264_rtp_sender.exe
```

它会一步一步询问：

```text
Input H264 file in sender folder [output.h264]:
Receiver IP address [127.0.0.1]:
Receiver RTP port [5004]:
Receiver RTCP port [5005]:
Sender local RTCP port [5007]:
Simulated video fps [25]:
```

直接按回车就是使用默认值。

之后你会看到类似流程：

```text
STEP 3: Pair with receiver by RTCP APP
send RTCP APP: PAIR HELLO
recv RTCP APP name=PAIR text=WELCOME ...
paired OK

STEP 4: Send H264 as RTP packets
NAL 1: type=7 SPS ...
RTCP SR sent ...
recv RTCP Receiver Report (RR)
```

## 3. 你应该看到什么

接收端窗口会持续打印类似：

```text
recv RTCP APP name=PAIR text=HELLO ...
send RTCP APP: PAIR WELCOME
RTP seq= 1000 ts=         0 M=0 -> Single NAL type= 7 size=...
RTP seq= 1001 ts=         0 M=0 -> Single NAL type= 8 size=...
RTP seq= 1002 ts=         0 M=1 -> FU-A type= 5 S=1 E=0 fragment=...
recv RTCP Sender Report (SR), size=28
send RTCP Receiver Report (RR): received=... highest_seq=...
```

发送端窗口会打印：

```text
send RTCP APP: PAIR HELLO
recv RTCP APP name=PAIR text=WELCOME ...
NAL      1: type= 7 SPS            size=...
RTCP SR sent: packets=... octets=...
recv RTCP Receiver Report (RR), size=32
```

这说明：

```text
发送端 RTP  ---> 接收端
发送端 RTCP SR ---> 接收端
接收端 RTCP RR ---> 发送端
```

链路已经闭环。

## 3.1 一键本机测试

如果你只是想先确认程序能跑，可以执行：

```bat
scripts\run_local_demo.bat
```

它会自动打开接收端窗口，然后启动发送端。
不过学习协议时，还是建议按上面的方式手动开两个窗口，方便观察每边日志。

## 4. 验证输出文件

接收结束后，会生成：

```text
received_output.h264
```

它是接收端从 RTP 里重组出来的 H264 Annex-B 文件。
如果你按本文的文件夹方式运行，它的位置是：

```text
build\mingw\receiver_app\received_output.h264
```

如果电脑上有 ffplay，可以试：

```bat
ffplay received_output.h264
```

如果没有 ffplay，也可以只用文件大小和控制台日志确认 RTP/FU-A 重组过程。

## 5. RTP/RTCP 不是 TCP 连接

新手容易误会“连接后传输”。RTP 常见承载方式是 UDP：

```text
RTP 负责媒体包格式、序号、时间戳
RTCP 负责统计、同步、反馈
UDP 负责把一个个数据报发到 IP:port
```

UDP 没有真正的 connect/accept 握手。

这个 demo 里的“连接”可以理解为：

```text
发送端知道接收端 IP/端口
接收端监听固定端口
双方按 RTP/RTCP 格式互相发 UDP 包
```

真实 IPC 摄像头通常还会有 RTSP：

```text
RTSP 负责控制：DESCRIBE、SETUP、PLAY、TEARDOWN
RTP/RTCP 负责真正传输音视频
```

本工程先专注 RTP/RTCP，避免 RTSP 把学习路线变复杂。
