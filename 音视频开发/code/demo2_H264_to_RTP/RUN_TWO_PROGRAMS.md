# 两个程序跑起来

这是最适合新手的练习方式。

## 1. 编译

### MinGW

```bat
scripts\build_mingw.bat
```

### Visual Studio

```bat
scripts\build_msvc.bat
```

## 2. 先开接收端

```bat
cd build\mingw\receiver_app
h264_rtp_receiver.exe
```

直接回车就行，会使用默认参数。

接收端会先等待 sender 的 RTCP APP `HELLO`。

## 3. 再开发送端

```bat
cd build\mingw\sender_app
h264_rtp_sender.exe
```

同样可以直接回车使用默认值。

## 4. 观察输出

你应该能看到：

```text
send RTCP APP: PAIR HELLO
send RTCP SR
recv RTCP Receiver Report (RR)
RTP seq=...
```

接收端会看到：

```text
sender says HELLO
send RTCP APP: PAIR WELCOME
RTP seq=...
recv RTCP Sender Report (SR)
send RTCP Receiver Report (RR)
```

## 5. 结果文件

接收端结束后会生成：

```text
received_output.h264
```

## 6. 一句话理解

```text
sender 发 RTP 传 H264
receiver 收 RTP 还原 H264
双方再用 RTCP 交换统计信息
```
