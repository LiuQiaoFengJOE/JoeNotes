# C ISP Pipeline Demo

This is a pure C version of the tiny ISP walkthrough.

## What it does

The executable does two things by default:

1. generate an exaggerated Bayer raw input:
   - `raw_samples/synthetic_sensor_extreme_rggb8_640x480.raw`
2. run a small software ISP on it and export all stages into:
   - `raw_samples/isp_demo_out_c_extreme`
3. generate preview PNGs for all raw stages automatically

The exaggerated input is intentionally ugly so the stage-to-stage differences are easier to see.

## Build

```bash
gcc -std=c99 -O2 -Wall -Wextra raw_samples/simple_isp_demo.c -lm -o raw_samples/simple_isp_demo.exe
```

## Run

```bash
raw_samples/simple_isp_demo.exe
```

If you already have a Bayer input and only want to run the ISP:

```bash
raw_samples/simple_isp_demo.exe --skip-generate --input-bayer raw_samples/synthetic_sensor_extreme_rggb8_640x480.raw --output-dir raw_samples/isp_demo_out_c_extreme
```

## Tuning knobs

The C version now supports these parameters:

```bash
./raw_samples/simple_isp_demo.exe --help
```

- `--base-rgb`
  - choose which RGB24 source image is used when synthesizing a Bayer raw
- `--generated-bayer`
  - choose where the synthesized Bayer raw file is written
- `--artifact-strength`
  - controls how ugly the generated fake sensor raw is
  - smaller: milder raw
  - larger: stronger color cast and shading issues
- `--black-level`
  - controls the sensor offset and the subtraction stage
- `--wb-r --wb-g --wb-b`
  - controls white-balance gains
- `--lsc-strength`
  - controls lens shading correction strength
- `--gamma`
  - controls the final display mapping
- `--ccm-strength`
  - blends between neutral RGB and the fixed color matrix
- `--saturation`
  - controls how strong the colors feel after CCM
- `--no-previews`
  - skip automatic PNG preview generation

## Example presets

Mild effect:

```bash
./raw_samples/simple_isp_demo.exe --artifact-strength 0.35 --black-level 32 --wb-r 1.8 --wb-g 1.0 --wb-b 0.9 --lsc-strength 0.45 --gamma 2.2 --output-dir raw_samples/isp_demo_out_c_mild
```

Very obvious effect:

```bash
./raw_samples/simple_isp_demo.exe --artifact-strength 1.6 --black-level 80 --wb-r 4.0 --wb-g 1.5 --wb-b 0.65 --lsc-strength 1.9 --gamma 2.0 --output-dir raw_samples/isp_demo_out_c_strong
```

Only retune the ISP on an existing Bayer raw:

```bash
./raw_samples/simple_isp_demo.exe --skip-generate --input-bayer raw_samples/synthetic_sensor_extreme_rggb8_640x480.raw --wb-r 4.0 --wb-g 1.4 --wb-b 0.7 --lsc-strength 2.0 --output-dir raw_samples/isp_demo_out_c_reprocess
```

## Stage files

- `00_sensor_mosaic_rggb8.raw`
- `01_black_level_gray8.raw`
- `02_lsc_wb_gray8.raw`
- `03_demosaic_linear_rgb24.raw`
- `04_display_rgb24.raw`
- `05_display_yuv420p.raw`

Matching preview PNGs are generated automatically:

- `00_sensor_preview.png`
- `01_black_level_preview.png`
- `02_lsc_wb_preview.png`
- `03_demosaic_linear_preview.png`
- `04_display_preview.png`
- `05_display_yuv420p_preview.png`

## View commands

```bash
ffplay -f rawvideo -pixel_format gray -video_size 640x480 raw_samples/isp_demo_out_c_extreme/00_sensor_mosaic_rggb8.raw
ffplay -f rawvideo -pixel_format gray -video_size 640x480 raw_samples/isp_demo_out_c_extreme/01_black_level_gray8.raw
ffplay -f rawvideo -pixel_format gray -video_size 640x480 raw_samples/isp_demo_out_c_extreme/02_lsc_wb_gray8.raw
ffplay -f rawvideo -pixel_format rgb24 -video_size 640x480 raw_samples/isp_demo_out_c_extreme/03_demosaic_linear_rgb24.raw
ffplay -f rawvideo -pixel_format rgb24 -video_size 640x480 raw_samples/isp_demo_out_c_extreme/04_display_rgb24.raw
ffplay -f rawvideo -pixel_format yuv420p -video_size 640x480 raw_samples/isp_demo_out_c_extreme/05_display_yuv420p.raw
```

## Quick PNG dump

In normal use you no longer need to run these manually, because the program now generates them automatically. They are still useful as reference commands:

```bash
ffmpeg -y -f rawvideo -pixel_format gray -video_size 640x480 -i raw_samples/isp_demo_out_c_extreme/00_sensor_mosaic_rggb8.raw -frames:v 1 raw_samples/isp_demo_out_c_extreme/00_sensor_preview.png
ffmpeg -y -f rawvideo -pixel_format gray -video_size 640x480 -i raw_samples/isp_demo_out_c_extreme/01_black_level_gray8.raw -frames:v 1 raw_samples/isp_demo_out_c_extreme/01_black_level_preview.png
ffmpeg -y -f rawvideo -pixel_format gray -video_size 640x480 -i raw_samples/isp_demo_out_c_extreme/02_lsc_wb_gray8.raw -frames:v 1 raw_samples/isp_demo_out_c_extreme/02_lsc_wb_preview.png
ffmpeg -y -f rawvideo -pixel_format rgb24 -video_size 640x480 -i raw_samples/isp_demo_out_c_extreme/03_demosaic_linear_rgb24.raw -frames:v 1 raw_samples/isp_demo_out_c_extreme/03_demosaic_linear_preview.png
ffmpeg -y -f rawvideo -pixel_format rgb24 -video_size 640x480 -i raw_samples/isp_demo_out_c_extreme/04_display_rgb24.raw -frames:v 1 raw_samples/isp_demo_out_c_extreme/04_display_preview.png
ffmpeg -y -f rawvideo -pixel_format yuv420p -video_size 640x480 -i raw_samples/isp_demo_out_c_extreme/05_display_yuv420p.raw -frames:v 1 raw_samples/isp_demo_out_c_extreme/05_display_yuv420p_preview.png
```
