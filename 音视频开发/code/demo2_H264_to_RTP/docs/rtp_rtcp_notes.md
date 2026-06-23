# RTP / RTCP 学习笔记

这份笔记配合代码一起看。

## 1. 为什么要用 RTP

H264 码流不是一帧一帧“天然适合发 UDP”的数据。
它在网络上传输时通常要解决几个问题：

- 怎么分包
- 怎么让接收端知道顺序
- 怎么表示时间
- 怎么告诉对方自己发了多少、收了多少

RTP 负责前 3 件事，RTCP 负责第 4 件事。

## 2. H264 Annex-B

本 demo 的输入是 Annex-B 格式的 `.h264` 文件。

起始码通常是：

```text
00 00 01
00 00 00 01
```

每个 NALU 的第一个字节是 NAL header：

```text
F | NRI | Type
```

常见 Type：

- 1: 非 IDR slice
- 5: IDR slice
- 6: SEI
- 7: SPS
- 8: PPS
- 9: AUD

## 3. RTP 固定头

RTP 固定头 12 字节：

```text
V P X CC | M PT
sequence number
timestamp
SSRC
```

字段含义：

- V: 版本，固定 2
- M: marker bit
- PT: payload type
- sequence number: 每个 RTP 包加 1
- timestamp: 同一帧的 RTP 包通常相同
- SSRC: 同步源 ID

视频常用时钟是 `90000Hz`。

如果是 `25fps`：

```text
timestamp_step = 90000 / 25 = 3600
```

## 4. H264 over RTP

### 4.1 Single NAL Unit

如果一个 NALU 足够小，可以直接放进一个 RTP 包：

```text
RTP header + 完整 NALU
```

### 4.2 FU-A

如果 NALU 太大，就拆成多个 FU-A 分片。

原始 NAL header：

```text
F | NRI | Type
```

FU-A 前两个字节：

```text
FU indicator: F | NRI | 28
FU header:    S | E | R | Type
```

- S=1: 第一个分片
- E=1: 最后一个分片

## 5. RTCP SR

Sender Report，包类型 `200`。

它告诉接收端：

- 我是谁
- 现在的 NTP 时间是多少
- 对应的 RTP timestamp 是多少
- 我已经发了多少 RTP 包
- 我已经发了多少 payload 字节

## 6. RTCP RR

Receiver Report，包类型 `201`。

它告诉发送端：

- 我是谁
- 我在汇报哪个 sender
- 我收到了哪些序号
- 我大概丢了多少包
- 我的 jitter 估计是多少

## 7. 接收端怎么还原 H264

流程是：

```text
UDP -> RTP 解析 -> H264 RTP payload -> H264 NALU -> Annex-B 文件
```

### Single NAL

直接在前面补起始码：

```text
00 00 00 01 + NALU
```

### FU-A

收到第一个分片时：

1. 写入起始码
2. 恢复原始 NAL header
3. 写入第一个分片数据

收到中间分片时：

1. 继续追加数据

收到最后一个分片时：

1. 追加最后一段数据
2. 一个完整 NALU 结束

## 8. 这个 demo 的简化点

- 没有 RTSP
- 没有 SDP
- 没有复杂乱序恢复
- 没有完整 jitter buffer
- 没有完整 RTT 计算

这些简化是为了让你先把 RTP/RTCP 的核心链路看懂。
