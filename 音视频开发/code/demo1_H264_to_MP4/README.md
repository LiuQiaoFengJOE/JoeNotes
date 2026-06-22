# H.264 to MP4 Muxer

一个小型 C 语言工程接口，用于把 Annex-B 裸 H.264 文件封装成 MP4 文件。实现不依赖 FFmpeg，便于移植到 Windows、Linux 或嵌入式平台。

## 接口

头文件在 `include/h264_mp4_muxer.h`：

```c
h264_mp4_config_t cfg;
cfg.fps_num = 0;    /* 0/0 表示从 SPS VUI timing 自动读取 */
cfg.fps_den = 0;
cfg.width = 0;      /* 0 表示从 SPS 解析 */
cfg.height = 0;     /* 0 表示从 SPS 解析 */
cfg.timescale = 90000;

h264_mp4_status_t ret = h264_mp4_mux_file("input.h264", "output.mp4", &cfg);
```

封装前也可以先读取 H.264 二进制规格：

```c
h264_mp4_info_t info;
h264_mp4_status_t ret = h264_mp4_probe_file("input.h264", &info);
if (ret == H264_MP4_OK) {
    printf("%ux%u fps=%u/%u frames=%u\n",
           info.width, info.height, info.fps_num, info.fps_den,
           info.frame_count);
}
```

## Windows 构建

使用 Visual Studio Developer Command Prompt：

```bat
build_msvc.bat
build\h264_to_mp4.exe output.h264 output.mp4
```

如果本机安装了 CMake：

```bat
cmake -S . -B build
cmake --build build --config Release
build\Release\h264_to_mp4.exe output.h264 output.mp4
```

## 说明

- 输入文件需要是 Annex-B H.264 码流，即 NALU 前带 `00 00 01` 或 `00 00 00 01` 起始码。
- 输入码流必须包含 SPS 和 PPS。
- 默认会先从 SPS VUI timing 读取原始帧率，避免把 60fps 等码流封装成慢动作。
- 如果码流没有 VUI timing，请显式传入帧率，例如 `60` 或 `30000/1001`。
- MP4 使用 32 位 chunk offset，适合常规录像文件；超大文件建议扩展为 `co64`。
