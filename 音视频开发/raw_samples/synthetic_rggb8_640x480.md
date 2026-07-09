# raw_samples

Two headerless raw binaries are provided in this folder.

For a step-by-step ISP demo built on top of Bayer raw, see `isp_pipeline_demo.md`.

## 1) Recommended FFmpeg first run

- File: `synthetic_rgb24_640x480.raw`
- Format: headerless RGB raw stream
- Pixel format for FFmpeg: `rgb24`
- Resolution: `640x480`
- Bytes: `921600`

Preview as PNG:

```bash
ffmpeg -f rawvideo -pixel_format rgb24 -video_size 640x480 -i raw_samples/synthetic_rgb24_640x480.raw -frames:v 1 raw_samples/synthetic_rgb24_640x480_preview.png
```

Wrap into MP4:

```bash
ffmpeg -f rawvideo -pixel_format rgb24 -video_size 640x480 -framerate 1 -i raw_samples/synthetic_rgb24_640x480.raw -vf format=yuv420p -c:v libx264 -crf 18 raw_samples/synthetic_rgb24_640x480.mp4
```

Play directly:

```bash
ffplay -f rawvideo -pixel_format rgb24 -video_size 640x480 raw_samples/synthetic_rgb24_640x480.raw
```

## 2) Planar YUV sample

- File: `synthetic_yuv420p_640x480.raw`
- Format: headerless YUV raw stream
- Pixel format for FFmpeg: `yuv420p`
- Resolution: `640x480`
- Bytes: `460800`

Preview as PNG:

```bash
ffmpeg -f rawvideo -pixel_format yuv420p -video_size 640x480 -i raw_samples/synthetic_yuv420p_640x480.raw -frames:v 1 raw_samples/synthetic_yuv420p_640x480_preview.png
```

Wrap into MP4:

```bash
ffmpeg -f rawvideo -pixel_format yuv420p -video_size 640x480 -framerate 1 -i raw_samples/synthetic_yuv420p_640x480.raw -c:v libx264 -crf 18 raw_samples/synthetic_yuv420p_640x480.mp4
```

Play directly:

```bash
ffplay -f rawvideo -pixel_format yuv420p -video_size 640x480 raw_samples/synthetic_yuv420p_640x480.raw
```

## 3) Sensor-style Bayer raw

- File: `synthetic_rggb8_640x480.raw`
- Format: headerless Bayer raw stream
- Bayer pattern: `RGGB`
- Pixel format for FFmpeg: `bayer_rggb8`
- Resolution: `640x480`
- Bytes: `307200`

Probe as raw input:

```bash
ffmpeg -f rawvideo -pixel_format bayer_rggb8 -video_size 640x480 -i raw_samples/synthetic_rggb8_640x480.raw -frames:v 1 raw_samples/synthetic_rggb8_640x480_preview.png
```

Note:

Some FFmpeg builds can read Bayer raw but do not include a debayer filter, so the preview may still look like a sensor mosaic. This file is useful when you want the byte stream to feel closer to a camera raw dump.
