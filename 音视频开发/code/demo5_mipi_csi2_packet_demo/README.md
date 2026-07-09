# MIPI CSI-2 RAW10 Demo

这个目录保留的是“最适合理解代码和码流结构”的文件。

## 建议先看

1. `src/mipi_csi2_packet_demo.c`
2. `out_mipi_csi2_sensor_like_raw10_annotated.txt`
3. `out_mipi_csi2_sensor_like_raw10.bin`

## 目录说明

- `src/mipi_csi2_packet_demo.c`
  - 核心示例代码
  - 演示如何构造简化版 MIPI CSI-2 包流
  - 演示 RAW10 的打包、解包、多行长包组织

- `out_mipi_csi2_sensor_like_raw10.bin`
  - 更像真实 Sensor 输出的多行 RAW10 二进制码流
  - 可以直接用十六进制工具打开看

- `out_mipi_csi2_sensor_like_raw10_annotated.txt`
  - 对应二进制码流的注释版说明
  - 包含每个 packet 的偏移、header、payload、demo crc、解包后的像素值

- `CMakeLists.txt`
  - 方便重新编译这个 demo

## 这个 demo 重点在讲什么

这个 demo 重点不是还原完整 MIPI 物理层，而是帮助理解：

- MIPI CSI-2 的数据在接收端“长什么样”
- 一帧图像如何拆成：
  - `Frame Start`
  - `Line Start`
  - `RAW10 Long Packet`
  - `Line End`
  - `Frame End`
- RAW10 为什么是 `4 个 10bit 像素 -> 5 字节`
- 接收端为什么先看到的是“包流”，然后才还原成图像行

## 编译

### MinGW

```bat
gcc -std=c99 -Wall -Wextra -O2 -o mipi_csi2_packet_demo.exe src\mipi_csi2_packet_demo.c
```

### CMake

```bat
cmake -S . -B build
cmake --build build
```

## 学习建议

如果你要真正搞懂这段代码，建议按这个顺序看：

1. 看 `build_sensor_like_frame()`，理解一帧是怎么由多行组成的
2. 看 `emit_short_packet()` 和 `emit_long_packet()`，理解短包和长包
3. 看 `pack_raw10_group()` 和 `unpack_raw10_group()`，理解 RAW10 打包
4. 对照 `out_mipi_csi2_sensor_like_raw10_annotated.txt` 看每个 offset 对应什么内容

## 说明

这里的 `ECC/CRC` 是教学用简化版，不是完整协议实现。
真实硬件里还会有：

- LP/HS 状态切换
- lane 级串行发送
- 时钟恢复
- 真正的协议校验
- SoC CSI 接收器解串与协议解析
