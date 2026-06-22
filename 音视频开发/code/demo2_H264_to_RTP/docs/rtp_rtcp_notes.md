# IPC 摄像头里的 H264 RTP/RTCP 学习笔记

这份笔记配合本 demo 的源码阅读。

## 1. IPC 为什么要用 RTP

IPC 摄像头编码出来的 H264 是一串 NALU，例如 SPS、PPS、IDR、P slice。

网络发送时，不能直接把一个巨大 H264 帧随便塞进 UDP：

- UDP 包太大会导致 IP 分片，丢一个分片整包就坏。
- 接收端需要知道序号、时间戳、负载类型。
- 播放器需要根据时间戳恢复播放节奏。

所以常见做法是：

```text
H264 NALU -> RTP payload -> UDP -> IP -> 网络
```

RTCP 则负责辅助信息，例如发送端报告、接收端报告、时间同步、统计信息。

## 2. H264 Annex-B 裸流

`output.h264` 这类文件通常是 Annex-B 格式。它用起始码分隔 NALU：

```text
00 00 01 [NALU]
00 00 00 01 [NALU]
```

NALU 第一个字节叫 NAL header：

```text
bit:  7   6 5   4 3 2 1 0
      F   NRI   Type
```

常见 Type：

```text
1 = 非 IDR 图像切片
5 = IDR 关键帧切片
6 = SEI
7 = SPS
8 = PPS
9 = AUD
```

本 demo 的 [src/h264_annexb.c](../src/h264_annexb.c) 做的事情就是：

1. 扫描起始码 `00 00 01` 或 `00 00 00 01`。
2. 找到两个起始码之间的数据。
3. 把不带起始码的 NALU 交给 RTP 封包模块。

## 3. RTP 固定头

RTP 固定头 12 字节：

```text
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|X| CC    |M| PT          | sequence number              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| timestamp                                                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| SSRC                                                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

字段含义：

- V：版本，固定为 2。
- M：marker bit。视频里常表示一帧最后一个 RTP 包。
- PT：payload type。H264 常用动态类型 96。
- sequence number：每发一个 RTP 包加 1，用来发现丢包和乱序。
- timestamp：同一帧的 RTP 包时间戳相同，视频常用 90000Hz 时钟。
- SSRC：同步源 ID，区分不同发送源。

本 demo 中，25fps 时：

```text
timestamp_step = 90000 / 25 = 3600
```

也就是每发送一个图像 NALU，RTP timestamp 增加 3600。

## 4. H264 over RTP 两种核心封包方式

代码在 [src/rtp_h264.c](../src/rtp_h264.c)。

### 4.1 Single NAL Unit

如果一个 NALU 比 RTP payload 最大值小，可以直接放进一个 RTP 包：

```text
RTP header + 完整 H264 NALU
```

例如 SPS、PPS 通常很小，直接这样发送。

### 4.2 FU-A 分片

如果一个 NALU 太大，就要分片。RFC 6184 里常用 FU-A。

原始 NAL header：

```text
F | NRI | Type
```

FU-A payload 前两个字节：

```text
FU indicator: F | NRI | 28
FU header:    S | E | R | Type
```

S 是 start，表示第一个分片。
E 是 end，表示最后一个分片。
R 保留，写 0。

发送顺序类似：

```text
RTP(seq=1000, S=1, E=0)
RTP(seq=1001, S=0, E=0)
RTP(seq=1002, S=0, E=1, marker=1)
```

接收端看到这些 FU-A 包后，可以拼回原始 NALU。

## 5. RTCP Sender Report

RTCP Sender Report 简称 SR，包类型是 200。

它告诉接收端：

- 发送端 SSRC 是谁。
- 当前 NTP wall-clock 时间是多少。
- 对应的 RTP timestamp 是多少。
- 已经发送了多少 RTP 包。
- 已经发送了多少 RTP payload 字节。

这对音视频同步很重要。比如音频和视频 RTP timestamp 使用不同频率，但 RTCP SR 可以把它们映射到同一个 NTP 时间线上。

本 demo 每 5 秒发送一次 SR，代码在 [src/rtcp.c](../src/rtcp.c)。

## 6. RTCP Receiver Report

Receiver Report 简称 RR，包类型是 201。

它通常由接收端发给发送端，告诉发送端：

- 我是谁，也就是 receiver SSRC。
- 我正在汇报哪个发送源，也就是 sender SSRC。
- 我收到了哪个最高 RTP sequence number。
- 我估计丢了多少包。
- 当前 jitter 抖动估计是多少。

本 demo 的接收端 [src/receiver.c](../src/receiver.c) 会在收到 SR 后回 RR。

发送端和接收端的 RTCP 关系可以这样看：

```text
sender   -- RTCP SR --> receiver
sender   <-- RTCP RR -- receiver
```

注意：RTCP RR 不是“让 UDP 变可靠”。它只是反馈统计信息。
如果 UDP 包丢了，RTP/RTCP 本身不会自动重传。

## 7. 接收端如何把 RTP 还原成 H264

接收端做的事情和发送端相反：

```text
UDP packet -> RTP header -> H264 RTP payload -> H264 NALU -> Annex-B file
```

### Single NAL Unit

如果 payload type 是 1..23，说明这个 RTP 包里就是一个完整 NALU。

接收端只需要在前面补起始码：

```text
00 00 00 01 + NALU
```

然后写入 `.h264` 文件。

### FU-A

如果 payload type 是 28，说明这是一个大 NALU 的分片。

接收端要看 FU header：

```text
S=1 表示第一个分片
E=1 表示最后一个分片
```

第一个分片到来时：

1. 写入 Annex-B 起始码 `00 00 00 01`。
2. 根据 FU indicator 和 FU header 恢复原始 NAL header。
3. 写入第一个分片数据。

中间分片到来时：

```text
继续追加 fragment data
```

最后一个分片到来时：

```text
追加最后一段数据，一个完整 NALU 重组完成
```

## 8. 和真实 IPC 的关系

真实 IPC 常见链路：

```text
Sensor -> ISP -> H264 Encoder -> RTP packetizer -> UDP socket
                                      |
                                      +-> RTCP
```

本 demo 用文件代替编码器：

```text
output.h264 -> Annex-B reader -> RTP packetizer -> UDP socket
```

移植到真实设备时，最常替换的是：

- 文件读取模块：换成编码器回调。
- UDP 平台层：换成 RTOS 或芯片 SDK 的 socket/sendto。
- 时间函数：换成系统 tick 和 NTP/RTC 时间。

RTP H264 封包逻辑通常可以保留。

## 9. 这个 demo 的简化点

为了让学习曲线更平滑，本 demo 做了一些简化：

- 没有实现 RTSP，也没有生成 SDP。
- 没有解析 H264 slice header 来精确判断一帧是否有多个 slice。
- marker bit 对 VCL NALU 置 1，适合单 slice 码流学习。
- 接收端实现了基础 FU-A 重组，但没有实现完整乱序重排、丢包恢复、jitter buffer。
- RR 里的 jitter/LSR/DLSR 暂时填 0，重点先学习 SR/RR 的结构和方向。

这些简化不影响你理解 RTP/H264 的核心封包过程。
